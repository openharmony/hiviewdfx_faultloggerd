/*
 * Copyright (c) 2021-2026 Huawei Device Co., Ltd.
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

#include <unistd.h>
#include <vector>

#include "dfx_define.h"
#include "dfx_dump_catcher_errno.h"
#include "dfx_log.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#include "dump_catcher_constants.h"
#include "dump_catcher_shared_state.h"
#include "elapsed_time.h"
#include "file_ex.h"
#include "kernel_stack_async_collector.h"
#include "local_dumper.h"
#include "remote_dumper.h"
#include "result_handler.h"
#include "string_printf.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxDumpCatcher"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
bool IsLinuxKernel()
{
    static bool isLinux = [] {
        std::string content;
        LoadStringFromFile("/proc/version", content);
        if (content.empty()) {
            return true;
        }
        return content.find("Linux") != std::string::npos;
    }();
    return isLinux;
}
}

class DfxDumpCatcher::Impl {
public:
    Impl() : remoteDumper_(state_), resultHandler_(state_) {}

    bool DumpCatch(int pid, int tid, std::string& msg, size_t maxFrameNums, bool isJson);
    bool DumpCatchFd(int pid, int tid, std::string& msg, int fd, size_t maxFrameNums);
    bool DumpCatchMultiPid(const std::vector<int>& pids, std::string& msg);
    std::pair<int, std::string> DumpCatchWithTimeout(int pid, std::string& msg,
        int timeout, int tid, bool isJson);

private:
    bool DoRemoteDump(int pid, int tid, std::string& msg, bool isJson,
        size_t maxFrameNums, uint64_t requestTime);
    bool CheckProcessExited(int pid, std::string& msg);

    DumpCatcherSharedState state_;
    LocalDumper localDumper_;
    RemoteDumper remoteDumper_;
    ResultHandler resultHandler_;
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

bool DfxDumpCatcher::DumpCatchMultiPid(const std::vector<int>& pids, std::string& msg)
{
    return impl_->DumpCatchMultiPid(pids, msg);
}

std::pair<int, std::string> DfxDumpCatcher::DumpCatchWithTimeout(int pid, std::string& msg,
    int timeout, int tid, bool isJson)
{
    return impl_->DumpCatchWithTimeout(pid, msg, timeout, tid, isJson);
}

bool DfxDumpCatcher::Impl::CheckProcessExited(int pid, std::string& msg)
{
    if (IsLinuxKernel()) {
        return true;
    }
    std::string statusPath = StringPrintf("/proc/%d/status", pid);
    if (access(statusPath.c_str(), F_OK) != 0 && errno != EACCES) {
        DFXLOGE("the pid(%{public}d) process has exited, errno(%{public}d)", pid, errno);
        msg.append("Result: pid(" + std::to_string(pid) + ") process has exited.\n");
        return false;
    }
    return true;
}

bool DfxDumpCatcher::Impl::DoRemoteDump(int pid, int tid, std::string& msg, bool isJson,
    size_t maxFrameNums, uint64_t requestTime)
{
    if (maxFrameNums != DEFAULT_MAX_FRAME_NUM) {
        DFXLOGI("dump_catch :: maxFrameNums does not support setting when pid is not equal to caller pid");
    }
    int timeout = (tid == 0) ? DumpCatcherConstants::DUMP_CATCH_TID_ZERO_TIMEOUT_MS
                             : DumpCatcherConstants::DUMP_CATCH_TID_NONZERO_TIMEOUT_MS;
    int32_t res = remoteDumper_.DoDumpRemoteLocked(pid, tid, msg, isJson, timeout);
    bool ret = (res == DUMPCATCH_ESUCCESS || res == DUMPCATCH_DUMP_ESYMBOL_NO_PARSE ||
        res == DUMPCATCH_DUMP_ESYMBOL_PARSE_TIMEOUT);
    if (!ret && state_.stack.errorCode != KernelStackAsyncCollector::STACK_SUCCESS) {
        res = ResultHandler::KernelRet2DumpcatchRet(state_.stack.errorCode);
    }
    void* retAddr = __builtin_return_address(0);
    remoteDumper_.ReportStats(pid, requestTime, res, retAddr);
    return ret;
}

bool DfxDumpCatcher::Impl::DumpCatch(int pid, int tid, std::string& msg, size_t maxFrameNums, bool isJson)
{
    if (pid <= 0 || tid < 0) {
        DFXLOGE("dump_catch :: param error.");
        return false;
    }
    if (!CheckProcessExited(pid, msg)) {
        return false;
    }
    ElapsedTime counter;
    std::unique_lock<std::mutex> lck(state_.mutex);
    DfxEnableTraceDlsym(true);
    state_.stack = {};
    state_.notifyCollect = false;
    int currentPid = getpid();
    uint64_t requestTime = GetTimeMilliSeconds();
    DFXLOGI("Receive DumpCatch request for cPid:(%{public}d), pid(%{public}d), tid:(%{public}d).",
        currentPid, pid, tid);
    bool ret = false;
    if (pid == currentPid) {
        ret = localDumper_.DumpLocalLocked(pid, tid, msg, maxFrameNums);
    } else {
        ret = DoRemoteDump(pid, tid, msg, isJson, maxFrameNums, requestTime);
    }
    DFXLOGI("dump_catch : pid = %{public}d, elapsed time = %{public}" PRId64 " ms, ret = %{public}d, " \
        "msgLength = %{public}zu", pid, counter.Elapsed<std::chrono::milliseconds>(), ret, msg.size());
    DfxEnableTraceDlsym(false);
    return ret;
}

bool DfxDumpCatcher::Impl::DumpCatchFd(int pid, int tid, std::string& msg, int fd, size_t maxFrameNums)
{
    bool ret = DumpCatch(pid, tid, msg, maxFrameNums, false);
    if (fd > 0) {
        ssize_t writeRet = OHOS_TEMP_FAILURE_RETRY(write(fd, msg.c_str(), msg.length()));
        ret = writeRet > 0;
    }
    return ret;
}

std::pair<int, std::string> DfxDumpCatcher::Impl::DumpCatchWithTimeout(int pid, std::string& msg,
    int timeout, int tid, bool isJson)
{
    std::unique_lock<std::mutex> lck(state_.mutex);
    DfxEnableTraceDlsym(true);
    ElapsedTime counter;
    uint64_t requestTime = GetTimeMilliSeconds();
    int32_t dumpcatchErrno = DUMPCATCH_UNKNOWN;
    bool reportStat = false;
    do {
        if (pid <= 0 || tid < 0 ||
            timeout <= DumpCatcherConstants::WAIT_KERNEL_STACK_TIMEOUT_MS) {
            DFXLOGE("DumpCatchWithTimeout:: param error.");
            dumpcatchErrno = DUMPCATCH_EPARAM;
            break;
        }
        if (!CheckProcessExited(pid, msg)) {
            dumpcatchErrno = DUMPCATCH_NO_PROCESS;
            break;
        }
        int currentPid = getpid();
        if (pid == currentPid) {
            bool ret = localDumper_.DumpLocalLocked(pid, tid, msg, DEFAULT_MAX_FRAME_NUM);
            dumpcatchErrno = ret ? DUMPCATCH_ESUCCESS : DUMPCATCH_DUMP_SELF_FAIL;
        } else {
            DFXLOGI("Receive DumpCatch request for cPid:(%{public}d), pid(%{public}d)", currentPid, pid);
            dumpcatchErrno = remoteDumper_.DoDumpRemoteLocked(pid, tid, msg, isJson, timeout);
            reportStat = true;
        }
    } while (false);

    auto result = resultHandler_.DealWithDumpCatchRet(pid, dumpcatchErrno, msg);
    if (reportStat) {
        void* retAddr = __builtin_return_address(0);
        remoteDumper_.ReportStats(pid, requestTime, dumpcatchErrno, retAddr);
    }
    DFXLOGI("dump_catch : pid = %{public}d, elapsed time = %{public}" PRId64 " ms, " \
        "msgLength = %{public}zu, ret = %{public}d\n%{public}s",
        pid, counter.Elapsed<std::chrono::milliseconds>(), msg.size(), result.first, result.second.c_str());
    DfxEnableTraceDlsym(false);
    return result;
}

bool DfxDumpCatcher::Impl::DumpCatchMultiPid(const std::vector<int>& pids, std::string& msg)
{
    int pidSize = static_cast<int>(pids.size());
    if (pidSize <= 0) {
        DFXLOGE("%{public}s :: param error, pidSize(%{public}d).", __func__, pidSize);
        return false;
    }
    std::unique_lock<std::mutex> lck(state_.mutex);
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
        bool dumpRet = remoteDumper_.DoDumpRemoteLocked(pid, 0, pidStr, false,
            DumpCatcherConstants::REMOTE_TIMEOUT_MS) == DUMPCATCH_ESUCCESS;
        if (dumpRet) {
            msg.append(pidStr + "\n");
        } else {
            msg.append("Failed to dump process:" + std::to_string(pid));
        }
        time_t currentTime = time(nullptr);
        if (currentTime <= 0) {
            DFXLOGE("%{public}s :: time() failed, breaking loop", __func__);
            break;
        }
        if (currentTime > startTime + DumpCatcherConstants::WORK_TIME_LIMIT_S) {
            break;
        }
    }
    DFXLOGD("%{public}s :: msg(%{public}s).", __func__, msg.c_str());
    return msg.find("Tid:") != std::string::npos;
}
} // namespace HiviewDFX
} // namespace OHOS
