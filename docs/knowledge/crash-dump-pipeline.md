# 崩溃抓栈处理流程

## 概述

本文档描述进程因异常信号崩溃后，faultloggerd 生成崩溃日志的完整流程。涉及的核心组件：信号处理器（SignalHandler）、进程抓栈二进制（ProcessDump）、Faultloggerd 守护进程、Hiview。

## 核心信号

崩溃抓栈覆盖的异常信号定义在 `interfaces/common/dfx_define.h`：

| 信号 | 值 | 含义 |
| --- | --- | --- |
| SIGILL | 4 | 非法指令 |
| SIGTRAP | 5 | 断点/陷阱异常 |
| SIGABRT | 6 | abort 发出 |
| SIGBUS | 7 | 非法内存访问 |
| SIGFPE | 8 | 浮点异常 |
| SIGSEGV | 11 | 无效内存访问 |
| SIGSTKFLT | 16 | 栈溢出 |
| SIGSYS | 31 | 系统调用异常 |

主动抓栈专用信号：`SIGDUMP=35`（远程抓栈）、`SIGLOCAL_DUMP=38`（本地抓栈）、`SIGLEAK_STACK=42`（内存泄漏类抓栈）。

## 关键文件与角色

| 组件 | 关键文件 | 职责 |
| --- | --- | --- |
| SignalHandler | `interfaces/innerkits/signal_handler/dfx_signal_handler.c` | 信号注册、上下文保存、fork ProcessDump |
| DumpRequest | `interfaces/innerkits/signal_handler/dfx_dumprequest.c` | 构造 ProcessDumpRequest、创建管道、exec processdump |
| ProcessDump | `tools/process_dump/process_dumper.cpp` | 读取请求、初始化 Unwinder、回栈、写入日志 |
| DumpInfo | `tools/process_dump/dump_info.h`、`dump_info_factory.h` | 按配置组装日志各部分内容 |
| FaultloggerdClient | `interfaces/innerkits/faultloggerd_client/` | ProcessDump 向服务端申请文件句柄 |
| Faultloggerd | `services/fault_logger_daemon.cpp`、`fault_logger_service.cpp` | 管理临时文件、管道、权限校验 |

## 处理流程

### 1. 信号注册（进程启动时）

- `dfx_signal_handler.c` 通过 `__attribute__((constructor))` 的 `InitHandler` 自动安装信号处理器。
- 使用 `sigchain` 注册到内核，覆盖上述崩溃信号。
- 全局静态变量 `g_request`（`struct ProcessDumpRequest`）在进程生命周期内保持有效，用于崩溃时快速填充上下文。

### 2. 信号到达与上下文保存

- 内核发送崩溃信号后进入 `DfxSignalHandler` 回调。
- 保存 `ucontext_t`（寄存器、PC、SP、fault address）到 `g_request`。
- 调用 `ThreadInfoCallBack`（如已注册）收集线程自定义信息。
- 通过互斥锁 `g_signalHandlerMutex` 防止并发崩溃导致重复抓栈，超时时间为 `PROCESSDUMP_TIMEOUT`（15s）。

### 3. Fork 子进程执行 ProcessDump

- `dfx_dumprequest.c` 中的 `StartProcessDump` 完成以下操作：
  1. 创建两对管道 `g_pipeFds`：`WRITE_TO_DUMP`（传栈数据）、`READ_FROM_DUMP_TO_CHILD`（传回栈结果）。
  2. 设置管道大小为 `MAX_PIPE_SIZE`（1MB）。
  3. `clone` 子进程，使用预分配栈 `RESERVED_CHILD_STACK_SIZE`（64位 32KB / 32位 16KB）。
  4. 子进程继承 capability（`CAP_SYS_PTRACE` 等）。
  5. 子进程 `execve` 执行 `/system/bin/processdump -signalhandler`。

### 4. ProcessDump 主流程

入口在 `tools/process_dump/main.cpp`，核心逻辑在 `process_dumper.cpp` 的 `ProcessDumper::Dump()`：

1. **读取请求** (`ReadRequestAndCheck`)：从 fd 读取 `ProcessDumpRequest`，校验信号和 pid。
2. **初始化 DfxProcess** (`InitDfxProcess`)：读取 `/proc/[pid]/` 下的 maps、status、task 等信息。
3. **初始化 Unwinder** (`InitUnwinder`)：创建 `Unwinder` 实例，解析 ELF/DWARF unwind tables。
4. **回栈** (`DumpProcess`)：
   - 对崩溃线程（key thread）使用 ptrace attach 后回栈。
   - 对其他线程使用 ptrace attach 后回栈（受 `dumpOtherThreads` 配置控制）。
   - 崩溃线程回栈使用 `UnwindRemote`（ptrace 方式），本地回栈使用 `UnwindLocal`。
5. **写日志**：按 `faultlogger.conf` 中 DumpInfo 配置组装各段内容，写入临时文件。
6. **上报**：通过 `Reporter` 上报 HiSysEvent 故障事件，Hiview 接收后生成正式 cppcrash 日志。

### 5. 日志输出路径

- 临时日志：`/data/log/faultlog/temp/cppcrash-[pid]-[timestamp].json`
- 正式日志：`/data/log/faultlog/faultlogger/`（由 Hiview 处理）
- ProcessDump 通过 `RequestFileDescriptor` 向 Faultloggerd 申请 fd 写入。

## 日志结构
崩溃日志按段组织，由 `faultlogger.conf` 的 `DumpInfos` 配置控制：

| 段 | 对应模块 | 内容 |
| --- | --- | --- |
| DumpInfoHeader | `dump_info_header.cpp` | Pid/Uid/进程名/Reason/时间戳 |
| KeyThreadDumpInfo | `key_thread_dump_info.cpp` | 崩溃线程调用栈、寄存器 |
| SubmitterStack | `submitter_stack.cpp` | 异步栈提交者链路（如果启用） |
| Registers | `registers.cpp` | 异常现场寄存器值 |
| ExtraCrashInfo | `extra_crash_info.cpp` | 附加崩溃信息（如 abort message） |
| OtherThreadDumpInfo | `other_thread_dump_info.cpp` | 非崩溃线程堆栈 |
| MemoryNearRegister | `memory_near_register.cpp` | 寄存器指向的内存数据 |
| FaultStack | `fault_stack.cpp` | 崩溃线程栈内存 dump |
| Maps | `maps.cpp` | 虚拟内存空间映射 |
| OpenFiles | `open_files.cpp` | 打开的文件列表 |

## 关键约束

1. **信号安全**：SignalHandler 中只能执行 async-safe 调用，避免 malloc/lock 等非异步安全操作。
2. **栈预留**：子进程使用预分配栈，避免依赖已崩溃进程的栈。
3. **超时控制**：ProcessDump 设置 `alarm(PROCESSDUMP_TIMEOUT)`，超时后进程退出，防止永久阻塞。
4. **单次崩溃**：通过 `g_dumpCount` 原子计数防止同一进程重复 fork ProcessDump。
5. **capability 继承**：子进程需要 `CAP_SYS_PTRACE` 才能 attach 其他线程。
6. **日志格式**：通常生成的cppcrash日志是json格式，但lite设备上生成的是文本格式。当日志内容不正确时需要同时检查采集的信息是否正确和json格式化是否正确。
