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

#include <directory_ex.h>
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

#include "../faultloggerd_client/include/faultloggerd_client.h"

static const std::string DFXDUMPCATCHER_TAG = "DfxDumpCatcher";

const std::string LOG_FILE_PATH = "/data/log/faultlog/temp";

static const int NUMBER_TEN = 10;
constexpr int MAX_TEMP_FILE_LENGTH = 256;

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

void DfxDumpCatcher::FreeStackInfo()
{
    free(DfxDumpCatcherLocalDumper::g_StackInfo_);
    DfxDumpCatcherLocalDumper::g_StackInfo_ = nullptr;
    DfxDumpCatcherLocalDumper::g_CurrentPosition = 0;
    DfxDumpCatcherLocalDumper::g_FrameV_.clear();
    DfxLogDebug("%s :: FreeStackInfo Exit.", DFXDUMPCATCHER_TAG.c_str());
}

bool DfxDumpCatcher::DoDumpLocalPidTid(int pid, int tid)
{
    DfxLogDebug("%s :: DoDumpLocalPidTid :: pid(%d), tid(%d).", DFXDUMPCATCHER_TAG.c_str(), pid, tid);
    bool ret = false;
    if (pid <= 0 || tid <= 0) {
        DfxLogError("%s :: DoDumpLocalPidTid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return ret;
    }

    do {
        if (RequestSdkDump(pid, tid) == true) {
            std::unique_lock<std::mutex> lck(DfxDumpCatcherLocalDumper::g_localDumperMutx_);
            if (DfxDumpCatcherLocalDumper::g_localDumperCV_.wait_for(lck, \
                std::chrono::seconds(DUMP_CATCHER_NUMBER_TWO))==std::cv_status::timeout) {
                // time out means we didn't got any back trace msg, just return false.
                ret = false;
                break;
            }

            ret = true;
        } else {
            break;
        }
    } while (false);

    DfxLogError("%s :: DoDumpLocalPidTid :: return %d.", DFXDUMPCATCHER_TAG.c_str(), ret);
    return ret;
}

bool DfxDumpCatcher::DoDumpLocalPid(int pid)
{
    DfxLogDebug("%s :: DoDumpLocalPid :: pid(%d).", DFXDUMPCATCHER_TAG.c_str(), pid);
    if (pid <= 0) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as param error.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

#ifdef CURRENT_THREAD_ONLY
    DfxDumpCatcherLocalDumper::ExecLocalDump(pid, syscall(SYS_gettid), DUMP_CATCHER_NUMBER_TWO);
#else
    char realPath[PATH_MAX] = {'\0'};
    if (realpath("/proc/self/task", realPath) == NULL) {
        DfxLogError("%s :: DoDumpLocalPid :: return false as realpath failed.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    DIR *dir = opendir(realPath);
    if (dir == NULL) {
        (void)closedir(dir);
        DfxLogError("%s :: DoDumpLocalPid :: return false as opendir failed.", DFXDUMPCATCHER_TAG.c_str());
        return false;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
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

        std::stringstream ss;
        ss << "Tid: " << tid;
        DfxDumpCatcherLocalDumper::WritePidTidInfo(ss.str());

        int currentTid = syscall(SYS_gettid);
        if (tid == currentTid) {
            DfxDumpCatcherLocalDumper::ExecLocalDump(pid, tid, DUMP_CATCHER_NUMBER_TWO);
        } else {
            DoDumpLocalPidTid(pid, tid);
        }
    }

    if (closedir(dir) == -1) {
        DfxLogError("closedir failed.");
    }
    DfxLogDebug("%s :: DoDumpLocalPid :: return true.", DFXDUMPCATCHER_TAG.c_str());
#endif
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
                DfxLogError("%s :: DoDumpRemote :: illegal path length(%d)", strlen(event->name));
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

bool DfxDumpCatcher::DoDumpRemote(int pid, int tid, std::string& msg)
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

bool DfxDumpCatcher::DoDumpLocal(int pid, int tid, std::string& msg)
{
    bool ret = false;
    DfxDumpCatcherLocalDumper::g_StackInfo_ = (char *) calloc(BACK_STACK_INFO_SIZE + 1, 1);
    errno_t err = memset_s(DfxDumpCatcherLocalDumper::g_StackInfo_, BACK_STACK_INFO_SIZE + 1, \
        '\0', BACK_STACK_INFO_SIZE);
    if (err != EOK) {
        DfxLogError("%s :: dump_catch :: memset_s failed, err = %d\n", DFXDUMPCATCHER_TAG.c_str(), err);
        FreeStackInfo();
        pthread_mutex_unlock(&g_dumpCatcherMutex);
        return false;
    }
    DfxDumpCatcherLocalDumper::g_CurrentPosition = 0;
    DfxDumpCatcherLocalDumper::DFX_InstallLocalDumper(SIGDUMP);

    if (tid == syscall(SYS_gettid)) {
        ret = DfxDumpCatcherLocalDumper::ExecLocalDump(pid, tid, DUMP_CATCHER_NUMBER_ONE);
    } else if (tid == 0) {
        ret = DoDumpLocalPid(pid);
    } else {
        ret = DoDumpLocalPidTid(pid, tid);
    }

    DfxDumpCatcherLocalDumper::DFX_UninstallLocalDumper(SIGDUMP);
    if (ret == true) {
        msg = DfxDumpCatcherLocalDumper::g_StackInfo_;
    }
    FreeStackInfo();
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
        ret = DoDumpLocal(pid, tid, msg);
    } else {
        ret = DoDumpRemote(pid, tid, msg);
    }
    DfxLogDebug("%s :: dump_catch :: ret(%d), msg(%s).", DFXDUMPCATCHER_TAG.c_str(), ret, msg.c_str());
    pthread_mutex_unlock(&g_dumpCatcherMutex);
    return ret;
}

bool DfxDumpCatcher::DumpCatchMultiPid(const std::vector<int> pidV, std::string& msg)
{
    pthread_mutex_lock(&g_dumpCatcherMutex);
    bool ret = false;
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
    DfxDumpCatcherLocalDumper::g_StackInfo_ = (char *) calloc(BACK_STACK_INFO_SIZE + 1, 1);
    errno_t err = memset_s(DfxDumpCatcherLocalDumper::g_StackInfo_, BACK_STACK_INFO_SIZE + 1, \
        '\0', BACK_STACK_INFO_SIZE);
    if (err != EOK) {
        DfxLogError("%s :: %s :: memset_s failed, err = %d\n", DFXDUMPCATCHER_TAG.c_str(), __func__, err);
        FreeStackInfo();
        pthread_mutex_unlock(&g_dumpCatcherMutex);
        return false;
    }
    DfxDumpCatcherLocalDumper::g_CurrentPosition = 0;

    time_t startTime = time(nullptr);
    if (startTime > 0) {
        DfxLogDebug("%s :: %s :: startTime(%ld).", DFXDUMPCATCHER_TAG.c_str(), __func__, startTime);
    }
    for (int i = 0; i < pidSize; i++) {
        int pid = pidV[i];

        std::stringstream ss;
        ss << "Pid: " << pid;
        DfxDumpCatcherLocalDumper::WritePidTidInfo(ss.str());
        if (pid == currentPid) {
            DfxDumpCatcherLocalDumper::DFX_InstallLocalDumper(SIGDUMP);
            ret = DoDumpLocalPid(pid);
            DfxDumpCatcherLocalDumper::DFX_UninstallLocalDumper(SIGDUMP);
        } else {
            ret = DoDumpRemote(pid, 0, msg);
            DfxDumpCatcherLocalDumper::WritePidTidInfo(msg);
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

    if (ret == true) {
        msg = DfxDumpCatcherLocalDumper::g_StackInfo_;
    }

    FreeStackInfo();
    DfxLogDebug("%s :: %s :: ret(%d), msg(%s).", DFXDUMPCATCHER_TAG.c_str(), __func__, ret, msg.c_str());
    pthread_mutex_unlock(&g_dumpCatcherMutex);
    return ret;
}

bool DfxDumpCatcher::DumpCatchFrame(int pid, int tid, std::string& msg, \
    std::vector<std::shared_ptr<DfxDumpCatcherFrame>>& frameV)
{
    pthread_mutex_lock(&g_dumpCatcherMutex);
    bool ret = false;
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    DfxLogDebug("%s :: %s :: cPid(%d), cTid(%d), pid(%d), tid(%d).",
        DFXDUMPCATCHER_TAG.c_str(), __func__, currentPid, currentTid, pid, tid);

    if (tid <= 0 || pid <= 0) {
        DfxLogError("%s :: %s :: param error, tid(%d), pid(%d).", DFXDUMPCATCHER_TAG.c_str(), __func__, tid, pid);
        pthread_mutex_unlock(&g_dumpCatcherMutex);
        return false;
    }
    DfxDumpCatcherLocalDumper::g_StackInfo_ = (char *) calloc(BACK_STACK_INFO_SIZE + 1, 1);
    errno_t err = memset_s(DfxDumpCatcherLocalDumper::g_StackInfo_, BACK_STACK_INFO_SIZE + 1, \
        '\0', BACK_STACK_INFO_SIZE);
    if (err != EOK) {
        DfxLogError("%s :: %s :: memset_s failed, err = %d\n", DFXDUMPCATCHER_TAG.c_str(), __func__, err);
        FreeStackInfo();
        pthread_mutex_unlock(&g_dumpCatcherMutex);
        return false;
    }
    DfxDumpCatcherLocalDumper::g_CurrentPosition = 0;

    if (pid == currentPid) {
        DfxDumpCatcherLocalDumper::DFX_InstallLocalDumper(SIGDUMP);
        if (tid == currentTid) {
            ret = DfxDumpCatcherLocalDumper::ExecLocalDump(pid, tid, DUMP_CATCHER_NUMBER_ONE);
        } else {
            ret = DoDumpLocalPidTid(pid, tid);
        }
        DfxDumpCatcherLocalDumper::DFX_UninstallLocalDumper(SIGDUMP);
    }

    if (ret == true) {
        msg = DfxDumpCatcherLocalDumper::g_StackInfo_;
        frameV = DfxDumpCatcherLocalDumper::g_FrameV_;
    }

    FreeStackInfo();
    DfxLogDebug("%s :: %s :: ret(%d), frameV(%d), msg(%s).", \
        DFXDUMPCATCHER_TAG.c_str(), __func__, ret, frameV.size(), msg.c_str());
    pthread_mutex_unlock(&g_dumpCatcherMutex);
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
