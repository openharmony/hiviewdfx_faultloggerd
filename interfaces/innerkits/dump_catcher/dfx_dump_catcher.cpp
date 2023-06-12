/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files contains faultlog sdk interface functions. */

#include "dfx_dump_catcher.h"

#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <dirent.h>
#include <poll.h>
#include <unistd.h>

#include <sys/syscall.h>
#include <sys/types.h>

#include "dfx_define.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "file_util.h"
#include "securec.h"
#include "strings.h"
#include "libunwind.h"
#include "libunwind_i-ohos.h"

#include "backtrace_local.h"
#include "procinfo.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static const int DUMP_CATCHE_WORK_TIME_S = 60;
static const int BACK_TRACE_DUMP_MIX_TIMEOUT_MS = 2000;
static const int BACK_TRACE_DUMP_CPP_TIMEOUT_MS = 10000;
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

bool DfxDumpCatcher::DoDumpCurrTid(const size_t skipFrameNum, std::string& msg)
{
    bool ret = false;
    ret = GetBacktrace(msg, skipFrameNum + 1, false);
    if (!ret) {
        int currTid = syscall(SYS_gettid);
        msg.append("Failed to dump curr thread:" + std::to_string(currTid) + ".\n");
    }
    DFXLOG_DEBUG("%s :: DoDumpCurrTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalTid(const int tid, std::string& msg)
{
    bool ret = false;
    if (tid <= 0) {
        DFXLOG_ERROR("%s :: DoDumpLocalTid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    ret = GetBacktraceStringByTid(msg, tid, 0, false);
    if (!ret) {
        msg.append("Failed to dump thread:" + std::to_string(tid) + ".\n");
    }
    DFXLOG_DEBUG("%s :: DoDumpLocalTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalPid(int pid, std::string& msg)
{
    bool ret = false;
    if (pid <= 0) {
        DFXLOG_ERROR("%s :: DoDumpLocalPid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }
    size_t skipFramNum = DUMP_CATCHER_NUMBER_THREE;

    std::function<bool(int)> func = [&](int tid) {
        if (tid <= 0) {
            return false;
        }

        if (tid == gettid()) {
            return DoDumpCurrTid(skipFramNum, msg);
        }
        return DoDumpLocalTid(tid, msg);
    };
    std::vector<int> tids;
    ret = GetTidsByPidWithFunc(getpid(), tids, func);

    DFXLOG_DEBUG("%s :: DoDumpLocalPid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpRemoteLocked(int pid, int tid, std::string& msg)
{
    int type = DUMP_TYPE_NATIVE;
    return DoDumpCatchRemote(type, pid, tid, msg);
}

bool DfxDumpCatcher::DoDumpLocalLocked(int pid, int tid, std::string& msg)
{
    bool ret = false;
    size_t skipFramNum = DUMP_CATCHER_NUMBER_TWO;
    if (tid == syscall(SYS_gettid)) {
        ret = DoDumpCurrTid(skipFramNum, msg);
    } else if (tid == 0) {
        ret = DoDumpLocalPid(pid, msg);
    } else {
        if (!IsThreadInPid(pid, tid)) {
            msg.append("tid(" + std::to_string(tid) + ") is not in pid(" + std::to_string(pid) + ").\n");
        } else {
            ret = DoDumpLocalTid(tid, msg);
        }
    }

    DFXLOG_DEBUG("%s :: DoDumpLocal :: ret(%d).", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DumpCatchMix(int pid, int tid, std::string& msg)
{
    int type = DUMP_TYPE_MIX;
    return DoDumpCatchRemote(type, pid, tid, msg);
}

bool DfxDumpCatcher::DumpCatch(int pid, int tid, std::string& msg)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        DFXLOG_ERROR("%s :: dump_catch :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    int currentPid = getpid();
    DFXLOG_DEBUG("%s :: dump_catch :: cPid(%d), pid(%d), tid(%d).",
        DFXDUMPCATCHER_TAG.c_str(), currentPid, pid, tid);

    if (pid == currentPid) {
        ret = DoDumpLocalLocked(pid, tid, msg);
    } else {
        ret = DoDumpRemoteLocked(pid, tid, msg);
    }
    DFXLOG_DEBUG("%s :: dump_catch :: ret: %d, msg: %s", DFXDUMPCATCHER_TAG.c_str(), ret, msg.c_str());
    return ret;
}

bool DfxDumpCatcher::DumpCatchFd(int pid, int tid, std::string& msg, int fd)
{
    bool ret = false;
    ret = DumpCatch(pid, tid, msg);
    if (fd > 0) {
        ret = write(fd, msg.c_str(), msg.length());
    }
    return ret;
}

static void LoadPathContent(const std::string& desc, const std::string& path, std::string& result)
{
    if (access(path.c_str(), F_OK) != 0) {
        result.append("Target path(");
        result.append(path);
        result.append(") is not exist. errno(");
        result.append(std::to_string(errno));
        result.append(").\n");
        return;
    }

    std::string content;
    OHOS::HiviewDFX::LoadStringFromFile(path, content);
    if (!content.empty()) {
        std::string str = desc + ":\n" + content + "\n";
        result.append(str);
    }
    return;
}

static void LoadPidStat(const int pid, std::string& msg)
{
    std::string statusPath = "/proc/" + std::to_string(pid) + "/status";
    std::string wchanPath = "/proc/" + std::to_string(pid) + "/wchan";
    LoadPathContent("Process status", statusPath, msg);
    LoadPathContent("Process wchan", wchanPath, msg);
    msg.append("\nProcess wchan:\n=======================================\n");
    ReadProcessWchan(msg, pid, true);
    msg.append("=======================================\n");
}

bool DfxDumpCatcher::DoDumpCatchRemote(const int type, int pid, int tid, std::string& msg)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        msg.append("Result: pid(" + std::to_string(pid) + ") param error.\n");
        DFXLOG_WARN("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
        return ret;
    }

    int sdkdumpRet = RequestSdkDump(type, pid, tid);
    if (sdkdumpRet != static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_PASS)) {
        if (sdkdumpRet == static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_REPEAT)) {
            msg.append("Result: pid(" + std::to_string(pid) + ") is dumping.\n");
        } else if (sdkdumpRet == static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_REJECT)) {
            msg.append("Result: pid(" + std::to_string(pid) + ") check permission error.\n");
        } else if (sdkdumpRet == static_cast<int>(FaultLoggerSdkDumpResp::SDK_DUMP_NOPROC)) {
            msg.append("Result: pid(" + std::to_string(pid) + ") syscall SIGDUMP error.\n");
            RequestDelPipeFd(pid);
        }
        DFXLOG_WARN("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
        return ret;
    }

    int pollRet = DoDumpRemotePid(type, pid, msg);
    switch (pollRet) {
        case DUMP_POLL_OK:
            ret = true;
            break;
        case DUMP_POLL_TIMEOUT:
            if (type == DUMP_TYPE_MIX) {
                msg.append("Result: pid(" + std::to_string(pid) + ") dump mix timeout, try dump native frame.\n");
                int type = DUMP_TYPE_NATIVE;
                return DoDumpCatchRemote(type, pid, tid, msg);
            } else if (type == DUMP_TYPE_NATIVE) {
                LoadPidStat(pid, msg);
            }
            break;
        default:
            break;
    }
    DFXLOG_INFO("%s :: %s :: pid(%d) ret: %d", DFXDUMPCATCHER_TAG.c_str(), __func__, pid, ret);
    return ret;
}

int DfxDumpCatcher::DoDumpRemotePid(const int type, int pid, std::string& msg)
{
    int readBufFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_READ_BUF);
    DFXLOG_DEBUG("read buf fd: %d", readBufFd);

    int readResFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_READ_RES);
    DFXLOG_DEBUG("read res fd: %d", readResFd);

    int timeout = BACK_TRACE_DUMP_CPP_TIMEOUT_MS;
    if (type == DUMP_TYPE_MIX) {
        timeout = BACK_TRACE_DUMP_MIX_TIMEOUT_MS;
    }

    int ret = DoDumpRemotePoll(readBufFd, readResFd, timeout, msg);

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

int DfxDumpCatcher::DoDumpRemotePoll(int bufFd, int resFd, int timeout, std::string& msg)
{
    int ret = DUMP_POLL_INIT;
    bool res = false;
    std::string bufMsg;
    std::string resMsg;
    struct pollfd readfds[2];
    (void)memset_s(readfds, sizeof(readfds), 0, sizeof(readfds));
    readfds[0].fd = bufFd;
    readfds[0].events = POLLIN;
    readfds[1].fd = resFd;
    readfds[1].events = POLLIN;
    int fdsSize = sizeof(readfds) / sizeof(readfds[0]);
    bool bPipeConnect = false;
    while (true) {
        if (bufFd < 0 || resFd < 0) {
            ret = DUMP_POLL_FD;
            resMsg.append("Result: bufFd or resFd  < 0.\n");
            break;
        }

        int pollRet = poll(readfds, fdsSize, timeout);
        if (pollRet < 0) {
            ret = DUMP_POLL_FAILED;
            resMsg.append("Result: poll error, errno(" + std::to_string(errno) + ")\n");
            break;
        } else if (pollRet == 0) {
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
            }

            if (readfds[i].fd == resFd) {
                resRet = DoReadRes(resFd, res, resMsg);
            }
        }

        if ((eventRet == false) || (bufRet == false) || (resRet == true)) {
            ret = DUMP_POLL_RETURN;
            break;
        }
    }

    DFXLOG_INFO("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, resMsg.c_str());
    msg = resMsg + bufMsg;
    if (res) {
        ret = DUMP_POLL_OK;
    }
    return ret;
}

bool DfxDumpCatcher::DoReadBuf(int fd, std::string& msg)
{
    char buffer[LOG_BUF_LEN] = {0};
    ssize_t nread = read(fd, buffer, sizeof(buffer) - 1);
    if (nread <= 0) {
        DFXLOG_WARN("%s :: %s :: read error", DFXDUMPCATCHER_TAG.c_str(), __func__);
        return false;
    }
    msg.append(buffer);
    return true;
}

bool DfxDumpCatcher::DoReadRes(int fd, bool &ret, std::string& msg)
{
    int32_t res = DumpErrorCode::DUMP_ESUCCESS;
    ssize_t nread = read(fd, &res, sizeof(res));
    if (nread != sizeof(res)) {
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
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    DFXLOG_DEBUG("%s :: %s :: cPid(%d), cTid(%d), pidSize(%d).", DFXDUMPCATCHER_TAG.c_str(), \
        __func__, currentPid, currentTid, pidSize);

    time_t startTime = time(nullptr);
    if (startTime > 0) {
        DFXLOG_DEBUG("%s :: %s :: startTime(%lld).", DFXDUMPCATCHER_TAG.c_str(), __func__, startTime);
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
            DFXLOG_DEBUG("%s :: %s :: startTime(%lld), currentTime(%lld).", DFXDUMPCATCHER_TAG.c_str(), \
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
} // namespace HiviewDFX
} // namespace OHOS
