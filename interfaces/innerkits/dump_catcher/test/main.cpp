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

/* This files contains dump_catcher sdk unit test tools. */

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <directory_ex.h>
#include <file_ex.h>
#include <securec.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>
#include <vector>

#include "dfx_dump_catcher.h"
#include "dfx_dump_catcher_frame.h"

static const int ARG1 = 1;
static const int ARG2 = 2;
static const int ARG3 = 3;
static const int ARG4 = 4;
static const int ARG5 = 5;
static const int ARG6 = 6;
static const int ARG7 = 7;
static const int ARG8 = 8;
static const int ARG9 = 9;
static const int ARG10 = 10;
static const int SLEEP_TIME = 100;

static int StartMultiThread();

static void PrintCommandHelp()
{
    printf("usage:\n");
    printf("-u uid -p pid -t tid dump the stacktrace of the all thread stack of a process(pid) or ");
    printf("the specified thread(mark by tid) thread of a process(pid).\n");
}

static bool CatchStack(int32_t pid, int32_t tid)
{
    printf("This is function DumpCatch.\n");
    OHOS::HiviewDFX::DfxDumpCatcher mDfxDumpCatcher;
    std::string msg = "";
    bool ret = mDfxDumpCatcher.DumpCatch(pid, tid, msg);

    printf("DumpCatch :: ret: %d.\n", ret);

    long lenStackInfo = msg.length();
    write(STDOUT_FILENO, msg.c_str(), lenStackInfo);

    return ret;
}

static bool CatchStackMulti(const std::vector<int> pidV)
{
    printf("This is function DumpCatchMultiPid.\n");
    OHOS::HiviewDFX::DfxDumpCatcher mDfxDumpCatcher;
    std::string msg = "";
    bool ret = mDfxDumpCatcher.DumpCatchMultiPid(pidV, msg);

    printf("DumpCatchMultiPid :: ret: %d.\n", ret);

    long lenStackInfo = msg.length();
    write(STDOUT_FILENO, msg.c_str(), lenStackInfo);

    return ret;
}

static bool CatchStackFrame(int32_t pid, int32_t tid)
{
    printf("This is function DumpCatchFrame :: pid(%d), tid(%d).\n", pid, tid);
    OHOS::HiviewDFX::DfxDumpCatcher mDfxDumpCatcher;
    std::string msg = "";
    std::vector<std::shared_ptr<OHOS::HiviewDFX::DfxDumpCatcherFrame>> frameV;
    bool ret = mDfxDumpCatcher.DumpCatchFrame(pid, tid, msg, frameV);

    printf("DumpCatchFrame :: ret: %d, frameV: %d.\n", ret, frameV.size());

    printf("DumpCatchFrame :: msg:\n");
    long lenStackInfo = msg.length();
    write(STDOUT_FILENO, msg.c_str(), lenStackInfo);

    printf("DumpCatchFrame :: frame:\n");
    for (int i = 0; i < frameV.size(); i++) {
        std::shared_ptr<OHOS::HiviewDFX::DfxDumpCatcherFrame> frame = frameV[i];
        if (frame->GetFrameFuncName() == "") {
            printf("#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s\n",
                i, frame->GetFrameRelativePc(), frame->GetFramePc(), (frame->GetFrameMap() == nullptr) ? \
             "Unknown" : frame->GetFrameMap()->GetMapPath().c_str());
        } else {
            printf("#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s(%s+%" PRIu64 ")\n", i, \
                frame->GetFrameRelativePc(), frame->GetFramePc(), (frame->GetFrameMap() == nullptr) ?
             "Unknown" : frame->GetFrameMap()->GetMapPath().c_str(), frame->GetFrameFuncName().c_str(), \
                frame->GetFrameFuncOffset());
        }
    }

    return ret;
}

static bool FunctionThree(int32_t pid, int32_t tid)
{
    printf("This is function three.\n");
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    bool ret = CatchStack(currentPid, currentTid);
    ret = CatchStack(currentPid, 0);

    StartMultiThread();
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/task", currentPid) <= 0) {
        return false;
    }

    char realPath[PATH_MAX] = {'\0'};
    if (realpath(path, realPath) == nullptr) {
        return false;
    }

    DIR *dir = opendir(realPath);
    if (dir == nullptr) {
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

        pid_t tid_l = atoi(ent->d_name);
        if (tid_l == 0) {
            continue;
        }

        if (tid_l == currentTid) {
            ret = CatchStackFrame(currentPid, currentTid);
        } else {
            ret = CatchStackFrame(currentPid, tid_l);
        }
    }
    ret = CatchStack(pid, tid);
    return ret;
}

static bool FunctionTwo(int32_t pid, int32_t tid)
{
    printf("This is function two.\n");
    bool ret = FunctionThree(pid, tid);
    return ret;
}

static bool FunctionOne(int32_t pid, int32_t tid)
{
    printf("This is function one.\n");
    bool ret = FunctionTwo(pid, tid);
    return ret;
}

static int SleepThread(int threadID)
{
    printf("SleepThread -Enter- :: threadID(%d).\n", threadID);
    int sleepTime = SLEEP_TIME;
    sleep(sleepTime);
    printf("SleepThread -Exit- :: threadID(%d).\n", threadID);
    return 0;
}

static int StartMultiThread()
{
    std::thread (SleepThread, ARG1).detach();
    std::thread (SleepThread, ARG2).detach();
    return 0;
}

int main(int const argc, char const * const argv[])
{
    bool ret = false;
    int32_t pid = 0;
    std::vector<int> pidV;
    int32_t tid = 0;
    int32_t uid = 1000;

    alarm(SLEEP_TIME);

    if (argc == ARG1) {
        PrintCommandHelp();
        return -1;
    }

    if (argc == ARG5) {
        if (strcmp("-p", argv[ARG3]) == 0) {
            pid = atoi(argv[ARG4]);
        } else {
            PrintCommandHelp();
            return -1;
        }

        if (strcmp("-u", argv[ARG1]) == 0) {
            uid = atoi(argv[ARG2]);
        } else {
            PrintCommandHelp();
            return -1;
        }
    } else if (argc == ARG7) {
        if (strcmp("-u", argv[ARG1]) == 0) {
            uid = atoi(argv[ARG2]);
        } else {
            PrintCommandHelp();
            return -1;
        }

        if (strcmp("-p", argv[ARG3]) == 0) {
            pid = atoi(argv[ARG4]);
        } else {
            PrintCommandHelp();
            return -1;
        }

        if (strcmp("-t", argv[ARG5]) == 0) {
            tid = atoi(argv[ARG6]);
        } else {
            PrintCommandHelp();
            return -1;
        }
    } else if (argc == ARG10) {
        if (strcmp("-u", argv[ARG1]) == 0) {
            uid = atoi(argv[ARG2]);
        } else {
            PrintCommandHelp();
            return -1;
        }

        if (strcmp("-p", argv[ARG3]) == 0) {
            pid = atoi(argv[ARG4]);
            pidV.push_back(pid);
            pid = atoi(argv[ARG5]);
            pidV.push_back(pid);
            pid = atoi(argv[ARG6]);
            pidV.push_back(pid);
            pid = atoi(argv[ARG7]);
            pidV.push_back(pid);
            int currentPid = getpid();
            pidV.push_back(currentPid);
        } else {
            PrintCommandHelp();
            return -1;
        }

        if (strcmp("-t", argv[ARG8]) == 0) {
            tid = atoi(argv[ARG9]);
        } else {
            PrintCommandHelp();
            return -1;
        }
    } else {
        PrintCommandHelp();
        return -1;
    }

    setuid(uid);
    StartMultiThread();
    if (argc == ARG10) {
        ret = CatchStackMulti(pidV);
    } else {
        ret = FunctionOne(pid, tid);
    }

    return ret;
}
