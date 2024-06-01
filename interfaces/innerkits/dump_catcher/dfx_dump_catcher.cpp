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

#include <cerrno>
#include <memory>
#include <vector>

#include <dlfcn.h>
#include <poll.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <securec.h>
#include <strings.h>

#include "backtrace_local.h"
#include "dfx_define.h"
#include "dfx_dump_res.h"
#include "dfx_kernel_stack.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "file_ex.h"
#include "procinfo.h"

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
static const std::string DFXDUMPCATCHER_TAG = "DfxDumpCatcher";
enum DfxDumpPollRes : int32_t {
    DUMP_POLL_INIT = -1,
    DUMP_POLL_OK,
    DUMP_POLL_FD,
    DUMP_POLL_FAILED,
    DUMP_POLL_TIMEOUT,
    DUMP_POLL_RETURN,
};
}

bool DfxDumpCatcher::DoDumpCurrTid(const size_t skipFrameNum, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;

    ret = GetBacktrace(msg, skipFrameNum + 1, false, maxFrameNums);
    if (!ret) {
        int currTid = getproctid();
        msg.append("Failed to dump curr thread:" + std::to_string(currTid) + ".\n");
    }
    DFXLOG_DEBUG("%s :: DoDumpCurrTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalTid(const int tid, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;
    if (tid <= 0) {
        DFXLOG_ERROR("%s :: DoDumpLocalTid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }
    ret = GetBacktraceStringByTid(msg, tid, 0, false, maxFrameNums);
    if (!ret) {
        msg.append("Failed to dump thread:" + std::to_string(tid) + ".\n");
    }
    DFXLOG_DEBUG("%s :: DoDumpLocalTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalPid(int pid, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;
    if (pid <= 0) {
        DFXLOG_ERROR("%s :: DoDumpLocalPid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }
    size_t skipFramNum = 3; // 3: skip 3 frame

    std::function<bool(int)> func = [&](int tid) {
        if (tid <= 0) {
            return false;
        }

        if (tid == getproctid()) {
            return DoDumpCurrTid(skipFramNum, msg, maxFrameNums);
        }
        return DoDumpLocalTid(tid, msg, maxFrameNums);
    };
    std::vector<int> tids;
#if defined(is_ohos) && is_ohos
    ret = GetTidsByPidWithFunc(getprocpid(), tids, func);
#else
    ret = GetTidsByPidWithFunc(getpid(), tids, func);
#endif
    DFXLOG_DEBUG("%s :: DoDumpLocalPid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpRemoteLocked(int pid, int tid, std::string& msg, bool isJson)
{
    return DoDumpCatchRemote(pid, tid, msg, isJson);
}

bool DfxDumpCatcher::DoDumpLocalLocked(int pid, int tid, std::string& msg, size_t maxFrameNums)
{
    bool ret = false;
    if (tid == getproctid()) {
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

    DFXLOG_DEBUG("%s :: DoDumpLocal :: ret(%d).", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DumpCatchMix(int pid, int tid, std::string& msg)
{
    return DoDumpCatchRemote(pid, tid, msg);
}

static void ReportDumpCatcherStats(int32_t pid,
    uint64_t requestTime, bool ret, std::string& msg, void* retAddr)
{
    std::vector<uint8_t> buf(sizeof(struct FaultLoggerdStatsRequest), 0);
    auto stat = reinterpret_cast<struct FaultLoggerdStatsRequest*>(buf.data());
    stat->type = DUMP_CATCHER;
    stat->pid = pid;
    stat->requestTime = requestTime;
    stat->dumpCatcherFinishTime = GetTimeMilliSeconds();
    stat->result = ret ? 0 : -1; // we need more detailed failure info
    size_t copyLen;
    if (!ret) {
        copyLen = std::min(sizeof(stat->summary) - 1, msg.size());
        if (memcpy_s(stat->summary, sizeof(stat->summary) - 1, msg.c_str(), copyLen) != 0) {
            DFXLOG_ERROR("%s::Failed to copy dumpcatcher summary", DFXDUMPCATCHER_TAG.c_str());
            return;
        }
    }

    Dl_info info;
    if (dladdr(retAddr, &info) != 0) {
        copyLen = std::min(sizeof(stat->callerElf) - 1, strlen(info.dli_fname));
        if (memcpy_s(stat->callerElf, sizeof(stat->callerElf) - 1, info.dli_fname, copyLen) != 0) {
            DFXLOG_ERROR("%s::Failed to copy caller elf info", DFXDUMPCATCHER_TAG.c_str());
            return;
        }
        stat->offset = reinterpret_cast<uintptr_t>(retAddr) - reinterpret_cast<uintptr_t>(info.dli_fbase);
    }

    std::string cmdline;
    if (OHOS::LoadStringFromFile("/proc/self/cmdline", cmdline)) {
        copyLen = std::min(sizeof(stat->callerProcess) - 1, cmdline.size());
        if (memcpy_s(stat->callerProcess, sizeof(stat->callerProcess) - 1,
            cmdline.c_str(), copyLen) != 0) {
            DFXLOG_ERROR("%s::Failed to copy caller cmdline", DFXDUMPCATCHER_TAG.c_str());
            return;
        }
    }

    ReportDumpStats(stat);
}

bool DfxDumpCatcher::DumpCatch(int pid, int tid, std::string& msg, size_t maxFrameNums, bool isJson)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        DFXLOG_ERROR("%s :: dump_catch :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    int currentPid = getprocpid();
    bool reportStat = false;
    uint64_t requestTime = GetTimeMilliSeconds();
    DFXLOG_INFO("Receive DumpCatch request for cPid:(%d), pid(%d), tid:(%d).", currentPid, pid, tid);
    if (pid == currentPid) {
        ret = DoDumpLocalLocked(pid, tid, msg, maxFrameNums);
    } else {
        if (maxFrameNums != DEFAULT_MAX_FRAME_NUM) {
            DFXLOG_INFO("%s :: dump_catch :: maxFrameNums does not support setting when pid is not equal to caller pid",
                DFXDUMPCATCHER_TAG.c_str());
        }
        reportStat = true;
        ret = DoDumpRemoteLocked(pid, tid, msg, isJson);
    }

    if (ret && isJson && !IsValidJson(msg)) {
        DFXLOG_INFO("%s :: dump_catch :: json stack info is invalid, try to dump stack again.",
            DFXDUMPCATCHER_TAG.c_str());
        msg.clear();
        if (pid == currentPid) {
            ret = DoDumpLocalLocked(pid, tid, msg, maxFrameNums);
        } else {
            reportStat = true;
            ret = DoDumpRemoteLocked(pid, tid, msg, false);
        }
    }

    if (reportStat) {
        void* retAddr = __builtin_return_address(0);
        ReportDumpCatcherStats(pid, requestTime, ret, msg, retAddr);
    }

    DFXLOG_INFO("%s :: dump_catch :: msgLength: %zu",  DFXDUMPCATCHER_TAG.c_str(), msg.size());
    DFXLOG_DEBUG("%s :: dump_catch :: ret: %d, msg: %s", DFXDUMPCATCHER_TAG.c_str(), ret, msg.c_str());
    return ret;
}

bool DfxDumpCatcher::DumpCatchFd(int pid, int tid, std::string& msg, int fd, size_t maxFrameNums)
{
    bool ret = false;
    ret = DumpCatch(pid, tid, msg, maxFrameNums);
    if (fd > 0) {
        ret = write(fd, msg.c_str(), msg.length());
    }
    return ret;
}

bool DfxDumpCatcher::DoDumpCatchRemote(int pid, int tid, std::string& msg, bool isJson)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        msg.append("Result: pid(" + std::to_string(pid) + ") param error.\n");
        DFXLOG_WARN("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
        return ret;
    }
    int sdkdumpRet = RequestSdkDumpJson(pid, tid, isJson);
    if (sdkdumpRet != static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_PASS)) {
        if (sdkdumpRet == static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_REPEAT)) {
            msg.append("Result: pid(" + std::to_string(pid) + ") is dumping.\n");
        } else if (sdkdumpRet == static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_REJECT)) {
            msg.append("Result: pid(" + std::to_string(pid) + ") check permission error.\n");
        } else if (sdkdumpRet == static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_NOPROC)) {
            msg.append("Result: pid(" + std::to_string(pid) + ") syscall SIGDUMP error.\n");
            msg.append(GetAllTidKernelStack(pid));
            RequestDelPipeFd(pid);
        } else if (sdkdumpRet == static_cast<int>(FaultLoggerSdkDumpResp::SDK_PROCESS_CRASHED)) {
            msg.append("Result: pid(" + std::to_string(pid) + ") has been crashed.\n");
        }
        DFXLOG_WARN("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
        return ret;
    }

    int pollRet = DoDumpRemotePid(pid, msg, isJson);
    switch (pollRet) {
        case DUMP_POLL_OK:
            ret = true;
            break;
        case DUMP_POLL_TIMEOUT: {
            msg.append(halfProcStatus_);
            msg.append(halfProcWchan_);
            msg.append(halfKernelStack_);
            break;
        }
        default:
            break;
    }

    if (!ret && isJson && msg != "") {
        DFXLOG_INFO("%s :: %s json msg not empty!", DFXDUMPCATCHER_TAG.c_str(), __func__);
        ret = true;
    }
    DFXLOG_INFO("%s :: %s :: pid(%d) ret: %d", DFXDUMPCATCHER_TAG.c_str(), __func__, pid, ret);
    return ret;
}

int DfxDumpCatcher::DoDumpRemotePid(int pid, std::string& msg, bool isJson, int32_t timeout)
{
    int readBufFd = -1;
    int readResFd = -1;
    pid_ = pid;
    if (isJson) {
        readBufFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_JSON_READ_BUF);
        readResFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_JSON_READ_RES);
    } else {
        readBufFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_READ_BUF);
        readResFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_READ_RES);
    }
    DFXLOG_DEBUG("read res fd: %d", readResFd);
    int ret = DoDumpRemotePoll(readBufFd, readResFd, timeout, msg, isJson);
    // request close fds in faultloggerd
    RequestDelPipeFd(pid);
    if (readBufFd >= 0) {
        close(readBufFd);
        readBufFd = -1;
    }
    if (readResFd >= 0) {
        close(readResFd);
        readResFd = -1;
    }
    DFXLOG_INFO("%s :: %s :: pid(%d) poll ret: %d", DFXDUMPCATCHER_TAG.c_str(), __func__, pid, ret);
    return ret;
}

void DfxDumpCatcher::CollectKernelInfo()
{
    DFXLOG_INFO("start collect kernel info for pid(%d) ", pid_);
    DfxGetKernelStack(pid_, halfKernelStack_);
    DFXLOG_INFO("finish collect kernel info for pid(%d) ", pid_);
    ReadProcessStatus(halfProcStatus_, pid_);
    ReadProcessWchan(halfProcWchan_, pid_, false, true);
}

std::string DfxDumpCatcher::GetAllTidKernelStack(int pid)
{
    std::string kernelStackInfo;
    std::string statusPath = StringPrintf("/proc/%d/status", pid);
    if (access(statusPath.c_str(), F_OK) != 0) {
        DFXLOG_WARN("No process(%d) status file exist!", pid_);
        return kernelStackInfo;
    }
    std::vector<int> tids = {};
    std::vector<int> nstids = {};
    if (GetTidsByPid(pid, tids, nstids) == false) {
        DFXLOG_ERROR("Process(%d) Get Tids fail!", pid_);
        return kernelStackInfo;
    }
    for (int tid : tids) {
        std::string tidKernelStackInfo;
        if (DfxGetKernelStack(tid, tidKernelStackInfo) != 0) {
            DFXLOG_ERROR("tid(%d) Get kernel stack fail!", tid);
            continue;
        }
        kernelStackInfo.append(tidKernelStackInfo);
    }
    return kernelStackInfo;
}

int DfxDumpCatcher::DoDumpRemotePoll(int bufFd, int resFd, int timeout, std::string& msg, bool isJson)
{
    if (bufFd < 0 || resFd < 0) {
        if (!isJson) {
            msg = "Result: bufFd or resFd < 0.\n";
        }
        DFXLOG_ERROR("%s", "invalid bufFd or resFd");
        return DUMP_POLL_FD;
    }
    int ret = DUMP_POLL_INIT;
    std::string resMsg;
    bool res = false;
    std::string bufMsg;
    struct pollfd readfds[2];
    (void)memset_s(readfds, sizeof(readfds), 0, sizeof(readfds));
    readfds[0].fd = bufFd;
    readfds[0].events = POLLIN;
    readfds[1].fd = resFd;
    readfds[1].events = POLLIN;
    int fdsSize = sizeof(readfds) / sizeof(readfds[0]);
    bool bPipeConnect = false;
    int remainTime = DUMPCATCHER_REMOTE_P90_TIMEOUT;
    bool collectKernelInfo = false;
    uint64_t startTime = GetAbsTimeMilliSeconds();
    uint64_t endTime = startTime + static_cast<uint64_t>(timeout);
    while (true) {
        int pollRet = poll(readfds, fdsSize, remainTime);
        if (pollRet < 0 && errno == EINTR) {
            uint64_t now = GetAbsTimeMilliSeconds();
            if (now >= endTime) {
                ret = DUMP_POLL_TIMEOUT;
                resMsg.append("Result: poll timeout.\n");
                break;
            }
            if (!collectKernelInfo && (remainTime == DUMPCATCHER_REMOTE_P90_TIMEOUT)) {
                CollectKernelInfo();
                collectKernelInfo = true;
            }
            remainTime = static_cast<int>(endTime - now);
            continue;
        } else if (pollRet < 0) {
            ret = DUMP_POLL_FAILED;
            resMsg.append("Result: poll error, errno(" + std::to_string(errno) + ")\n");
            break;
        } else if (pollRet == 0) {
            if (!collectKernelInfo && (remainTime == DUMPCATCHER_REMOTE_P90_TIMEOUT)) {
                CollectKernelInfo();
                remainTime = timeout - DUMPCATCHER_REMOTE_P90_TIMEOUT;
                collectKernelInfo = true;
                continue;
            }
            ret = DUMP_POLL_TIMEOUT;
            resMsg.append("Result: poll timeout.\n");
            break;
        }

        bool bufRet = true;
        bool resRet = false;
        bool eventRet = true;
        for (int i = 0; i < fdsSize; ++i) {
            if (!bPipeConnect && ((uint32_t)readfds[i].revents & POLLIN)) {
                bPipeConnect = true;
            }
            if (bPipeConnect &&
                (((uint32_t)readfds[i].revents & POLLERR) || ((uint32_t)readfds[i].revents & POLLHUP))) {
                eventRet = false;
                resMsg.append("Result: poll events error.\n");
                break;
            }

            if (((uint32_t)readfds[i].revents & POLLIN) != POLLIN) {
                continue;
            }

            if (readfds[i].fd == bufFd) {
                bufRet = DoReadBuf(bufFd, bufMsg);
                continue;
            }

            if (readfds[i].fd == resFd) {
                resRet = DoReadRes(resFd, res, resMsg);
            }
        }

        if ((eventRet == false) || (bufRet == false) || (resRet == true)) {
            DFXLOG_INFO("%s :: %s :: eventRet(%d) bufRet: %d resRet: %d", DFXDUMPCATCHER_TAG.c_str(), __func__,
                eventRet, bufRet, resRet);
            ret = DUMP_POLL_RETURN;
            break;
        }
        uint64_t now = GetAbsTimeMilliSeconds();
        if (now >= endTime) {
            ret = DUMP_POLL_TIMEOUT;
            resMsg.append("Result: poll timeout.\n");
            break;
        }
        remainTime = static_cast<int>(endTime - now);
    }

    DFXLOG_INFO("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, resMsg.c_str());
    msg = isJson ? bufMsg : (resMsg + bufMsg);
    return res ? DUMP_POLL_OK : ret;
}

bool DfxDumpCatcher::DoReadBuf(int fd, std::string& msg)
{
    bool ret = false;
    char *buffer = new char[MAX_PIPE_SIZE];
    do {
        ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(fd, buffer, MAX_PIPE_SIZE));
        if (nread <= 0) {
            DFXLOG_WARN("%s :: %s :: read error", DFXDUMPCATCHER_TAG.c_str(), __func__);
            break;
        }
        DFXLOG_INFO("%s :: %s :: nread: %zu", DFXDUMPCATCHER_TAG.c_str(), __func__, nread);
        ret = true;
        msg.append(buffer);
    } while (false);
    delete []buffer;
    return ret;
}

bool DfxDumpCatcher::DoReadRes(int fd, bool &ret, std::string& msg)
{
    int32_t res = DumpErrorCode::DUMP_ESUCCESS;
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(fd, &res, sizeof(res)));
    if (nread <= 0 || nread != sizeof(res)) {
        DFXLOG_WARN("%s :: %s :: read error", DFXDUMPCATCHER_TAG.c_str(), __func__);
        return false;
    }
    if (res == DumpErrorCode::DUMP_ESUCCESS) {
        ret = true;
    }
    msg.append("Result: " + DfxDumpRes::ToString(res) + "\n");
    return true;
}

bool DfxDumpCatcher::DumpCatchMultiPid(const std::vector<int> pidV, std::string& msg)
{
    bool ret = false;
    int pidSize = (int)pidV.size();
    if (pidSize <= 0) {
        DFXLOG_ERROR("%s :: %s :: param error, pidSize(%d).", DFXDUMPCATCHER_TAG.c_str(), __func__, pidSize);
        return ret;
    }

    std::unique_lock<std::mutex> lck(mutex_);
#if defined(is_ohos) && is_ohos
    int currentPid = getprocpid();
    int currentTid = getproctid();
#else
    int currentPid = getpid();
    int currentTid = gettid();
#endif
    DFXLOG_DEBUG("%s :: %s :: cPid(%d), cTid(%d), pidSize(%d).", DFXDUMPCATCHER_TAG.c_str(), \
        __func__, currentPid, currentTid, pidSize);

    time_t startTime = time(nullptr);
    if (startTime > 0) {
        DFXLOG_DEBUG("%s :: %s :: startTime(%" PRId64 ").", DFXDUMPCATCHER_TAG.c_str(), __func__, startTime);
    }

    for (int i = 0; i < pidSize; i++) {
        int pid = pidV[i];
        std::string pidStr;
        if (DoDumpRemoteLocked(pid, 0, pidStr)) {
            msg.append(pidStr + "\n");
        } else {
            msg.append("Failed to dump process:" + std::to_string(pid));
        }

        time_t currentTime = time(nullptr);
        if (currentTime > 0) {
            DFXLOG_DEBUG("%s :: %s :: startTime(%" PRId64 "), currentTime(%" PRId64 ").", DFXDUMPCATCHER_TAG.c_str(), \
                __func__, startTime, currentTime);
            if (currentTime > startTime + DUMP_CATCHE_WORK_TIME_S) {
                break;
            }
        }
    }

    DFXLOG_DEBUG("%s :: %s :: msg(%s).", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
    if (msg.find("Tid:") != std::string::npos) {
        ret = true;
    }
    return ret;
}

bool DfxDumpCatcher::IsValidJson(const std::string& json)
{
    int squareBrackets = 0;
    int braces = 0;
    for (const auto& ch : json) {
        switch (ch) {
            case '[':
                squareBrackets++;
                break;
            case ']':
                squareBrackets--;
                break;
            case '{':
                braces++;
                break;
            case '}':
                braces--;
                break;
            default:
                break;
        }
    }
    return squareBrackets == 0 && braces == 0;
}
} // namespace HiviewDFX
} // namespace OHOS
