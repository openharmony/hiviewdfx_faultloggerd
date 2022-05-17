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

static const std::string DFXDUMPCATCHER_TAG = "DfxDumpCatcher";

const std::string LOG_FILE_PATH = "/data/log/faultlog/temp";

static const int NUMBER_TEN = 10;
constexpr int MAX_TEMP_FILE_LENGTH = 256;
constexpr int SINGLE_THREAD_UNWIND_TIMEOUT = 100; // 100 millseconds

namespace OHOS {
namespace HiviewDFX {
static pthread_mutex_t g_dumpCatcherMutex = PTHREAD_MUTEX_INITIALIZER;

DfxDumpCatcher::DfxDumpCatcher()
{
    DfxLogDebug("%s :: construct.", DFXDUMPCATCHER_TAG.c_str());
}

DfxDumpCatcher::~DfxDumpCatcher()
{
    DfxLogDebug("%s :: destructor.", DFXDUMPCATCHER_TAG.c_str());
}

bool DfxDumpCatcher::DoDumpLocalTid(int tid)
{
    bool ret = false;
    if (tid <= 0) {
        DfxLogError("%s :: DoDumpLocalTid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    do {
        if (DfxDumpCatcherLocalDumper::SendLocalDumpRequest(tid) == true) {
            std::unique_lock<std::mutex> lck(DfxDumpCatcherLocalDumper::g_localDumperMutx);
            if (DfxDumpCatcherLocalDumper::g_localDumperCV.wait_for(lck, \
                std::chrono::milliseconds(SINGLE_THREAD_UNWIND_TIMEOUT))==std::cv_status::timeout) {
                // time out means we didn't got any back trace msg, just return false.
                ret = false;
                break;
            }

            ret = true;
        } else {
            break;
        }
    } while (false);

    DfxLogError("%s :: DoDumpLocalTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalPid(int pid, std::string& msg)
{
    DfxLogDebug("%s :: DoDumpLocalPid :: pid(%d).", DFXDUMPCATCHER_TAG.c_str(), pid);
    if (pid <= 0) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    char realPath[PATH_MAX] = {'\0'};
    if (realpath("/proc/self/task", realPath) == nullptr) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as realpath failed.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    DIR *dir = opendir(realPath);
    if (dir == nullptr) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as opendir failed.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0) {
            continue;
        }

        if (strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        pid_t tid = atoi(ent->d_name);
        if (tid == 0) {
            continue;
        }

        int currentTid = syscall(SYS_gettid);
        if (tid == currentTid) {
            DfxDumpCatcherLocalDumper::ExecLocalDump(tid, DUMP_CATCHER_NUMBER_THREE);
            msg.append(DfxDumpCatcherLocalDumper::CollectUnwindResult(tid));
        } else {
            if (DoDumpLocalTid(tid)) {
                msg.append(DfxDumpCatcherLocalDumper::CollectUnwindResult(tid));
            } else {
                msg.append("Failed to dump thread:" + std::to_string(tid) + ".\n");
            }
        }
    }

    if (closedir(dir) == -1) {
        DfxLogError("closedir failed.");
    }
    DfxLogDebug("%s :: DoDumpLocalPid :: return true.", DFXDUMPCATCHER_TAG.c_str());
    return true;
}

std::string DfxDumpCatcher::TryToGetGeneratedLog(const std::string& path, const std::string& prefix)
{
    DfxLogDebug("%s :: TryToGetGeneratedLog :: path(%s), prefix(%s).", \
        DFXDUMPCATCHER_TAG.c_str(), path.c_str(), prefix.c_str());
    std::vector<std::string> files;
    OHOS::GetDirFiles(path, files);
    int i = 0;
    while (i < (int)files.size() && files[i].find(prefix) == std::string::npos) {
        i++;
    }
    if (i < (int)files.size()) {
        DfxLogDebug("%s :: TryToGetGeneratedLog :: return filePath(%s)", \
            DFXDUMPCATCHER_TAG.c_str(), files[i].c_str());
        return files[i];
    }
    DfxLogDebug("%s :: TryToGetGeneratedLog :: return empty string", DFXDUMPCATCHER_TAG.c_str());
    return "";
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
                DfxLogError("%s :: DoDumpRemote :: illegal path length(%d)",
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
    DfxLogDebug("%s :: WaitForLogGenerate :: return empty string", DFXDUMPCATCHER_TAG.c_str());
    inotify_rm_watch (inotifyFd, wd);
    close(inotifyFd);
    return "";
}

bool DfxDumpCatcher::DoDumpRemoteLocked(int pid, int tid, std::string& msg)
{
    DfxLogDebug("%s :: DoDumpRemote :: pid(%d), tid(%d).", DFXDUMPCATCHER_TAG.c_str(), pid, tid);
    if (pid <= 0 || tid < 0) {
        DfxLogError("%s :: DoDumpRemote :: param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    if (RequestSdkDump(pid, tid) == true) {
        // Get stack trace file
        long long stackFileLength = 0;
        std::string stackTraceFilePatten = LOG_FILE_PATH + "/stacktrace-" + std::to_string(pid);
        std::string stackTraceFileName = WaitForLogGenerate(LOG_FILE_PATH, stackTraceFilePatten);
        if (stackTraceFileName.empty()) {
            return false;
        }

        bool ret = OHOS::LoadStringFromFile(stackTraceFileName, msg);
        OHOS::RemoveFile(stackTraceFileName);
        DfxLogDebug("%s :: DoDumpRemote :: return true.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }
    DfxLogError("%s :: DoDumpRemote :: return false.", DFXDUMPCATCHER_TAG.c_str());
    return false;
}

bool DfxDumpCatcher::DoDumpLocalLocked(int pid, int tid, std::string& msg)
{
    bool ret = DfxDumpCatcherLocalDumper::InitLocalDumper();
    if (!ret) {
        DfxDumpCatcherLocalDumper::DestroyLocalDumper();
        return ret;
    }

    if (tid == syscall(SYS_gettid)) {
        ret = DfxDumpCatcherLocalDumper::ExecLocalDump(tid, DUMP_CATCHER_NUMBER_TWO);
        msg.append(DfxDumpCatcherLocalDumper::CollectUnwindResult(tid));
    } else if (tid == 0) {
        ret = DoDumpLocalPid(pid, msg);
    } else {
        ret = DoDumpLocalTid(tid);
        if (ret) {
            msg.append(DfxDumpCatcherLocalDumper::CollectUnwindResult(tid));
        } else {
            msg.append("Failed to dump thread:" + std::to_string(tid) + ".\n");
        }
    }

    DfxDumpCatcherLocalDumper::DestroyLocalDumper();
    return ret;
}

bool DfxDumpCatcher::DumpCatch(int pid, int tid, std::string& msg)
{
    pthread_mutex_lock(&g_dumpCatcherMutex);
    int currentPid = getpid();
    DfxLogDebug("%s :: dump_catch :: cPid(%d), pid(%d), tid(%d).",
        DFXDUMPCATCHER_TAG.c_str(), currentPid, pid, tid);

    if (pid <= 0 || tid < 0) {
        DfxLogError("%s :: dump_catch :: param error.", DFXDUMPCATCHER_TAG.c_str());
        pthread_mutex_unlock(&g_dumpCatcherMutex);
        return false;
    }

    bool ret = false;
    if (pid == currentPid) {
        ret = DoDumpLocalLocked(pid, tid, msg);
    } else {
        ret = DoDumpRemoteLocked(pid, tid, msg);
    }
    DfxLogDebug("%s :: dump_catch :: ret(%d), msg(%s).", DFXDUMPCATCHER_TAG.c_str(), ret, msg.c_str());
    pthread_mutex_unlock(&g_dumpCatcherMutex);
    return ret;
}

bool DfxDumpCatcher::DumpCatchMultiPid(const std::vector<int> pidV, std::string& msg)
{
    pthread_mutex_lock(&g_dumpCatcherMutex);
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    int pidSize = (int)pidV.size();
    DfxLogDebug("%s :: %s :: cPid(%d), cTid(%d), pidSize(%d).", DFXDUMPCATCHER_TAG.c_str(), \
        __func__, currentPid, currentTid, pidSize);

    if (pidSize <= 0) {
        DfxLogError("%s :: %s :: param error, pidSize(%d).", DFXDUMPCATCHER_TAG.c_str(), __func__, pidSize);
        pthread_mutex_unlock(&g_dumpCatcherMutex);
        return false;
    }

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
    pthread_mutex_unlock(&g_dumpCatcherMutex);
    if (msg.find("Tid:") != std::string::npos) {
        return true;
    } else {
        return false;
    }
}

bool DfxDumpCatcher::DumpCatchFrame(int pid, int tid, std::string& msg, \
    std::vector<std::shared_ptr<DfxDumpCatcherFrame>>& frameV)
{
    if (pid != getpid() || tid == 0) {
        DfxLogError("DumpCatchFrame :: only support localDump.");
        return false;
    }

    pthread_mutex_lock(&g_dumpCatcherMutex);
    bool ret = DfxDumpCatcherLocalDumper::InitLocalDumper();
    if (!ret) {
        DfxLogError("DumpCatchFrame :: failed to init local dumper.");
        DfxDumpCatcherLocalDumper::DestroyLocalDumper();
        pthread_mutex_unlock(&g_dumpCatcherMutex);
        return ret;
    }

    if (tid == syscall(SYS_gettid)) {
        ret = DfxDumpCatcherLocalDumper::ExecLocalDump(tid, DUMP_CATCHER_NUMBER_ONE);
    } else {
        ret = DoDumpLocalTid(tid);
    }

    if (ret) {
        msg = DfxDumpCatcherLocalDumper::CollectUnwindResult(tid);
        DfxDumpCatcherLocalDumper::CollectUnwindFrames(frameV);
    }

    DfxDumpCatcherLocalDumper::DestroyLocalDumper();
    pthread_mutex_unlock(&g_dumpCatcherMutex);
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
