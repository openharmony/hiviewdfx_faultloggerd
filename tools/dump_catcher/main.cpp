/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include <string>
#include <unistd.h>
#include <getopt.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dump_catcher.h"
#include "faultloggerd_client.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif

using namespace OHOS::HiviewDFX;
static const std::string DUMP_STACK_TAG_USAGE = "Usage:";
static const std::string DUMP_STACK_TAG_FAILED = "Failed:";
static constexpr int SAVE_CORE_DUMP_TIMEOUT = 10000; //  time out 10000 ms

static void PrintCommandHelp()
{
    printf("%s\n", DUMP_STACK_TAG_USAGE.c_str());
    printf("-p pid                | dump the stacktrace of all the threads with given pid, timeout default 3000ms.\n");
    printf("-p pid -t tid         | dump the stacktrace of the thread with given tid .\n");
    printf("-p pid -T timeout(ms) | dump the stacktrace of the thread with given tid in timeout.\n");
    printf("-k pid                | dump the origin kernel stacktrace without ark of the pid.\n");
    printf("-k pid -a             | dump the origin kernel stacktrace with ark of the pid.\n");
    printf("-k pid -a -f          | dump the origin kernel stacktrace with ark of the pid and format parse stack.\n");
    printf("-c save pid           | begin to coredump the process of the pid.\n");
    printf("-c cancel pid         | cancel to coredump the process of the pid.\n");
}

static void PrintCommandFailed()
{
    printf("%s\npid and tid and timeout must > 0.\n", DUMP_STACK_TAG_FAILED.c_str());
}


static int GetIdFromArgs(char *optarg, int32_t &id)
{
    int ret = 0;

    if (atoi(optarg) > 0) {
        ret = 1;
        id = atoi(optarg);
    } else {
        ret = -1;
        PrintCommandFailed();
    }

    return ret;
}

static int ExecuteCoredumpCmd(std::string subCmd, char* pidChar)
{
    if (atoi(pidChar) <= 0) {
        printf("pid error, input should like dumpcatcher -c save/cancel pid\n");
        return -1;
    }
    pid_t pid = atoi(pidChar);
    printf("cmd is -c %s %d \n", subCmd.c_str(), pid);
    if (subCmd == "save") {
        SaveCoredumpToFileTimeout(pid, SAVE_CORE_DUMP_TIMEOUT);
        return 0;
    } else if (subCmd == "cancel") {
        CancelCoredump(pid);
        return 0;
    } else {
        printf("input error coredump cmd!\n");
        return -1;
    }
}

static int ParseParamters(int argc, char *argv[], DumpOptions& dumpOpt)
{
    int ret = 0;
    if (argc <= 1) {
        return ret;
    }
    DFXLOGD("[%{public}d]: argc: %{public}d, argv1: %{public}s", __LINE__, argc, argv[1]);

    int optRet;
    const char *optString = "p:t:c:T:k:af";
    while ((optRet = getopt(argc, argv, optString)) != -1) {
        if (optarg == nullptr) {
            continue;
        }
        switch (optRet) {
            case 'a':
                dumpOpt.needArk = true;
                break;
            case 'c':
                if (optind >= argc) {
                    printf("input error, input should like dumpcatcher -c save/cancel pid\n");
                    return 0;
                }
                ExecuteCoredumpCmd(optarg, argv[optind]);
                return 0;
            case 'f':
                dumpOpt.needParse = true;
                break;
            case 'k':
                ret = GetIdFromArgs(optarg, dumpOpt.pid);
                dumpOpt.type = DumpType::DUMP_KERNEL_STACK;
                break;
            case 'p':
                ret = GetIdFromArgs(optarg, dumpOpt.pid);
                dumpOpt.type = DumpType::DUMP_USER_STACK;
                break;
            case 't':
                ret = GetIdFromArgs(optarg, dumpOpt.tid);
                break;
            case 'T':
                ret = GetIdFromArgs(optarg, dumpOpt.timeout);
                break;
            default:
                ret = 0;
                break;
        }
    }

    if (ret == 0) {
        PrintCommandHelp();
    }
    return ret;
}

int main(int argc, char *argv[])
{
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
    DFX_InstallLocalSignalHandler();
#endif

    DumpOptions dumpOpt;

    alarm(DUMPCATCHER_TIMEOUT);
    setsid();

    if (ParseParamters(argc, argv, dumpOpt) <= 0) {
        return 0;
    }

    DFXLOGD("pid: %{public}d, tid: %{public}d, timeout: %{public}d", dumpOpt.pid, dumpOpt.tid, dumpOpt.timeout);
    if (dumpOpt.type == DumpType::DUMP_USER_STACK) {
        DumpCatcher::GetInstance().Dump(dumpOpt);
    } else if (dumpOpt.type == DumpType::DUMP_KERNEL_STACK) {
        DumpCatcher::GetInstance().DumpKernelStack(dumpOpt);
    }
    return 0;
}
