# Faultloggerd 服务端架构与 Socket 协议

## 概述

本文档描述 Faultloggerd 守护进程的架构、Epoll 事件模型、Socket 通信协议、权限校验机制，以及 Coredump 和 Minidump 子模块。

## 关键文件

| 组件 | 路径 | 职责 |
| --- | --- | --- |
| FaultLoggerDaemon | `services/fault_logger_daemon.h/cpp` | 守护进程单例，管理主服务器和辅助服务器 |
| SocketServer | `services/fault_logger_server.h/cpp` | Socket 监听和连接管理 |
| FaultLoggerService | `services/fault_logger_service.h/cpp` | 请求分发和各 Service 实现 |
| EpollManager | `services/epoll_manager.h/cpp` | Epoll 事件循环 |
| TempFileManager | `services/temp_file_manager.h/cpp` | 临时文件管理 |
| FaultLoggerConfig | `services/fault_logger_config.h/cpp` | 日志配置解析 |
| FaultLoggerPipe | `services/fault_logger_pipe.h/cpp` | 管道管理 |
| SocketRequest | `interfaces/common/dfx_socket_request.h` | 协议结构体定义 |
| Coredump | `services/coredump/` | Coredump 会话管理 |
| Minidump | `services/minidump/` | Minidump 管理 |
| Snapshot | `services/snapshot/` | 内核快照解析 |

## 进程架构

`main.cpp` 启动两个 Epoll 线程：

```
main 线程                          辅助线程（detach）
  |                                  |
  EpollManager.Init()                EpollManager.Init()
  FaultLoggerDaemon::InitMainServer()  FaultLoggerDaemon::InitHelperServer()
  EpollManager.StartEpoll()          EpollManager.StartEpoll()
```

- **主服务器**：监听 `faultloggerd.server` socket，处理文件句柄申请、管道管理、异常上报、统计。
- **辅助服务器**：监听 `faultloggerd.crash.server` 和 `faultloggerd.sdkdump.server`，处理 SDK Dump 和 Coredump 请求。

## Socket 协议

### Socket 路径

定义在 `dfx_socket_request.h`：

| Socket 名称 | 用途 |
| --- | --- |
| `faultloggerd.server` | 主服务器：文件句柄、管道、异常、统计 |
| `faultloggerd.crash.server` | 崩溃服务器：Coredump |
| `faultloggerd.sdkdump.server` | SDK Dump 服务器：主动抓栈 |

路径前缀：`/dev/unix/socket/`

### 请求格式

所有请求以 `RequestDataHead` 开头（`__attribute__((packed))`）：

```c
struct RequestDataHead {
    int8_t clientType;    // FaultLoggerClientType
    int32_t clientPid;    // 进程号
};
```

### 客户端类型

`FaultLoggerClientType` 定义了所有支持的请求类型：

| 类型 | Service 类 | 说明 |
| --- | --- | --- |
| `LOG_FILE_DES_CLIENT` | `FileDesService` | 申请日志文件 fd |
| `SDK_DUMP_CLIENT` | `SdkDumpService` | SDK 主动抓栈 |
| `PIPE_FD_CLIENT` | `PipeService` | 申请管道 fd |
| `REPORT_EXCEPTION_CLIENT` | `ExceptionReportService` | 上报异常 |
| `DUMP_STATS_CLIENT` | `StatsService` | 上报抓栈统计 |
| `COREDUMP_CLIENT` | （Coredump 模块） | Coredump 请求 |
| `COREDUMP_PROCESS_DUMP_CLIENT` | （Coredump 模块） | Coredump 状态上报 |
| `PIPE_FD_LITEPERF_CLIENT` | `LitePerfPipeService` | LitePerf 管道 |
| `LIMITED_PROCESS_DUMP_CLIENT` | `LiteProcDumperService` | 低权限进程抓栈 |
| `PIPE_FD_LIMITED_CLIENT` | `LiteProcDumperPipeService` | 低权限进程管道 |
| `MINIDUMP_CLIENT` | `MiniDumpService` | Minidump 设置 |
| `BINDER_PIDS_DUMP_CLIENT` | `BinderPidsDumpService` | Binder 关联进程抓栈 |

### 响应码

`ResponseCode` 定义了所有可能的响应：

| 码值 | 含义 |
| --- | --- |
| 0 | `REQUEST_SUCCESS` |
| 1 | `UNKNOWN_CLIENT_TYPE` |
| 2 | `INVALID_REQUEST_DATA` |
| 3 | `REQUEST_REJECT` |
| 4 | `ABNORMAL_SERVICE` |
| 5+ | 各场景专用码（SDK_DUMP_REPEAT 等） |

## 服务模板

`FaultLoggerService<T>` 是模板基类，按请求结构体大小自动反序列化：

```cpp
template<typename T>
class FaultLoggerService : public IFaultLoggerService {
    int32_t OnReceiveMsg(socketName, connectionFd, nRead, buf) {
        if (nRead != sizeof(T)) return INVALID_REQUEST_DATA;
        return OnRequest(socketName, connectionFd, *(T*)(buf.data()));
    }
};
```

各具体 Service 实现 `OnRequest` 和 `Filter`（权限校验）。

## Epoll 事件模型

`EpollManager` 管理 Epoll 事件循环，`EpollListener` 是事件监听基类：

```cpp
class EpollListener {
    EventResult OnEventPoll();  // 数据到达回调
    void OnTimeOut();           // 超时回调
};
```

- `SocketServerListener`：监听新连接
- `ClientRequestListener`：处理客户端请求，持有 `uid_t` 用于权限校验
- `TempFileListener`：监听临时文件创建（Binder 抓栈场景）

## 权限校验

各 Service 的 `Filter` 方法进行权限校验：

1. **UID 检查**：通过 `getsockopt(SO_PEERCRED)` 获取连接方 uid。
2. **重复请求检测**：SDK Dump 检测重复请求返回 `SDK_DUMP_REPEAT`。
3. **资源限制**：`LitePerfPipeService::PerfResourceLimiter` 限制并发 perf 数量。

## 临时文件管理

`TempFileManager` 负责创建和管理临时日志文件：

- 崩溃日志临时路径：`/data/log/faultlog/temp/`
- 文件命名：`cppcrash-[pid]-[timestamp]`
- 支持通过 `CreateFileDescriptor` 静态方法直接创建 fd（用于本地崩溃处理器）。

## Coredump 子系统

`services/coredump/` 实现 Coredump 会话管理：

| 类 | 职责 |
| --- | --- |
| `CoredumpFacade` | 外部入口门面 |
| `CoredumpManagerService` | 核心管理 |
| `CoredumpSessionManager` | 会话生命周期 |
| `CoredumpSessionService` | 会话服务 |
| `CoredumpSessionState` | 会话状态机 |
| `CoredumpSignalService` | 信号处理 |
| `CoredumpTaskScheduler` | 任务调度 |

配置文件：`services/config/fault_coredump.json`，定义 Coredump profile（FULL 模式等）。

## Minidump 子系统

`services/minidump/` 提供 Minidump 生成能力：

- `MinidumpManagerService` 管理 Minidump 生命周期
- 通过 `MiniDumpService` 接收请求，向目标进程发送 `SIGDUMP(35)` + `si_code=7`
- 拉起ProcessDump后执行-minidump进行minidump文件生成、按需解析生成cppcrash日志并上报故障

## 关键约束

1. **协议兼容性**：`RequestDataHead` 和各请求结构体使用 `__attribute__((packed))`，修改字段顺序或类型会破坏跨进程通信。
2. **Socket 路径**：三个 socket 名称是系统约定，客户端硬编码连接。
3. **权限校验**：Filter 方法不可绕过，新增 Service 必须实现权限校验。
4. **连接数限制**：`maxConnection=30`，`maxEpollEvent=1024`。
5. **SELinux 标签**：临时目录需要正确的 SELinux 标签（`faultloggerd_temp_file`），标签失效会导致日志写入为空。
