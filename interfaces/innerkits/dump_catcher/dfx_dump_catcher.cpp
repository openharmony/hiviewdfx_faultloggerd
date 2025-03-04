/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dfx_dump_catcher.h"

#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <dlfcn.h>
#include <poll.h>
#include <securec.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "backtrace_local.h"
#include "dfx_define.h"
#include "dfx_dump_catcher_errno.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#include "elapsed_time.h"
#include "faultloggerd_client.h"
#include "file_ex.h"
#include "kernel_stack_async_collector.h"
#include "procinfo.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxDumpCatcher"
#endif
static const int DUMP_CATCHE_WORK_TIME_S = 60;
static constexpr int WAIT_GET_KERNEL_STACK_TIMEOUT = 1000; // 1000 : time out 1000 ms
static constexpr uint32_t HIVIEW_UID = 1201;
static constexpr uint32_t FOUNDATION_UID = 5523;

enum DfxDumpPollRes : int32_t {
    DUMP_POLL_INIT = -1,
    DUMP_POLL_OK,
    DUMP_POLL_FD,
    DUMP_POLL_FAILED,
    DUMP_POLL_TIMEOUT,
    DUMP_POLL_RETURN,
};

enum DfxDumpStatRes : int32_t {
    DUMP_RES_NO_KERNELSTACK = -2,
    DUMP_RES_WITH_KERNELSTACK = -1,
    DUMP_RES_WITH_USERSTACK = 0,
};
}

static bool IsLinuxKernel()
{
    static bool isLinux = [] {
        std::string content;
        LoadStringFromFile("/proc/version", content);
        if (content.empty()) {
            return true;
        }
        if (content.find("Linux") != std::string::npos) {
            return true;
        }
        return false;
    }();
    return isLinux;
}

class DfxDumpCatcher::Impl {
public:
    bool DumpCatch(int pid, int tid, std::string& msg, size_t maxFrameNums, bool isJson);
    bool DumpCatchFd(int pid, int tid, std::string& msg, int fd, size_t maxFrameNums);
    bool DumpCatchMultiPid(const std::vector<int> &pids, std::string& msg);
    int DumpCatchProcess(int pid, std::string& msg, size_t maxFrameNums, bool isJson);
    std::pair<int, std::string> DumpCatchWithTimeout(int pid, std::string& msg, int timeout, int tid, bool isJson);
private:
    bool DoDumpCurrTid(const size_t skipFrameNum, std::string& msg, size_t maxFrameNums);
    bool DoDumpLocalTid(const int tid, std::string& msg, size_t maxFrameNums);
    bool DoDumpLocalPid(int pid, std::string& msg, size_t maxFrameNums);
    bool DoDumpLocalLocked(int pid, int tid, std::string& msg, size_t maxFrameNums);
    int32_t DoDumpRemoteLocked(int pid, int tid, std::string& msg, bool isJson = false,
        int timeout = DUMPCATCHER_REMOTE_TIMEOUT);
    int32_t DoDumpCatchRemote(int pid, int tid, std::string& msg, bool isJson = false,
        int timeout = DUMPCATCHER_REMOTE_TIMEOUT);
    int DoDumpRemotePid(int pid, std::string& msg, int (&pipeReadFd)[2],
        bool isJson = false, int32_t timeout = DUMPCATCHER_REMOTE_TIMEOUT);
    bool HandlePollError(const uint64_t endTime, int& remainTime, std::string& resMsg, int& ret);
    bool HandlePollTimeout(const int timeout, int& remainTime, std::string& resMsg, int& ret);
    bool HandlePollEvents(std::pair<int, std::string>& bufState, std::pair<int, std::string>& resState,
        const struct pollfd (&readFds)[2], bool& bPipeConnect, bool& res);
    std::pair<bool, int> DumpRemotePoll(const int timeout, std::pair<int, std::string>& bufState,
        std::pair<int, std::string>& resState);
    int DoDumpRemotePoll(int timeout, std::string& msg, const int (&pipeReadFd)[2], bool isJson = false);
    bool DoReadBuf(int fd, std::string& msg);
    bool DoReadRes(int fd, bool& ret, std::string& msg);
    void DealWithPollRet(int pollRet, int pid, int32_t& ret, std::string& msg);
    void DealWithSdkDumpRet(int sdkdumpRet, int pid, int32_t& ret, std::string& msg);
    std::pair<int, std::string> DealWithDumpCatchRet(int pid, int32_t& ret, std::string& msg);
    void ReportDumpCatcherStats(int32_t pid, uint64_t requestTime, int32_t ret, void* retAddr);

    static int32_t KernelRet2DumpcatchRet(int32_t ret);
    static const int DUMPCATCHER_REMOTE_P90_TIMEOUT = 1000;
    static const int DUMPCATCHER_REMOTE_TIMEOUT = 10000;

    std::mutex mutex_;
    int32_t pid_ = -1;
    bool notifyCollect_ = false;
    KernelStackAsyncCollector stackKit_;
    KernelStackAsyncCollector::KernelResult stack_;
};

DfxDumpCatcher::DfxDumpCatcher() : impl_(std::make_shared<Impl>())
{}

bool DfxDumpCatcher::DumpCatch(int pid, int tid, std::string& msg, size_t maxFrameNums, bool isJson)
{
    return impl_->DumpCatch(pid, tid, msg, maxFrameNums, isJson);
}

bool DfxDumpCatcher::DumpCatchFd(int pid, int tid, std::string& msg, int fd, size_t maxFrameNums)
{
    return impl_->DumpCatchFd(pid, tid, msg, fd, maxFrameNums);
}

bool DfxDumpCatcher::DumpCatchMultiPid(const std::vector<int> &pids, std::string& msg)
{
    return impl_->DumpCatchMultiPid(pids, msg);
}

int DfxDumpCatcher::DumpCatchProcess(int pid, std::string& msg, size_t maxFrameNums, bool isJson)
{
    return impl_->DumpCatchProcess(pid, msg, maxFrameNums, isJson);
}

std::pair<int, std::string> DfxDumpCatcher::DumpCatchWithTimeout(int pid, std::string& msg,
    int timeout, int tid, bool isJson)
{
    return impl_->DumpCatchWithTimeout(pid, msg, timeout, tid, isJson);
}

bool DfxDumpCatcher::Impl::DoDumpCurrTid(const size_t skipFrameNum, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;

    ret = GetBacktrace(msg, skipFrameNum + 1, false, maxFrameNums);
    if (!ret) {
        int currTid = gettid();
        msg.append("Failed to dump curr thread:" + std::to_string(currTid) + ".\n");
    }
    DFXLOGD("DoDumpCurrTid :: return %{public}d.", ret);
    return ret;
}

bool DfxDumpCatcher::Impl::DoDumpLocalTid(const int tid, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;
    if (tid <= 0) {
        DFXLOGE("DoDumpLocalTid :: return false as param error.");
        return ret;
    }
    ret = GetBacktraceStringByTid(msg, tid, 0, false, maxFrameNums);
    if (!ret) {
        msg.append("Failed to dump thread:" + std::to_string(tid) + ".\n");
    }
    DFXLOGD("DoDumpLocalTid :: return %{public}d.", ret);
    return ret;
}

bool DfxDumpCatcher::Impl::DoDumpLocalPid(int pid, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;
    if (pid <= 0) {
        DFXLOGE("DoDumpLocalPid :: return false as param error.");
        return ret;
    }
    size_t skipFramNum = 5; // 5: skip 5 frame

    msg = GetStacktraceHeader();
    std::function<bool(int)> func = [&](int tid) {
        if (tid <= 0) {
            return false;
        }
        std::string threadMsg;
        if (tid == gettid()) {
            ret = DoDumpCurrTid(skipFramNum, threadMsg, maxFrameNums);
        } else {
            ret = DoDumpLocalTid(tid, threadMsg, maxFrameNums);
        }
        msg += threadMsg;
        return ret;
    };
    std::vector<int> tids;
    ret = GetTidsByPidWithFunc(getpid(), tids, func);
    DFXLOGD("DoDumpLocalPid :: return %{public}d.", ret);
    return ret;
}

int32_t DfxDumpCatcher::Impl::DoDumpRemoteLocked(int pid, int tid, std::string& msg, bool isJson, int timeout)
{
    return DoDumpCatchRemote(pid, tid, msg, isJson, timeout);
}

bool DfxDumpCatcher::Impl::DoDumpLocalLocked(int pid, int tid, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;
    if (tid == gettid()) {
        size_t skipFramNum = 4; // 4: skip 4 frame
        ret = DoDumpCurrTid(skipFramNum, msg, maxFrameNums);
    } else if (tid == 0) {
        ret = DoDumpLocalPid(pid, msg, maxFrameNums);
    } else {
        if (!IsThreadInPid(pid, tid)) {
            msg.append("tid(" + std::to_string(tid) + ") is not in pid(" + std::to_string(pid) + ").\n");
        } else {
            ret = DoDumpLocalTid(tid, msg, maxFrameNums);
        }
    }

    DFXLOGD("DoDumpLocal :: ret(%{public}d).", ret);
    return ret;
}

void DfxDumpCatcher::Impl::ReportDumpCatcherStats(int32_t pid,
    uint64_t requestTime, int32_t ret, void* retAddr)
{
    std::vector<uint8_t> buf(sizeof(struct FaultLoggerdStatsRequest), 0);
    auto stat = reinterpret_cast<struct FaultLoggerdStatsRequest*>(buf.data());
    stat->type = DUMP_CATCHER;
    stat->pid = pid;
    stat->requestTime = requestTime;
    stat->dumpCatcherFinishTime = GetTimeMilliSeconds();
    stat->result = (ret == DUMPCATCH_ESUCCESS) ? DUMP_RES_WITH_USERSTACK : DUMP_RES_WITH_KERNELSTACK;
    if ((ret != DUMPCATCH_ESUCCESS) && stack_.second.empty()) {
        stat->result = DUMP_RES_NO_KERNELSTACK;
    }
    size_t copyLen;
    std::string processName;
    ReadProcessName(pid, processName);
    copyLen = std::min(sizeof(stat->targetProcess) - 1, processName.size());
    if (memcpy_s(stat->targetProcess, sizeof(stat->targetProcess) - 1, processName.c_str(), copyLen) != 0) {
        DFXLOGE("Failed to copy target process");
        return;
    }

    if (ret != DUMPCATCH_ESUCCESS) {
        std::string summary = DfxDumpCatchError::ToString(ret);
        copyLen = std::min(sizeof(stat->summary) - 1, summary.size());
        if (memcpy_s(stat->summary, sizeof(stat->summary) - 1, summary.c_str(), copyLen) != 0) {
            DFXLOGE("Failed to copy dumpcatcher summary");
            return;
        }
    }

    Dl_info info;
    if (dladdr(retAddr, &info) != 0) {
        copyLen = std::min(sizeof(stat->callerElf) - 1, strlen(info.dli_fname));
        if (memcpy_s(stat->callerElf, sizeof(stat->callerElf) - 1, info.dli_fname, copyLen) != 0) {
            DFXLOGE("Failed to copy caller elf info");
            return;
        }
        stat->offset = reinterpret_cast<uintptr_t>(retAddr) - reinterpret_cast<uintptr_t>(info.dli_fbase);
    }

    std::string cmdline;
    if (OHOS::LoadStringFromFile("/proc/self/cmdline", cmdline)) {
        copyLen = std::min(sizeof(stat->callerProcess) - 1, cmdline.size());
        if (memcpy_s(stat->callerProcess, sizeof(stat->callerProcess) - 1,
            cmdline.c_str(), copyLen) != 0) {
            DFXLOGE("Failed to copy caller cmdline");
            return;
        }
    }

    ReportDumpStats(stat);
}

static bool IsBitOn(const std::string& content, const std::string& filed, int signal)
{
    if (content.find(filed) == std::string::npos) {
        return false;
    }
    //SigBlk:   0000000000000000
    std::string num = content.substr(content.find(filed) + filed.size() + 2, 16);
    uint64_t hexValue = strtoul(num.c_str(), nullptr, 16);
    uint64_t mask = 1ULL << (signal - 1);

    return (hexValue & mask) != 0;
}

static bool IsSignalBlocked(int pid, int32_t& ret)
{
    std::vector<int> tids;
    std::vector<int> nstids;
    GetTidsByPid(pid, tids, nstids);
    std::string threadName;
    std::string content;
    int targetTid = -1;
    for (size_t i = 0; i < tids.size(); ++i) {
        ReadThreadNameByPidAndTid(pid, tids[i], threadName);
        if (threadName == "OS_DfxWatchdog") {
            targetTid = tids[i];
            break;
        }
    }
    if (targetTid != -1) {
        std::string threadStatusPath = StringPrintf("/proc/%d/task/%d/status", pid, targetTid);
        if (!LoadStringFromFile(threadStatusPath, content) || content.empty()) {
            DFXLOGE("the pid(%{public}d)thread(%{public}d) read status fail, errno(%{public}d)", pid, targetTid, errno);
            ret = DUMPCATCH_UNKNOWN;
            return true;
        }

        if (IsBitOn(content, "SigBlk", SIGDUMP) || IsBitOn(content, "SigIgn", SIGDUMP)) {
            DFXLOGI("the pid(%{public}d)thread(%{public}d) signal has been blocked by target process", pid, targetTid);
            ret = DUMPCATCH_TIMEOUT_SIGNAL_BLOCK;
            return true;
        }
    }
    return false;
}

static bool IsFrozen(int pid, int32_t& ret)
{
    std::string content;
    std::string cgroupPath = StringPrintf("/proc/%d/cgroup", pid);
    if (!LoadStringFromFile(cgroupPath, content)) {
        DFXLOGE("the pid (%{public}d) read cgroup fail, errno (%{public}d)", pid, errno);
        ret = DUMPCATCH_UNKNOWN;
        return true;
    }

    if (content.find("Frozen") != std::string::npos) {
        DFXLOGI("the pid (%{public}d) has been frozen", pid);
        ret = DUMPCATCH_TIMEOUT_KERNEL_FROZEN;
        return true;
    }
    return false;
}

static void AnalyzeTimeoutReason(int pid, int32_t& ret)
{
    std::string statusPath = StringPrintf("/proc/%d/status", pid);
    if (access(statusPath.c_str(), F_OK) != 0) {
        DFXLOGI("the pid (%{public}d) process exit during the dump, errno (%{public}d)", pid, errno);
        ret = DUMPCATCH_TIMEOUT_PROCESS_KILLED;
        return;
    }

    if (IsSignalBlocked(pid, ret)) {
        return;
    }

    if (IsFrozen(pid, ret)) {
        return;
    }

    DFXLOGI("the pid (%{public}d) dump slow", pid);
    ret = DUMPCATCH_TIMEOUT_DUMP_SLOW;
}

void DfxDumpCatcher::Impl::DealWithPollRet(int pollRet, int pid, int32_t& ret, std::string& msg)
{
    if (pollRet == DUMP_POLL_OK) {
        ret = DUMPCATCH_ESUCCESS;
        return;
    }
    // get result
    if (notifyCollect_) {
        stack_ = stackKit_.GetCollectedStackResult();
    } else {
        stack_ = stackKit_.GetProcessStackWithTimeout(pid, WAIT_GET_KERNEL_STACK_TIMEOUT);
    }

    std::string halfProcStatus;
    std::string halfProcWchan;
    ReadProcessStatus(halfProcStatus, pid);
    ReadProcessWchan(halfProcWchan, pid, false, true);
    msg.append(std::move(halfProcStatus));
    msg.append(std::move(halfProcWchan));
    switch (pollRet) {
        case DUMP_POLL_FD:
            ret = DUMPCATCH_EFD;
            break;
        case DUMP_POLL_FAILED:
            ret = DUMPCATCH_EPOLL;
            break;
        case DUMP_POLL_TIMEOUT:
            AnalyzeTimeoutReason(pid, ret);
            break;
        case DUMP_POLL_RETURN:
            if (msg.find("ptrace attach thread failed") != std::string::npos) {
                ret = DUMPCATCH_DUMP_EPTRACE;
            } else if (msg.find("stop unwinding") != std::string::npos) {
                ret = DUMPCATCH_DUMP_EUNWIND;
            } else if (msg.find("mapinfo is not exist") != std::string::npos) {
                ret = DUMPCATCH_DUMP_EMAP;
            } else {
                ret = DUMPCATCH_UNKNOWN;
            }
            break;
        default:
            ret = DUMPCATCH_UNKNOWN;
            break;
    }
}

void DfxDumpCatcher::Impl::DealWithSdkDumpRet(int sdkdumpRet, int pid, int32_t& ret, std::string& msg)
{
    uint32_t uid = getuid();
    if (sdkdumpRet == ResponseCode::SDK_DUMP_REPEAT) {
        stack_ = stackKit_.GetProcessStackWithTimeout(pid, WAIT_GET_KERNEL_STACK_TIMEOUT);
        msg.append("Result: pid(" + std::to_string(pid) + ") process is dumping.\n");
        ret = DUMPCATCH_IS_DUMPING;
    } else if (sdkdumpRet == ResponseCode::REQUEST_REJECT) {
        msg.append("Result: pid(" + std::to_string(pid) + ") process check permission error.\n");
        ret = DUMPCATCH_EPERMISSION;
    } else if (sdkdumpRet == ResponseCode::SDK_DUMP_NOPROC) {
        msg.append("Result: pid(" + std::to_string(pid) + ") process has exited.\n");
        ret = DUMPCATCH_NO_PROCESS;
    } else if (sdkdumpRet == ResponseCode::SDK_PROCESS_CRASHED) {
        msg.append("Result: pid(" + std::to_string(pid) + ") process has been crashed.\n");
        ret = DUMPCATCH_HAS_CRASHED;
    } else if (sdkdumpRet == ResponseCode::CONNECT_FAILED) {
        if (uid == HIVIEW_UID || uid == FOUNDATION_UID) {
            stack_ = stackKit_.GetProcessStackWithTimeout(pid, WAIT_GET_KERNEL_STACK_TIMEOUT);
        }
        msg.append("Result: pid(" + std::to_string(pid) + ") process fail to conntect faultloggerd.\n");
        ret = DUMPCATCH_ECONNECT;
    } else if (sdkdumpRet == ResponseCode::SEND_DATA_FAILED) {
        if (uid == HIVIEW_UID || uid == FOUNDATION_UID) {
            stack_ = stackKit_.GetProcessStackWithTimeout(pid, WAIT_GET_KERNEL_STACK_TIMEOUT);
        }
        msg.append("Result: pid(" + std::to_string(pid) + ") process fail to write to faultloggerd.\n");
        ret = DUMPCATCH_EWRITE;
    }
    DFXLOGW("%{public}s :: %{public}s", __func__, msg.c_str());
}

std::pair<int, std::string> DfxDumpCatcher::Impl::DealWithDumpCatchRet(int pid, int32_t& ret, std::string& msg)
{
    int result = ret == 0 ? 0 : -1;
    std::string reason;
    if (result == 0) {
        reason = "Reason:" + DfxDumpCatchError::ToString(ret) + "\n";
    } else {
        reason = "Reason:\nnormal stack:" + DfxDumpCatchError::ToString(ret) + "\n";
        if (stack_.first != KernelStackAsyncCollector::STACK_SUCCESS) {
            ret = KernelRet2DumpcatchRet(stack_.first);
            reason += "kernel stack:" + DfxDumpCatchError::ToString(ret) + "\n";
        } else if (!stack_.second.empty()) {
            msg.append(stack_.second);
            result = 1;
        } else {
            reason += "kernel stack:" + DfxDumpCatchError::ToString(DUMPCATCH_KERNELSTACK_NONEED) + "\n";
        }
    }
    std::string toFind = "Result:";
    size_t startPos = msg.find(toFind);
    if (startPos != std::string::npos) {
        size_t endPos = msg.find("\n", startPos);
        if (endPos != std::string::npos) {
            msg.erase(startPos, endPos - startPos + 1);
        }
    }
    return std::make_pair(result, reason);
}

std::pair<int, std::string> DfxDumpCatcher::Impl::DumpCatchWithTimeout(int pid, std::string& msg, int timeout,
    int tid, bool isJson)
{
    DfxEnableTraceDlsym(true);
    ElapsedTime counter;
    uint64_t requestTime = GetTimeMilliSeconds();
    int32_t dumpcatchErrno = DUMPCATCH_UNKNOWN;
    bool reportStat = false;
    do {
        if (pid <= 0 || tid <0 || timeout <= WAIT_GET_KERNEL_STACK_TIMEOUT) {
            DFXLOGE("DumpCatchWithTimeout:: param error.");
            dumpcatchErrno = DUMPCATCH_EPARAM;
            break;
        }
        if (!IsLinuxKernel()) {
            std::string statusPath = StringPrintf("/proc/%d/status", pid);
            if (access(statusPath.c_str(), F_OK) != 0 && errno != EACCES) {
                DFXLOGE("DumpCatchWithTimeout:: the pid(%{public}d) process has exited, errno(%{public}d)", pid, errno);
                msg.append("Result: pid(" + std::to_string(pid) + ") process has exited.\n");
                dumpcatchErrno = DUMPCATCH_NO_PROCESS;
                break;
            }
        }
        std::unique_lock<std::mutex> lck(mutex_);
        int currentPid = getpid();
        if (pid == currentPid) {
            DFXLOGE("DumpCatchWithTimeout:: param error (don't support dumpcatch self)");
            dumpcatchErrno = DUMPCATCH_EPARAM;
            break;
        } else {
            DFXLOGI("Receive DumpCatch request for cPid:(%{public}d), pid(%{public}d)", currentPid, pid);
            dumpcatchErrno = DoDumpRemoteLocked(pid, tid, msg, isJson, timeout);
            reportStat = true;
        }
    } while (false);

    auto result = DealWithDumpCatchRet(pid, dumpcatchErrno, msg);
    if (reportStat) {
        void* retAddr = __builtin_return_address(0);
        ReportDumpCatcherStats(pid, requestTime, dumpcatchErrno, retAddr);
    }

    DFXLOGI("dump_catch : pid = %{public}d, elapsed time = %{public}" PRId64 " ms, " \
        "msgLength = %{public}zu, ret = %{public}d\n%{public}s",
        pid, counter.Elapsed<std::chrono::milliseconds>(), msg.size(), result.first, result.second.c_str());
    DfxEnableTraceDlsym(false);
    return result;
}

int DfxDumpCatcher::Impl::DumpCatchProcess(int pid, std::string& msg, size_t maxFrameNums, bool isJson)
{
    if (DumpCatch(pid, 0, msg, maxFrameNums, isJson)) {
        return 0;
    }
    // kernel stack
    if (stack_.first == KernelStackAsyncCollector::STACK_SUCCESS && !stack_.second.empty()) {
        msg.append(std::move(stack_.second));
        return 1;
    }
    return -1;
}

bool DfxDumpCatcher::Impl::DumpCatch(int pid, int tid, std::string& msg, size_t maxFrameNums, bool isJson)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        DFXLOGE("dump_catch :: param error.");
        return ret;
    }
    if (!IsLinuxKernel()) {
        std::string statusPath = StringPrintf("/proc/%d/status", pid);
        if (access(statusPath.c_str(), F_OK) != 0 && errno != EACCES) {
            DFXLOGE("DumpCatch:: the pid(%{public}d) process has exited, errno(%{public}d)", pid, errno);
            msg.append("Result: pid(" + std::to_string(pid) + ") process has exited.\n");
            return ret;
        }
    }
    DfxEnableTraceDlsym(true);
    ElapsedTime counter;
    std::unique_lock<std::mutex> lck(mutex_);
    stack_ = {};
    notifyCollect_ = false;
    int currentPid = getpid();
    uint64_t requestTime = GetTimeMilliSeconds();
    DFXLOGI("Receive DumpCatch request for cPid:(%{public}d), pid(%{public}d), " \
        "tid:(%{public}d).", currentPid, pid, tid);
    if (pid == currentPid) {
        ret = DoDumpLocalLocked(pid, tid, msg, maxFrameNums);
    } else {
        if (maxFrameNums != DEFAULT_MAX_FRAME_NUM) {
            DFXLOGI("dump_catch :: maxFrameNums does not support setting " \
                "when pid is not equal to caller pid");
        }
        int timeout = (tid == 0 ? 3 : 10) * 1000; // when tid not zero, timeout is 10s
        int32_t res = DoDumpRemoteLocked(pid, tid, msg, isJson, timeout);
        if (res != DUMPCATCH_ESUCCESS && stack_.first != KernelStackAsyncCollector::STACK_SUCCESS) {
            res = KernelRet2DumpcatchRet(stack_.first);
        }
        void* retAddr = __builtin_return_address(0);
        ReportDumpCatcherStats(pid, requestTime, res, retAddr);
        ret = res == DUMPCATCH_ESUCCESS;
    }

    DFXLOGI("dump_catch : pid = %{public}d, elapsed time = %{public}" PRId64 " ms, ret = %{public}d, " \
        "msgLength = %{public}zu",
        pid, counter.Elapsed<std::chrono::milliseconds>(), ret, msg.size());
    DfxEnableTraceDlsym(false);
    return ret;
}

bool DfxDumpCatcher::Impl::DumpCatchFd(int pid, int tid, std::string& msg, int fd, size_t maxFrameNums)
{
    bool ret = false;
    ret = DumpCatch(pid, tid, msg, maxFrameNums, false);
    if (fd > 0) {
        ret = OHOS_TEMP_FAILURE_RETRY(write(fd, msg.c_str(), msg.length()));
    }
    return ret;
}

int32_t DfxDumpCatcher::Impl::DoDumpCatchRemote(int pid, int tid, std::string& msg, bool isJson, int timeout)
{
    DFX_TRACE_SCOPED_DLSYM("DoDumpCatchRemote");
    int32_t ret = DUMPCATCH_UNKNOWN;
    if (pid <= 0 || tid < 0 || timeout <= WAIT_GET_KERNEL_STACK_TIMEOUT) {
        msg.append("Result: pid(" + std::to_string(pid) + ") param error.\n");
        DFXLOGW("%{public}s :: %{public}s", __func__, msg.c_str());
        return DUMPCATCH_EPARAM;
    }
    pid_ = pid;
    int pipeReadFd[] = { -1, -1 };
    uint64_t sdkDumpStartTime = GetAbsTimeMilliSeconds();
    int sdkdumpRet = RequestSdkDump(pid, tid, pipeReadFd, isJson, timeout);
    if (sdkdumpRet != ResponseCode::REQUEST_SUCCESS) {
        DealWithSdkDumpRet(sdkdumpRet, pid, ret, msg);
        return ret;
    }
    // timeout sub the cost time of sdkdump
    timeout -= static_cast<int>(GetAbsTimeMilliSeconds() - sdkDumpStartTime);
    int pollRet = DoDumpRemotePid(pid, msg, pipeReadFd, isJson, timeout);
    DealWithPollRet(pollRet, pid, ret, msg);
    DFXLOGI("%{public}s :: pid(%{public}d) ret: %{public}d", __func__, pid, ret);
    return ret;
}

int DfxDumpCatcher::Impl::DoDumpRemotePid(int pid, std::string& msg, int (&pipeReadFd)[2], bool isJson, int32_t timeout)
{
    DFX_TRACE_SCOPED_DLSYM("DoDumpRemotePid");
    if (timeout <= 0) {
        DFXLOGW("timeout less than 0, try to get kernel stack and return directly!");
        stack_ = stackKit_.GetProcessStackWithTimeout(pid, WAIT_GET_KERNEL_STACK_TIMEOUT);
        RequestDelPipeFd(pid);
        CloseFd(pipeReadFd[PIPE_BUF_INDEX]);
        CloseFd(pipeReadFd[PIPE_RES_INDEX]);
        return DUMP_POLL_TIMEOUT;
    } else if (timeout < 1000) { // 1000 : one thousand milliseconds
        DFXLOGW("timeout less than 1 seconds, get kernel stack directly!");
        notifyCollect_ = stackKit_.NotifyStartCollect(pid);
    }
    int ret = DoDumpRemotePoll(timeout, msg, pipeReadFd, isJson);
    // request close fds in faultloggerd
    RequestDelPipeFd(pid);
    CloseFd(pipeReadFd[PIPE_BUF_INDEX]);
    CloseFd(pipeReadFd[PIPE_RES_INDEX]);
    DFXLOGI("%{public}s :: pid(%{public}d) poll ret: %{public}d", __func__, pid, ret);
    return ret;
}

int32_t DfxDumpCatcher::Impl::KernelRet2DumpcatchRet(int32_t ret)
{
    switch (ret) {
        case KernelStackAsyncCollector::STACK_ECREATE:
             return DUMPCATCH_KERNELSTACK_ECREATE;
        case KernelStackAsyncCollector::STACK_EOPEN:
             return DUMPCATCH_KERNELSTACK_EOPEN;
        case KernelStackAsyncCollector::STACK_EIOCTL:
             return DUMPCATCH_KERNELSTACK_EIOCTL;
        case KernelStackAsyncCollector::STACK_TIMEOUT:
            return DUMPCATCH_KERNELSTACK_TIMEOUT;
        case KernelStackAsyncCollector::STACK_OVER_LIMIT:
            return DUMPCATCH_KERNELSTACK_OVER_LIMITL;
        default:
            return DUMPCATCH_UNKNOWN;
    }
}

bool DfxDumpCatcher::Impl::HandlePollError(const uint64_t endTime, int& remainTime, std::string& resMsg, int& ret)
{
    if (errno == EINTR) {
        uint64_t now = GetAbsTimeMilliSeconds();
        if (now >= endTime) {
            ret = DUMP_POLL_TIMEOUT;
            resMsg.append("Result: poll timeout.\n");
            return false;
        }
        if (!notifyCollect_ && (remainTime == DUMPCATCHER_REMOTE_P90_TIMEOUT)) {
            notifyCollect_ = stackKit_.NotifyStartCollect(pid_);
        }
        remainTime = static_cast<int>(endTime - now);
        return true;
    }
    ret = DUMP_POLL_FAILED;
    resMsg.append("Result: poll error, errno(" + std::to_string(errno) + ")\n");
    return false;
}

bool DfxDumpCatcher::Impl::HandlePollTimeout(const int timeout, int& remainTime, std::string& resMsg, int& ret)
{
    if (!notifyCollect_ && (remainTime == DUMPCATCHER_REMOTE_P90_TIMEOUT)) {
        notifyCollect_ = stackKit_.NotifyStartCollect(pid_);
        remainTime = timeout - DUMPCATCHER_REMOTE_P90_TIMEOUT;
        return true;
    }
    ret = DUMP_POLL_TIMEOUT;
    resMsg.append("Result: poll timeout.\n");
    return false;
}

bool DfxDumpCatcher::Impl::HandlePollEvents(std::pair<int, std::string>& bufState,
    std::pair<int, std::string>& resState, const struct pollfd (&readFds)[2], bool& bPipeConnect, bool& res)
{
    bool bufRet = true;
    bool resRet = false;
    bool eventRet = true;
    for (auto& readFd : readFds) {
        if (!bPipeConnect && (static_cast<uint32_t>(readFd.revents) & POLLIN)) {
            bPipeConnect = true;
        }

        if (bPipeConnect &&
            ((static_cast<uint32_t>(readFd.revents) & POLLERR) || (static_cast<uint32_t>(readFd.revents) & POLLHUP))) {
            eventRet = false;
            resState.second.append("Result: poll events error.\n");
            break;
        }

        if ((static_cast<uint32_t>(readFd.revents) & POLLIN) != POLLIN) {
            continue;
        }

        if (readFd.fd == bufState.first) {
            bufRet = DoReadBuf(bufState.first, bufState.second);
        } else if (readFd.fd == resState.first) {
            resRet = DoReadRes(resState.first, res, resState.second);
        }
    }

    if ((eventRet == false) || (bufRet == false) || (resRet == true)) {
        DFXLOGI("eventRet:%{public}d bufRet:%{public}d resRet:%{public}d", eventRet, bufRet, resRet);
        return false;
    }
    return true;
}

std::pair<bool, int> DfxDumpCatcher::Impl::DumpRemotePoll(const int timeout,
    std::pair<int, std::string>& bufState, std::pair<int, std::string>& resState)
{
    int ret = DUMP_POLL_INIT;
    bool res = false;
    struct pollfd readFds[2];
    (void)memset_s(readFds, sizeof(readFds), 0, sizeof(readFds));
    readFds[0].fd = bufState.first;
    readFds[0].events = POLLIN;
    readFds[1].fd = resState.first;
    readFds[1].events = POLLIN;
    int fdsSize = sizeof(readFds) / sizeof(readFds[0]);
    bool bPipeConnect = false;
    int remainTime = DUMPCATCHER_REMOTE_P90_TIMEOUT < timeout ? DUMPCATCHER_REMOTE_P90_TIMEOUT : timeout;
    uint64_t startTime = GetAbsTimeMilliSeconds();
    uint64_t endTime = startTime + static_cast<uint64_t>(timeout);
    bool isContinue = true;
    do {
        int pollRet = poll(readFds, fdsSize, remainTime);
        if (pollRet < 0) {
            isContinue = HandlePollError(endTime, remainTime, resState.second, ret);
            continue;
        } else if (pollRet == 0) {
            isContinue = HandlePollTimeout(timeout, remainTime, resState.second, ret);
            continue;
        }
        if (!HandlePollEvents(bufState, resState, readFds, bPipeConnect, res)) {
            ret = DUMP_POLL_RETURN;
            break;
        }
        uint64_t now = GetAbsTimeMilliSeconds();
        if (now >= endTime) {
            ret = DUMP_POLL_TIMEOUT;
            resState.second.append("Result: poll timeout.\n");
            break;
        }
        remainTime = static_cast<int>(endTime - now);
    } while (isContinue);
    return std::make_pair(res, ret);
}

int DfxDumpCatcher::Impl::DoDumpRemotePoll(int timeout, std::string& msg, const int (&pipeReadFd)[2], bool isJson)
{
    DFX_TRACE_SCOPED_DLSYM("DoDumpRemotePoll");
    if (pipeReadFd[PIPE_BUF_INDEX] < 0 || pipeReadFd[PIPE_RES_INDEX] < 0) {
        if (!isJson) {
            msg = "Result: bufFd or resFd < 0.\n";
        }
        DFXLOGE("invalid bufFd or resFd");
        return DUMP_POLL_FD;
    }
    std::pair<int, std::string> bufState = std::make_pair(pipeReadFd[PIPE_BUF_INDEX], "");
    std::pair<int, std::string> resState = std::make_pair(pipeReadFd[PIPE_RES_INDEX], "");
    std::pair<bool, int> result = DumpRemotePoll(timeout, bufState, resState);

    DFXLOGI("%{public}s :: %{public}s", __func__, resState.second.c_str());
    msg = isJson && result.first ? bufState.second : (resState.second + bufState.second);
    return result.first ? DUMP_POLL_OK : result.second;
}

bool DfxDumpCatcher::Impl::DoReadBuf(int fd, std::string& msg)
{
    bool ret = false;
    char *buffer = new char[MAX_PIPE_SIZE];
    do {
        ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(fd, buffer, MAX_PIPE_SIZE));
        if (nread <= 0) {
            DFXLOGW("%{public}s :: read error", __func__);
            break;
        }
        DFXLOGD("%{public}s :: nread: %{public}zu", __func__, nread);
        ret = true;
        msg.append(buffer);
    } while (false);
    delete []buffer;
    return ret;
}

bool DfxDumpCatcher::Impl::DoReadRes(int fd, bool& ret, std::string& msg)
{
    int32_t res = DumpErrorCode::DUMP_ESUCCESS;
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(fd, &res, sizeof(res)));
    if (nread <= 0 || nread != sizeof(res)) {
        DFXLOGW("%{public}s :: read error", __func__);
        return false;
    }
    if (res == DumpErrorCode::DUMP_ESUCCESS) {
        ret = true;
    }
    msg.append("Result: " + DfxDumpRes::ToString(res) + "\n");
    return true;
}

bool DfxDumpCatcher::Impl::DumpCatchMultiPid(const std::vector<int> &pids, std::string& msg)
{
    bool ret = false;
    int pidSize = (int)pids.size();
    if (pidSize <= 0) {
        DFXLOGE("%{public}s :: param error, pidSize(%{public}d).", __func__, pidSize);
        return ret;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    int currentPid = getpid();
    int currentTid = gettid();
    DFXLOGD("%{public}s :: cPid(%{public}d), cTid(%{public}d), pidSize(%{public}d).",
        __func__, currentPid, currentTid, pidSize);

    time_t startTime = time(nullptr);
    if (startTime > 0) {
        DFXLOGD("%{public}s :: startTime(%{public}" PRId64 ").", __func__, startTime);
    }

    for (int i = 0; i < pidSize; i++) {
        int pid = pids[i];
        std::string pidStr;
        bool ret = DoDumpRemoteLocked(pid, 0, pidStr) == DUMPCATCH_ESUCCESS;
        if (ret) {
            msg.append(pidStr + "\n");
        } else {
            msg.append("Failed to dump process:" + std::to_string(pid));
        }

        time_t currentTime = time(nullptr);
        if (currentTime > 0) {
            DFXLOGD("%{public}s :: startTime(%{public}" PRId64 "), currentTime(%{public}" PRId64 ").",
                __func__, startTime, currentTime);
            if (currentTime > startTime + DUMP_CATCHE_WORK_TIME_S) {
                break;
            }
        }
    }

    DFXLOGD("%{public}s :: msg(%{public}s).", __func__, msg.c_str());
    if (msg.find("Tid:") != std::string::npos) {
        ret = true;
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
