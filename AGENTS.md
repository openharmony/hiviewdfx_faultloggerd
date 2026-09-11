# Faultloggerd 组件指引

## 项目定位

本仓库对应 OpenHarmony `base/hiviewdfx/faultloggerd`。它是 C/C++ 运行时崩溃临时日志的生成及管理模块，同时提供 Rust Panic 故障日志生成能力。优先按这些目录定位问题：

- `interfaces/innerkits/`：InnerKits 接口，包含信号处理器、回栈、抓栈、客户端、unwinder 等。
- `interfaces/common/`：跨模块共享的协议结构体和定义。
- `services/`：Faultloggerd 守护进程，包含服务端、管道、临时文件、coredump、minidump、快照。
- `tools/`：ProcessDump 二进制、DumpCatcher 命令行工具。
- `frameworks/`：主动抓栈实现、内存分配器、本地信号处理、轻量客户端。
- `common/`：工具库、日志、格式化器、trace。
- `interfaces/rust/`：Rust Panic 处理器、rustc_demangle、stacktrace。
- `services/config/`：运行配置（faultlogger.conf、fault_coredump.json、faultloggerd.cfg 等）。
- `test/`：单元测试、模块测试、系统测试、基准测试、Fuzz 测试、FuncHook 测试、崩溃构造器、Rust Panic构造器。

### 按任务类型定位代码

| 任务类型 | 首选目录 | 关键文件 |
| --- | --- | --- |
| 修改崩溃信号处理 | `interfaces/innerkits/signal_handler/` | `dfx_signal_handler.c`, `dfx_dumprequest.c` |
| 修改回栈（DWARF/FP/混合栈） | `interfaces/innerkits/unwinder/` | `unwinder.cpp`, `unwinder.h`, `src/unwind_entry_parser/` |
| 修改本地回栈接口 | `interfaces/innerkits/backtrace/` | `backtrace_local.cpp`, `backtrace_local.h` |
| 修改主动抓栈接口 | `interfaces/innerkits/dump_catcher/` | `dfx_dump_catcher.cpp`, `dfx_dump_catcher.h` |
| 修改客户端 socket 通信 | `interfaces/innerkits/faultloggerd_client/` | `faultloggerd_client.cpp`, `faultloggerd_socket.cpp` |
| 修改进程抓栈（ProcessDump） | `tools/process_dump/` | `process_dumper.cpp`, `dfx_process.cpp`, `dfx_thread.cpp` |
| 修改服务端逻辑 | `services/` | `fault_logger_daemon.cpp`, `fault_logger_service.cpp`, `fault_logger_server.cpp` |
| 修改日志结构 | `tools/process_dump/` | `dump_info_header.cpp`, `key_thread_dump_info.cpp`, `other_thread_dump_info.cpp` |
| 修改日志配置 | `services/config/` | `faultlogger.conf`, `fault_coredump.json`, `faultloggerd.cfg` |
| 修改 Coredump 逻辑 | `services/coredump/`, `tools/process_dump/coredump/` | `coredump_manager.cpp`, `coredump_controller.cpp` |
| 修改 Minidump 逻辑 | `services/minidump/`, `tools/process_dump/minidump_dumper.cpp` | `minidump_manager_service.cpp`, `minidump_dumper.cpp` |
| 修改异步栈追踪 | `interfaces/innerkits/async_stack/` | `async_stack.cpp`, `unique_stack_table.cpp`, `async_context_manager.cpp` |
| 修改内核快照解析 | `services/snapshot/` | `kernel_snapshot_parser.cpp`, `kernel_snapshot_trie.cpp` |
| 修改 Rust Panic 处理 | `interfaces/rust/panic_handler/` | `lib.rs` |
| 修改 Socket 协议结构 | `interfaces/common/` | `dfx_socket_request.h`, `dfx_define.h`, `dfx_dump_request.h` |
| 修改权限校验 | `services/fault_logger_service.cpp` | 各 Service 的 `Filter` 方法 |
| 修改临时文件管理 | `services/` | `temp_file_manager.cpp`, `fault_logger_pipe.cpp` |
| 修改崩溃构造器 | `tools/crasher_c/`, `tools/crasher_cpp/` | `dfx_crasher.c`, `dfx_crasher.cpp` |

### 嵌套指引

本仓库无目录级别的嵌套指引。所有任务级指导均通过 `docs/knowledge/` 中的场景文档提供。

## 构建和验证

构建命令从 OpenHarmony 源码根目录执行，不在本子目录执行。

```sh
./build.sh --product-name rk3568 --ccache --no-prebuilt-sdk --build-target faultloggerd_targets faultloggerd_tests
```

### 完成标准

任务被认为完成，当且仅当：

1. **代码改动已提交** - 使用 `git commit -s`，多代理协作时添加 `Co-Authored-By: Agent`
2. **本地构建通过** - 执行上述构建命令
3. **相关测试通过** - 对应单元测试通过
4. **板侧验证（如适用）** - 涉及崩溃抓栈、信号处理、回栈库的改动需提供板侧验证证据
5. **文档更新（如适用）** - 公共 API 修改需更新注释和文档

### 如果无法运行验证

明确说明无法运行的原因，列出推荐的验证步骤供人工执行，标记需要人工验证的部分。

### 完成报告格式

报告应包含：改动摘要（文件列表、改动点）、验证结果（构建/测试输出）、风险评估（API 兼容性、性能风险）、未完成事项。

## 知识索引

稳定背景知识放在 `docs/knowledge/`。改动前按场景读取对应文件：

### 场景与路径路由

| 场景 | 修改目录 | 先读文档 |
| --- | --- | --- |
| 崩溃信号处理、fork ProcessDump、日志结构、崩溃日志格式 | `interfaces/innerkits/signal_handler/`, `tools/process_dump/` | `docs/knowledge/crash-dump-pipeline.md` |
| 回栈、DWARF/FP 回栈、混合栈、本地/远程回栈、Backtrace 接口 | `interfaces/innerkits/unwinder/`, `interfaces/innerkits/backtrace/` | `docs/knowledge/unwinder-and-backtrace.md` |
| 主动抓栈、DumpCatcher 接口、SDK Dump、命令行工具、限流策略 | `interfaces/innerkits/dump_catcher/`, `tools/dump_catcher/` | `docs/knowledge/dump-catcher-and-sdk-dump.md` |
| 服务端架构、Socket 协议、Epoll 事件、权限校验、Coredump/Minidump 会话 | `services/`, `interfaces/common/dfx_socket_request.h` | `docs/knowledge/faultloggerd-service.md` |
| 构建、板侧测试、产物替换、SELinux 标签、崩溃验证 | 任何构建/测试相关改动 | `docs/knowledge/build-and-board-verification.md` |

### 开始编辑前

在修改代码前，按以下顺序确认：
1. 确认任务类别
2. 根据上表确定需要阅读的文档
3. 根据"项目约束"确认不违反任何约束
4. 声明："我将修改 X，已阅读 Y 文档，遵循 Z 约束"

## 项目约束

### 性能约束

- 信号处理器（SignalHandler）中只能执行 async-safe 调用，禁止 malloc/lock/IO 等非异步安全操作。
- 回栈和抓栈是高延迟操作，不要在回栈路径中增加全量扫描、字符串格式化或非必要的 INFO 日志。
- ProcessDump 的 `alarm(PROCESSDUMP_TIMEOUT)` 超时机制不可移除，防止进程永久阻塞。

### 架构约束

- SignalHandler、ProcessDump、Faultloggerd 三者职责分离，不要将服务端逻辑混入信号处理器或 ProcessDump。
- 本地回栈（BacktraceLocal）和远程回栈（Unwinder ptrace）路径保持独立，不要折叠到统一路径。
- 各架构（ARM64/ARM32/RISC-V/LoongArch64/x86_64）的寄存器定义独立维护，不要跨架构复用寄存器操作代码。
- 临时文件和管道的生命周期由服务端管理，客户端不可直接管理文件描述符的生命周期。

### 编码约定
#### 技术栈
- **语言**: c++11

#### 开发原则
- 编码前思考: 不要假设。不要隐藏困惑。呈现权衡。
- 简洁优先：用最少的代码解决问题。不要过渡推测。
- 精准修改：只清理自己造成的混乱。
- 目标驱动标准：定义成功标准。循环验证直到达成。

#### c/c++代码风格
- 函数名类名使用 UpperCamelCase 命名
- 局部变量名使用 camelCase 命名
- 类成员变量名使用 camelCase_ 命名
- 宏定义和全局静态不可变变量使用 UPPER_SNAKE_CASE 命名
- 函数的行数在不计算空行时不能超过50行
- 每行代码不能超过120个字符
- 圈复杂度不超过20
- 嵌套深度不能超过4
- 不允许使用try catch捕获异常
- 不允许纯空格的空行，需要空行的不要用空格进行缩进填充
- 全部不可变静态变量最好只允许基础类型，不能出现全局不可变类，例如，const std::string使用constexpr const char * const替代
- 变量在使用时才声明并初始化
- 不允许全空格行，需要空行直接换行
- 行末不能是空格
- && ||等逻辑操作符号不能在行首
- 不允许出现魔鬼数字，需要将其定义为有意义的const变量
- C++ 新增改动进行指针判空时，不要使用 `CHKPV*`、`CHKPR*`、`CHKPC*` 等改变代码逻辑的宏。
- C 代码使用 `extern "C"` 包裹公共接口，保持 C/C++ 混合链接兼容。
- 使用 `__attribute__((packed))` 的结构体不要随意调整字段顺序。

#### 不允许出现的基础问题
- 编译/解析错误：语法错误、类型错误、缺少导入
- 明确的逻辑错误：无论如何输入都会产生错误结果
- 安全问题：SQL 注入、XSS、硬编码密钥
- 明显的 API 误用：不符合标准用法（如 argparse type=bool）
- 严重的错误处理问题：空 catch 块、吞掉异常
- 接口契约不一致：API 文档单位与实现不匹配、头文件与实现不同步
- 调用链冗余/冲突：同一操作在调用链中被重复执行
- 参数声明未使用：函数签名中的参数在函数体内从未引用
- 资源泄漏：new 无 delete、open 无 close、捕获悬空指针
- 并发安全：全局变量无同步保护、回调中捕获 this 的生命周期风险
- 回调/lambda 捕获的指针生命周期风险
- 数组/vector 访问越界
- 空指针解引用
- 除法运算除零
- 外部输入不校验直接使用，外部路径访问前不使用realpath进行合法性校验
- 括号不平衡

#### 避免稳定问题的编码要求
- 禁止将外部传入的裸指针在内部直接构造智能指针
- 禁止多个独立创建的智能指针管理同一地址
- 禁止接口返回局部变量引用/指针
- 禁止在信号函数中加锁
- 禁止在信号处理函数中使用非信号安全的函数
- json对象在取值之前必须先判断类型，避免类型不匹配
- 避免使用未明确位宽的整型，选择使用int8_t、uint8_t等类型
- 指针变量、表示资源描述符的变量、bool变量必须赋初值
- 申请内存后异常退出前需要及时进行内存释放, 尽可能使用RAII进行资源管理
- 整数之间运算时必须严格检查，确保不会出现溢出、反转、除0
- 禁止对有符号整数进行位操作符运算
- 禁止使用内存操作类危险函数，需要使用安全函数
- 必须检查安全函数的返回值，并进行正确处理

### 公共 API 约束

**Do not（禁止）：**
- 修改已发布的 InnerKits API 的签名、参数类型、返回值类型（`dfx_signal_handler.h`、`faultloggerd_client.h`、`dfx_dump_catcher.h`、`backtrace_local.h`、`unwinder.h`、`async_stack.h` 等）
- 修改已有 API 的错误码，除非明确标注为废弃
- 删除或重命名已有公共 API
- 修改已有 API 的行为语义（如异步变同步、阻塞变非阻塞）

**Ask before（修改前必须确认）：**
- 新增公共 API：确认是否需要 DFX 日志、事件追踪、权限检查
- 修改内部接口：评估是否影响跨模块兼容性
- 修改错误处理逻辑：确认是否影响应用层的错误码兼容性

### 安全与权限边界

**Do not（禁止）：**
- 绕过已有的权限校验逻辑（各 Service 的 `Filter` 方法、UID 检查、进程归属检查）
- 在未验证的情况下直接使用跨进程传递的文件描述符、共享内存、管道数据
- 将崩溃进程的敏感信息（如密码、密钥）写入崩溃日志
- 修改涉及多用户/多账户访问控制的逻辑，除非经过安全评审

**Ask before（修改前必须确认）：**
- 涉及 `getsockopt(SO_PEERCRED)` 获取连接方凭据的改动
- 涉及 ptrace attach 其他进程的改动
- 涉及向其他进程发送信号（SIGDUMP 等）的改动
- 涉及进程命名空间（namespace）和沙箱路径处理的改动

### 协议与数据格式兼容性

**Do not（禁止）：**
- 修改 `dfx_socket_request.h` 中已定义的请求结构体字段顺序、类型
- 修改 `__attribute__((packed))` 结构体的内存布局
- 修改 Socket 名称（`faultloggerd.server` 等）和路径前缀
- 修改 `FaultLoggerType`、`FaultLoggerClientType`、`ResponseCode` 的已定义枚举值

**Ask before（修改前必须确认）：**
- 新增 Socket 请求类型：确认是否需要跨版本兼容性处理
- 新增 `FaultLoggerType` 枚举：确认不与现有值冲突
- 修改崩溃日志格式：确认是否影响 Hiview 和分析工具的解析

### 信号安全约束

**Do not（禁止）：**
- 在信号处理器回调中调用非 async-safe 函数（malloc、free、pthread_mutex_lock、printf 等）
- 在信号处理器中修改全局可变状态而不使用原子操作或 sig_atomic_t
- 移除信号处理器的超时保护（`g_signalHandlerMutex` + `SIGNAL_HANDLER_MUTEX_TIMEOUT_SEC`）

**Ask before（修改前必须确认）：**
- 新增注册信号处理函数
- 增加处理的信号类型

**正确做法：**
- 使用预分配的静态缓冲区（如 `g_request`、`g_callbackMsg`）
- 通过 fork + exec 分离危险操作到子进程
- 使用 `write`、`sigaction` 等 async-safe 调用

### 设备操作约束

**涉及真实设备时的注意事项：**
- 不执行可能影响设备正常运行的破坏性操作（如强制 kill 关键系统进程）
- 需要在真实设备上验证的改动，必须提供板侧证据（崩溃日志、hdc 输出）
- 替换 faultloggerd/processdump 二进制后需恢复 SELinux 标签（`restorecon`）
- 不要手动删除并重建 `/data/log/faultlog/temp` 目录，会导致 SELinux 标签失效
