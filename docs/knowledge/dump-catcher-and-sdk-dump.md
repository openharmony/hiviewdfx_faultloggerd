# 主动抓栈：DumpCatcher 与 SDK Dump

## 概述

本文档描述 faultloggerd 提供的主动抓栈能力，包括应用层调用的 DumpCatcher 接口、SDK Dump 接口、以及命令行工具的使用场景和处理流程。

## 关键文件

| 组件 | 路径 | 职责 |
| --- | --- | --- |
| DfxDumpCatcher | `interfaces/innerkits/dump_catcher/dfx_dump_catcher.cpp` | 主动抓栈接口封装 |
| DumpCatcher CLI | `tools/dump_catcher/dump_catcher.cpp`、`main.cpp` | 命令行工具 |
| FaultloggerdClient | `interfaces/innerkits/faultloggerd_client/faultloggerd_client.cpp` | 客户端 socket 通信 |
| SdkDumpService | `services/fault_logger_service.cpp` | 服务端 SDK Dump 请求处理 |
| SlowPolicy | `interfaces/innerkits/dump_catcher/dfx_dump_catcher_slow_policy.cpp` | 限流策略 |
| Errno | `interfaces/innerkits/dump_catcher/dfx_dump_catcher_errno.h` | 错误码定义 |

## DumpCatcher 接口

`DfxDumpCatcher`（`dfx_dump_catcher.h`）提供以下接口：

| 接口 | 说明 | 本地/远程 |
| --- | --- | --- |
| `DumpCatch(pid, tid, msg, ...)` | 抓取指定进程线程的栈 | 本地直接回栈/远程走请求服务端 |
| `DumpCatchFd(pid, tid, msg, fd, ...)` | 抓栈并写入指定 fd | 本地直接回栈/远程走请求服务端 |
| `DumpCatchMultiPid(pids, msg)` | 批量抓取多进程栈 | 远程 |
| `DumpCatchWithTimeout(pid, msg, timeout, ...)` | 带超时的抓栈 | 本地+远程 |

### 本地 vs 远程判断

- 当 `pid == getpid()` 时走本地回栈路径，直接调用 `BacktraceLocal` / `Unwinder`。
- 当 `pid != getpid()` 时走远程路径，向 Faultloggerd 发送 `SDK_DUMP_CLIENT` 请求。

### 远程抓栈流程

```
调用方进程                        Faultloggerd                    目标进程
    |                                  |                              |
    |--RequestSdkDump(pid, tid)------>|                              |
    |                                  |--send SIGDUMP(35)---------->|
    |                                  |                              |
    |<--pipeFd[2]----------------------|<--pipeFd (回栈结果)----------|
    |                                  |                              |
    |<--读取栈数据 from pipeFd---------|                              |
```

1. 调用方通过 `faultloggerd_client` 向服务端发送 `SdkDumpRequestData`。
2. 服务端 `SdkDumpService::OnRequest` 接收请求，创建管道。
3. 服务端向目标进程发送 `SIGDUMP(35)` 信号。
4. 目标进程的 SignalHandler 收到 SIGDUMP 后 fork ProcessDump。
5. ProcessDump 通过管道将回栈结果回传给调用方。
6. 调用方从 `pipeFd[0]` 读取栈数据，从 `pipeFd[1]` 读取结果状态。

### 超时与限流

- `DumpCatchWithTimeout` 支持自定义超时（最小 1000ms，默认 3000ms）。
- `dfx_dump_catcher_slow_policy.cpp` 实现限流策略，5s之内如果抓取目标进程已经失败过，直接抓内核栈。
- 服务端返回 `ResponseCode`（如 `SDK_DUMP_REPEAT`、`SDK_DUMP_NOPROC`）。

## DumpCatcher 错误码

`dfx_dump_catcher_errno.h` 定义了错误码，`DumpCatchWithTimeout` 返回 `std::pair<int, std::string>`：

| ret 值 | 含义 |
| --- | --- |
| -1 | 抓栈失败（reason 含失败原因） |
| 0 | 正常用户态栈 |
| 1 | 内核栈（非 JSON 格式） |

## SDK Dump 接口

`faultloggerd_client.h` 提供底层 SDK Dump 接口：

```cpp
int32_t RequestSdkDump(int32_t pid, int32_t tid, int (&pipeReadFd)[2], bool isjson, int timeout);
```

- `pipeReadFd[0]`：读取调用栈消息
- `pipeReadFd[1]`：读取回栈结果
- `isjson`：true 返回 JSON 格式，false 返回字符串
-  以下进程才能通过请求
    0, // rootUid
    1000, // bmsUid
    1201, // hiviewUid
    1202, // faultloggerdUid
    1212, // hidumperServiceUid
    5523, // foundationUid
    7400, // dev_assistant

## 命令行工具

`dumpcatcher` 位于 `/system/bin/`：

```sh
dumpcatcher -p [pid]              # 打印进程所有线程栈
dumpcatcher -p [pid] -t [tid]     # 打印指定线程栈
```

仅 Debug 版本提供，需要root权限。

## 其他客户端接口

`faultloggerd_client.h` 还提供以下接口：

| 接口 | 用途 |
| --- | --- |
| `RequestFileDescriptor(type)` | 申请临时文件 fd |
| `RequestPipeFd(pid, pipeType, pipeFd)` | 申请管道 fd |
| `RequestDelPipeFd(pid)` | 删除管道 fd |
| `ReportDumpStats(request)` | 上报抓栈统计信息 |
| `CancelCoredump(targetPid)` | 取消 coredump |
| `StartCoredumpCb(targetPid, processDumpPid)` | coredump 开始回调 |
| `FinishCoredumpCb(targetPid, fileName, ret)` | coredump 完成回调 |
| `RequestBinderPidsDump(pid, binderPids, ...)` | 批量抓取 binder 关联进程栈 |
| `SaveCoredumpToFileTimeout(targetPid, timeoutMs)` | coredump 到文件 |

## 关键约束

1. **权限**：DumpCatcher 需要管理员（system/root）权限或抓取自身进程。
2. **暂停时间**：远程抓栈会暂停目标进程，高频调用影响性能。限流策略不可绕过。
3. **管道生命周期**：`DumpCatcherPipeData` 析构时自动调用 `RequestDelPipeFd` 清理服务端资源。
4. **JSON 格式**：`DumpCatch` 的 `isJson` 参数和 `RequestSdkDump` 的 `isjson` 参数需保持一致使用。
5. **并发冲突**：可能存在多处同时请求远程抓栈同一目标进程，服务端通过 `SDK_DUMP_REPEAT` 检测重复请求。
