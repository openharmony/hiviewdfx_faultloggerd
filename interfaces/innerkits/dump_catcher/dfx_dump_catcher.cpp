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
#include "dfx_frame.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "file_ex.h"
#include "iosfwd"
#include "securec.h"
#include "strings.h"
#include "libunwind.h"
#include "libunwind_i-ohos.h"

#include "backtrace.h"
#include "backtrace_local_static.h"
#include "backtrace_local_thread.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static const int DUMP_CATCHE_WORK_TIME_S = 60;
static const int BACK_TRACE_DUMP_MIX_TIMEOUT_MS = 2000;
static const int BACK_TRACE_DUMP_CPP_TIMEOUT_MS = 10000;
static const std::string DFXDUMPCATCHER_TAG = "DfxDumpCatcher";

static bool IsThreadInCurPid(int32_t tid)
{
    std::string path = "/proc/self/task/" + std::to_string(tid);
    return access(path.c_str(), F_OK) == 0;
}
enum DfxDumpPollRes : int32_t {
    DUMP_POLL_INIT = -1,
    DUMP_POLL_OK,
    DUMP_POLL_FD,
    DUMP_POLL_FAILED,
    DUMP_POLL_TIMEOUT,
    DUMP_POLL_RETURN,
};
}

DfxDumpCatcher::DfxDumpCatcher()
{
    frameCatcherPid_ = 0;
    (void)GetProcStatus(procInfo_);
}

DfxDumpCatcher::DfxDumpCatcher(int32_t pid) : frameCatcherPid_(pid)
{
    (void)GetProcStatus(procInfo_);
}

DfxDumpCatcher::~DfxDumpCatcher()
{
    BacktraceLocalStatic::GetInstance().CleanUp();
}

bool DfxDumpCatcher::DoDumpCurrTid(const size_t skipFrameNum, std::string& msg)
{
    bool ret = false;
    ret = BacktraceLocalThread::GetBacktraceString(msg, BACKTRACE_CURRENT_THREAD, skipFrameNum + 1, false);
    if (!ret) {
        int currTid = syscall(SYS_gettid);
        msg.append("Failed to dump curr thread:" + std::to_string(currTid) + ".\n");
    }
    DfxLogDebug("%s :: DoDumpCurrTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalTid(const int tid, std::string& msg)
{
    bool ret = false;
    if (tid <= 0) {
        DfxLogError("%s :: DoDumpLocalTid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    ret = BacktraceLocalThread::GetBacktraceString(msg, tid, 0, false);
    if (!ret) {
        msg.append("Failed to dump thread:" + std::to_string(tid) + ".\n");
    }
    DfxLogDebug("%s :: DoDumpLocalTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalPid(int pid, std::string& msg)
{
    bool ret = false;
    if (pid <= 0) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }
    size_t skipFramNum = DUMP_CATCHER_NUMBER_THREE;

    char realPath[PATH_MAX] = {'\0'};
    if (realpath("/proc/self/task", realPath) == nullptr) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as realpath failed.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    DIR *dir = opendir(realPath);
    if (dir == nullptr) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as opendir failed.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        pid_t tid = atoi(ent->d_name);
        if (tid == 0) {
            continue;
        }

        int currentTid = syscall(SYS_gettid);
        if (tid == currentTid) {
            ret = DoDumpCurrTid(skipFramNum, msg);
        } else {
            ret = DoDumpLocalTid(tid, msg);
        }
    }

    if (closedir(dir) == -1) {
        DfxLogError("closedir failed.");
    }

    DfxLogDebug("%s :: DoDumpLocalPid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpRemoteLocked(int pid, int tid, std::string& msg)
{
    int type = static_cast<int>(DUMP_TYPE_NATIVE);
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
        if (!IsThreadInCurPid(tid)) {
            msg.append("tid(" + std::to_string(tid) + ") is not in pid(" + std::to_string(pid) + ").\n");
        } else {
            ret = DoDumpLocalTid(tid, msg);
        }
    }

    DfxLogDebug("%s :: DoDumpLocal :: ret(%d).", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DumpCatchMix(int pid, int tid, std::string& msg)
{
    int type = static_cast<int>(DUMP_TYPE_MIX);
    return DoDumpCatchRemote(type, pid, tid, msg);
}

bool DfxDumpCatcher::DumpCatch(int pid, int tid, std::string& msg)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        DfxLogError("%s :: dump_catch :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    std::unique_lock<std::mutex> lck(dumpCatcherMutex_);
    int currentPid = getpid();
    DfxLogDebug("%s :: dump_catch :: cPid(%d), pid(%d), tid(%d).",
        DFXDUMPCATCHER_TAG.c_str(), currentPid, pid, tid);

    if (pid == currentPid) {
        ret = DoDumpLocalLocked(pid, tid, msg);
    } else {
        ret = DoDumpRemoteLocked(pid, tid, msg);
    }
    DfxLogDebug("%s :: dump_catch :: ret: %d, msg: %s", DFXDUMPCATCHER_TAG.c_str(), ret, msg.c_str());
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
    OHOS::LoadStringFromFile(path, content);
    if (!content.empty()) {
        std::string str = desc + ":\n" + content + "\n";
        result.append(str);
    }
    return;
}

static void LoadPidStat(const int pid, std::string& msg)
{
    std::string statPath = "/proc/" + std::to_string(pid) + "/stat";
    std::string wchanPath = "/proc/" + std::to_string(pid) + "/wchan";
    LoadPathContent("stat", statPath, msg);
    LoadPathContent("wchan", wchanPath, msg);
}

bool DfxDumpCatcher::DoDumpCatchRemote(const int type, int pid, int tid, std::string& msg)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        msg.append("Result: pid(" + std::to_string(pid) + ") param error.\n");
        DfxLogWarn("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
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
        }
        DfxLogWarn("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
        RequestDelPipeFd(pid);
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
                int type = static_cast<int>(DUMP_TYPE_NATIVE);
                return DoDumpCatchRemote(type, pid, tid, msg);
            } else if (type == DUMP_TYPE_NATIVE) {
                LoadPidStat(pid, msg);
            }
            break;
        default:
            break;
    }
    DfxLogInfo("%s :: %s :: ret: %d", DFXDUMPCATCHER_TAG.c_str(), __func__, ret);
    return ret;
}

int DfxDumpCatcher::DoDumpRemotePid(const int type, int pid, std::string& msg)
{
    int readBufFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_READ_BUF);
    DfxLogDebug("read buf fd: %d", readBufFd);

    int readResFd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_READ_RES);
    DfxLogDebug("read res fd: %d", readResFd);

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
    DfxLogInfo("%s :: %s :: ret: %d", DFXDUMPCATCHER_TAG.c_str(), __func__, ret);
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

    DfxLogInfo("%s :: %s :: %s", DFXDUMPCATCHER_TAG.c_str(), __func__, resMsg.c_str());
    msg = resMsg + bufMsg;
    if (res) {
        ret = DUMP_POLL_OK;
    }
    return ret;
}

bool DfxDumpCatcher::DoReadBuf(int fd, std::string& msg)
{
    char buffer[LOG_BUF_LEN];
    bzero(buffer, sizeof(buffer));
    ssize_t nread = read(fd, buffer, sizeof(buffer) - 1);
    if (nread <= 0) {
        DfxLogWarn("%s :: %s :: read error", DFXDUMPCATCHER_TAG.c_str(), __func__);
        return false;
    }
    msg.append(buffer);
    return true;
}

bool DfxDumpCatcher::DoReadRes(int fd, bool &ret, std::string& msg)
{
    DumpResMsg dumpRes;
    ssize_t nread = read(fd, &dumpRes, sizeof(struct DumpResMsg));
    if (nread != sizeof(struct DumpResMsg)) {
        DfxLogWarn("%s :: %s :: read error", DFXDUMPCATCHER_TAG.c_str(), __func__);
        return false;
    }

    if (dumpRes.res == ProcessDumpRes::DUMP_ESUCCESS) {
        ret = true;
    }
    DfxDumpRes::GetInstance().SetRes(dumpRes.res);
    msg.append("Result: " + DfxDumpRes::GetInstance().ToString() + "\n");
    return true;
}

bool DfxDumpCatcher::DumpCatchMultiPid(const std::vector<int> pidV, std::string& msg)
{
    bool ret = false;
    int pidSize = (int)pidV.size();
    if (pidSize <= 0) {
        DfxLogError("%s :: %s :: param error, pidSize(%d).", DFXDUMPCATCHER_TAG.c_str(), __func__, pidSize);
        return ret;
    }

    std::unique_lock<std::mutex> lck(dumpCatcherMutex_);
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    DfxLogDebug("%s :: %s :: cPid(%d), cTid(%d), pidSize(%d).", DFXDUMPCATCHER_TAG.c_str(), \
        __func__, currentPid, currentTid, pidSize);

    time_t startTime = time(nullptr);
    if (startTime > 0) {
        DfxLogDebug("%s :: %s :: startTime(%lld).", DFXDUMPCATCHER_TAG.c_str(), __func__, startTime);
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
            DfxLogDebug("%s :: %s :: startTime(%lld), currentTime(%lld).", DFXDUMPCATCHER_TAG.c_str(), \
                __func__, startTime, currentTime);
            if (currentTime > startTime + DUMP_CATCHE_WORK_TIME_S) {
                break;
            }
        }
    }

    DfxLogDebug("%s :: %s :: msg(%s).", DFXDUMPCATCHER_TAG.c_str(), __func__, msg.c_str());
    if (msg.find("Tid:") != std::string::npos) {
        ret = true;
    }
    return ret;
}

bool DfxDumpCatcher::InitFrameCatcher()
{
    std::unique_lock<std::mutex> lck(dumpCatcherMutex_);
    if (as_ != nullptr) {
        return true;
    }

    unw_init_local_address_space(&as_);
    if (as_ == nullptr) {
        return false;
    }

    cache_ = std::make_shared<DfxSymbolsCache>();
    return true;
}

void DfxDumpCatcher::DestroyFrameCatcher()
{
    std::unique_lock<std::mutex> lck(dumpCatcherMutex_);
    if (as_ != nullptr) {
        unw_destroy_local_address_space(as_);
        as_ = nullptr;
    }

    cache_ = nullptr;
}

bool DfxDumpCatcher::ReleaseThread(int tid)
{
    BacktraceLocalStatic::GetInstance().ReleaseThread(tid);
    return true;
}

bool DfxDumpCatcher::CatchFrame(std::vector<NativeFrame>& frames)
{
    std::unique_lock<std::mutex> lck(dumpCatcherMutex_);
    if (as_ == nullptr || cache_ == nullptr) {
        return false;
    }

    int skipFrameNum = 1; // skip current frame
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
    if (!thread.Unwind(as_, cache_, skipFrameNum)) {
        return false;
    }

    frames.clear();
    frames = thread.GetFrames();
    return true;
}

bool DfxDumpCatcher::CatchFrame(int tid, std::vector<NativeFrame>& frames, bool releaseThread)
{
    if (as_ == nullptr || cache_ == nullptr) {
        return false;
    }

    if (tid <= 0 || frameCatcherPid_ != procInfo_.pid) {
        DfxLogError("DfxDumpCatchFrame :: only support localDump.");
        return false;
    }

    if (!IsThreadInCurPid(tid) && !procInfo_.ns) {
        DfxLogError("DfxDumpCatchFrame :: target tid is not in our task.");
        return false;
    }

    if (tid == procInfo_.tid) {
        return CatchFrame(frames);
    }

    std::unique_lock<std::mutex> lck(dumpCatcherMutex_);
    BacktraceLocalThread thread(tid);
    if (!thread.Unwind(as_, cache_, 0, false, releaseThread)) {
        return false;
    }

    frames.clear();
    frames = thread.GetFrames();
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
