/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "dfx_unwind_local.h"

#include <algorithm>
#include <climits>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <strstream>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <file_ex.h>
#include <poll.h>
#include <pthread.h>
#include <securec.h>
#include <ucontext.h>
#include <unistd.h>

#include <sstream>
#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "directory_ex.h"
#include "../faultloggerd_client/include/faultloggerd_client.h"

static const std::string LOG_FILE_PATH = "/data/log/faultlog/temp";
static const int NUMBER_TEN = 10;
static const int MAX_TEMP_FILE_LENGTH = 256;
static const int DUMP_CATCHER_WAIT_LOG_FILE_GEN_TIME_US = 10000;
static const int DUMP_CATCHE_WORK_TIME_S = 60;
static const int NUMBER_TWO_KB = 2048;


namespace OHOS {
namespace HiviewDFX {
namespace {
static const std::string DFXDUMPCATCHER_TAG = "DfxDumpCatcher";
}

DfxDumpCatcher::DfxDumpCatcher()
{
}

DfxDumpCatcher::~DfxDumpCatcher()
{
}

bool DfxDumpCatcher::DoDumpLocalTid(int tid, std::string& msg)
{
    bool ret = false;
    if (tid <= 0) {
        DfxLogError("%s :: DoDumpLocalTid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }
    
    if (DfxUnwindLocal::GetInstance().SendLocalDumpRequest(tid) == true) {
        ret = DfxUnwindLocal::GetInstance().WaitLocalDumpRequest();
    }

    if (ret) {
        msg.append(DfxUnwindLocal::GetInstance().CollectUnwindResult(tid));
    } else {
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
            DfxUnwindLocal::GetInstance().ExecLocalDumpUnwind(tid, DUMP_CATCHER_NUMBER_THREE);
            msg.append(DfxUnwindLocal::GetInstance().CollectUnwindResult(tid));
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

std::string DfxDumpCatcher::WaitForLogGenerate(const std::string& path, const std::string& prefix)
{
    DfxLogDebug("%s :: WaitForLogGenerate :: path(%s), prefix(%s).", \
        DFXDUMPCATCHER_TAG.c_str(), path.c_str(), prefix.c_str());
    time_t pastTime = 0;
    time_t startTime = time(nullptr);
    if (startTime < 0) {
        DfxLogError("%s :: WaitForLogGenerate :: startTime(%d) is less than zero.", \
            DFXDUMPCATCHER_TAG.c_str(), startTime);
    }
    int32_t inotifyFd = inotify_init();
    if (inotifyFd == -1) {
        return "";
    }

    int wd = inotify_add_watch(inotifyFd, path.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO);
    if (wd < 0) {
        close(inotifyFd);
        return "";
    }

    while (true) {
        pastTime = time(nullptr) - startTime;
        if (pastTime > NUMBER_TEN) {
            break;
        }

        char buffer[NUMBER_TWO_KB] = {0}; // 2048 buffer size;
        char *offset = nullptr;
        struct inotify_event *event = nullptr;
        int len = read(inotifyFd, buffer, NUMBER_TWO_KB); // 2048 buffer size;
        if (len < 0) {
            break;
        }

        offset = buffer;
        event = (struct inotify_event *)buffer;
        while ((reinterpret_cast<char *>(event) - buffer) < len) {
            if (strlen(event->name) > MAX_TEMP_FILE_LENGTH) {
                DfxLogError("%s :: WaitForLogGenerate :: illegal path length(%d)",
                    DFXDUMPCATCHER_TAG.c_str(), strlen(event->name));
                auto tmpLen = sizeof(struct inotify_event) + event->len;
                event = (struct inotify_event *)(offset + tmpLen);
                offset += tmpLen;
                continue;
            }

            std::string filePath = path + "/" + std::string(event->name);
            if ((filePath.find(prefix) != std::string::npos) &&
                (filePath.length() < MAX_TEMP_FILE_LENGTH)) {
                inotify_rm_watch (inotifyFd, wd);
                close(inotifyFd);
                return filePath;
            }
            auto tmpLen = sizeof(struct inotify_event) + event->len;
            event = (struct inotify_event *)(offset + tmpLen);
            offset += tmpLen;
        }
        usleep(DUMP_CATCHER_WAIT_LOG_FILE_GEN_TIME_US);
    }
    inotify_rm_watch (inotifyFd, wd);
    close(inotifyFd);
    return "";
}

bool DfxDumpCatcher::DoDumpRemoteLocked(int pid, int tid, std::string& msg)
{
    bool ret = false;
    if (pid <= 0 || tid < 0) {
        DfxLogError("%s :: DoDumpRemote :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    if (RequestSdkDump(pid, tid) == true) {
        // Get stack trace file
        long long stackFileLength = 0;
        std::string stackTraceFilePatten = LOG_FILE_PATH + "/stacktrace-" + std::to_string(pid);
        std::string stackTraceFileName = WaitForLogGenerate(LOG_FILE_PATH, stackTraceFilePatten);
        if (stackTraceFileName.empty()) {
            return ret;
        }
        ret = OHOS::LoadStringFromFile(stackTraceFileName, msg);
        OHOS::RemoveFile(stackTraceFileName);
    }
    DfxLogDebug("%s :: DoDumpRemote :: ret(%d).", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalLocked(int pid, int tid, std::string& msg)
{
    bool ret = DfxUnwindLocal::GetInstance().Init();
    if (!ret) {
        DfxLogError("%s :: DoDumpLocal :: Init error.", DFXDUMPCATCHER_TAG.c_str());
        DfxUnwindLocal::GetInstance().Destroy();
        return ret;
    }

    if (tid == syscall(SYS_gettid)) {
        ret = DfxUnwindLocal::GetInstance().ExecLocalDumpUnwind(tid, DUMP_CATCHER_NUMBER_TWO);
        msg.append(DfxUnwindLocal::GetInstance().CollectUnwindResult(tid));
    } else if (tid == 0) {
        ret = DoDumpLocalPid(pid, msg);
    } else {
        ret = DoDumpLocalTid(tid, msg);
    }

    DfxUnwindLocal::GetInstance().Destroy();
    DfxLogDebug("%s :: DoDumpLocal :: ret(%d).", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
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
    DfxLogDebug("%s :: dump_catch :: ret(%d), msg(%s).", DFXDUMPCATCHER_TAG.c_str(), ret, msg.c_str());
    return ret;
}

bool DfxDumpCatcher::DumpCatchFd(int pid, int tid, std::string& msg, int fd)
{
    bool ret = DumpCatch(pid, tid, msg);

    DfxUnwindLocal::GetInstance().WriteUnwindResult(fd, msg);
    return ret;
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
        DfxLogDebug("%s :: %s :: startTime(%ld).", DFXDUMPCATCHER_TAG.c_str(), __func__, startTime);
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
            DfxLogDebug("%s :: %s :: startTime(%ld), currentTime(%ld).", DFXDUMPCATCHER_TAG.c_str(), \
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

bool DfxDumpCatcher::DumpCatchFrame(int pid, int tid, std::string& msg, \
    std::vector<std::shared_ptr<DfxFrame>>& frames)
{
    if (pid != getpid() || tid == 0) {
        DfxLogError("DumpCatchFrame :: only support localDump.");
        return false;
    }

    bool ret = DfxUnwindLocal::GetInstance().Init();
    if (!ret) {
        DfxLogError("DumpCatchFrame :: failed to init local dumper.");
        DfxUnwindLocal::GetInstance().Destroy();
        return ret;
    }

    std::unique_lock<std::mutex> lck(dumpCatcherMutex_);
    if (tid == syscall(SYS_gettid)) {
        ret = DfxUnwindLocal::GetInstance().ExecLocalDumpUnwind(tid, DUMP_CATCHER_NUMBER_ONE);
        msg = DfxUnwindLocal::GetInstance().CollectUnwindResult(tid);
    } else {
        ret = DoDumpLocalTid(tid, msg);
    }

    if (ret) {
        DfxUnwindLocal::GetInstance().CollectUnwindFrames(frames);
    }

    DfxUnwindLocal::GetInstance().Destroy();
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
