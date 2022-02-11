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

#include "../../interfaces/innerkits/dump_catcher/include/dfx_dump_catcher.h"

#include <cinttypes>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/syscall.h>
#include <sys/types.h>
#include <thread>

#define LOG_BUF_LEN 1024
static const int ARG1 = 1;
static const int ARG2 = 2;
static const int ARG3 = 3;
static const int ARG4 = 4;
static const int ARG5 = 5;
static const int ARG6 = 6;
static const int ARG7 = 7;
static const int SLEEP_TIME = 10;

static void PrintCommandHelp()
{
    printf("usage:\n");
    printf("-u uid -p pid -t tid dump the stacktrace of the all thread stack of a process(pid) or ");
    printf("the specified thread(mark by tid) thread of a process(pid).\n");
}

static bool CatchStack(int32_t pid, int32_t tid)
{
    printf("This is function real catch stack.\n");
    OHOS::HiviewDFX::DfxDumpCatcher mDfxDumpCatcher;
    std::string msg = "";
    bool ret = mDfxDumpCatcher.DumpCatch(pid, tid, msg);

    printf("DumpCatch :: ret: %d.\n", ret);

    long lenStackInfo = msg.length();
    write(STDOUT_FILENO, msg.c_str(), lenStackInfo);

    return ret;
}

static bool FunctionThree(int32_t pid, int32_t tid)
{
    printf("This is function three.\n");
    int currentPid = getpid();
    int currentTid = syscall(SYS_gettid);
    bool ret = CatchStack(currentPid, currentTid);
    ret = CatchStack(currentPid, 0);
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
    int sleepTime = 10;
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
    int32_t pid = 0;
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
    } else {
        PrintCommandHelp();
        return -1;
    }

    setuid(uid);
    StartMultiThread();
    bool ret = FunctionOne(pid, tid);

    return ret;
}
