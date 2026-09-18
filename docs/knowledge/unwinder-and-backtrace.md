# 回栈与 Unwinder 机制

## 概述

本文档描述 faultloggerd 的回栈（unwind）核心实现，包括本地回栈、远程回栈、混合栈（JS/Native）、FP 回栈与 DWARF 回栈，以及 Backtrace 和 Unwinder 两个关键模块的关系。

## 关键文件

| 模块 | 路径 | 职责 |
| --- | --- | --- |
| Unwinder | `interfaces/innerkits/unwinder/` | 核心回栈，支持 FP/DWARF/ptrace 回栈 |
| Backtrace | `interfaces/innerkits/backtrace/` | 封装 Unwinder，提供简化的本地回栈 API |
| DumpCatcher | `interfaces/innerkits/dump_catcher/` | 封装本地+远程抓栈，面向应用 |
| UnwindContext | `interfaces/innerkits/unwinder/include/unwind_context.h` | 回栈上下文抽象 |
| DfxAccessors | `interfaces/innerkits/unwinder/include/dfx_accessors.h` | 内存/寄存器访问接口抽象 |
| DfxMaps | `interfaces/innerkits/unwinder/include/dfx_maps.h` | 虚拟内存映射管理 |
| DfxSymbols | `interfaces/innerkits/unwinder/include/dfx_symbols.h` | 符号解析 |

## Unwinder 架构

### 类设计

`Unwinder` 类（`unwinder.h`）是回栈的统一入口，提供三种构造方式：

```cpp
// 本地回栈
Unwinder(bool needMaps = true);
// 远程回栈（ptrace 方式，用于 ProcessDump 抓取其他进程）
Unwinder(int pid, bool crash = true, std::shared_ptr<DfxMaps> maps = nullptr);
// 自定义回栈（用于hiperf、hiprofiler进行回栈）
Unwinder(std::shared_ptr<UnwindAccessors> accessors, bool local = false);
```

### 回栈方法

| 方法 | 场景 | 说明 |
| --- | --- | --- |
| `UnwindLocalWithContext` | 本地，有 ucontext | 使用信号上下文进行本地回栈 |
| `UnwindLocalWithTid` | 本地，指定 tid | 暂停目标线程后回栈 |
| `UnwindLocalByOtherTid` | 本地，其他线程 | 当前线程回栈其他线程 |
| `UnwindLocal` | 本地，当前线程 | 当前线程回栈 |
| `UnwindRemote` | 远程 | ptrace attach 目标进程线程后回栈 |
| `Unwind` | 通用 | 通过 ctx 上下文回栈 |
| `UnwindByFp` | FP 回栈 | 仅用帧指针回栈，速度快但不完整 |
| `Step` | 单步回栈 | 回溯一帧，返回 pc/sp |

### 配置开关

| 方法 | 控制 |
| --- | --- |
| `EnableUnwindCache` | 是否缓存 ELF 解析结果 |
| `EnableFpCheckMapExec` | FP 回栈时检查映射可执行权限 |
| `EnableFillFrames` | 是否填充帧列表（不填充时只计算帧数） |
| `EnableParseNativeSymbol` | 是否解析 native 符号（函数名/偏移） |
| `EnableJsvmstack` | 是否解析 JSVM 混合栈 |
| `IgnoreMixstack` | 是否忽略混合栈解析 |

## 回栈方式

### FP 回栈（Frame Pointer）

- 实现在 `fp_backtrace.cpp` / `fp_unwinder.h`
- 通过帧指针寄存器（ARM: fp/r11, ARM64: x29）遍历栈帧
- 速度快，但要求编译时未省略帧指针（`-fno-omit-frame-pointer`）
- 适用于快速本地回栈场景

### DWARF 回栈

- 通过 ELF 的 `.eh_frame` / `.ARM.exidx` 段解析
- 支持 `dwarf_cfa_instructions.h`、`exidx_entry_parser.h`、`unwind_entry_parser.h`
- 适用于需要完整栈的场景，不依赖帧指针
- ProcessDump 崩溃抓栈默认使用 DWARF 回栈

### 混合栈（JS/Native）

- 解析 ArkTS/JSVM 的 JS 栈帧，与 native 栈交替输出
- 实现在 `unwinder/src/ark/` 和 `unwinder/src/jsvm/`
- `DfxArk` / `DfxJsvm` 负责获取 JS 运行时的栈信息
- JSVM 混合栈通过 `EnableJsvmstack` 开关启用

## Unwinder 源码结构

```
interfaces/innerkits/unwinder/src/
├── ark/                  # ArkTS 混合栈解析
├── elf/                  # ELF 解析（DWARF/eh_frame/ARM.exidx）
├── jsvm/                 # JSVM 混合栈解析
├── maps/                 # 内存映射管理
├── memory/               # 内存读取实现
├── registers/            # 寄存器操作（各架构）
├── unwind_entry_parser/  # unwind table 解析，arm 32位系统用exidx, arm 64位系统用dwarf
├── unwind_local/         # 本地回栈实现
└── utils/                # 工具函数
```

### 架构支持

寄存器定义在各 `unwind_*_define.h`：
- `unwind_arm64_define.h` — ARM64
- `unwind_arm_define.h` — ARM32
- `unwind_riscv64_define.h` — RISC-V 64
- `unwind_loongarch64_define.h` — LoongArch64
- `unwind_x86_64_define.h` — x86_64

## Backtrace 模块

`backtrace_local.h` 提供简化的本地回栈 API，封装 Unwinder：

| 接口 | 说明 |
| --- | --- |
| `GetBacktraceStringByTid` | 获取指定线程栈（支持 FP/DWARF） |
| `GetBacktraceStringByTidWithMix` | 获取指定线程混合栈 |
| `PrintBacktrace` | 打印当前线程栈到 fd |
| `GetBacktrace` | 获取当前线程调用栈 |
| `GetProcessStacktrace` | 获取进程所有线程栈（不含当前线程） |

关键参数：
- `fast`：true=FP 回栈，false=DWARF 回栈
- `skipFrameNum`：跳过的帧数
- `maxFrameNums`：最大帧数，默认 256
- `enableKernelStack`：是否获取内核栈（需要 ioctl 权限）

## DfxAccessors 抽象

Unwinder 可以工作在多种模式下：

- 本地模式：直接读自身内存
- 远程模式：通过 ptrace 读目标进程内存
- 自定义模式：使用`UnwindAccessors`自定义的回栈所需的内存和寄存器访问接口

## 错误处理

- `GetLastErrorCode()` 返回最后回栈错误码
- `GetLastErrorAddr()` 返回最后错误地址
- 常见错误：unwind table 缺失、内存不可读、映射不存在

## 关键约束

1. **缓存策略**：`EnableUnwindCache` 开启后 ELF 解析结果缓存，远程回栈时减少重复解析。
2. **符号解析**：`EnableParseNativeSymbol` 控制是否解析函数名，关闭可加速但丢失函数信息。
3. **栈完整性**：FP 回栈可能不完整，DWARF 更可靠但更慢。崩溃场景应优先 DWARF。
4. **架构隔离**：各架构寄存器定义独立维护，不要跨架构复用寄存器操作代码。
5. **混合栈顺序**：JS 栈帧与 native 栈帧交替输出，修改解析顺序需保持一致性。
