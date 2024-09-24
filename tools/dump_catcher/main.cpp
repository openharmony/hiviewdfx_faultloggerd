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

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif

static const std::string DUMP_STACK_TAG_USAGE = "Usage:";
static const std::string DUMP_STACK_TAG_FAILED = "Failed:";

static void PrintCommandHelp()
{
    printf("%s\n", DUMP_STACK_TAG_USAGE.c_str());
    printf("-p pid -t tid    dump the stacktrace of the thread with given tid.\n");
    printf("-p pid    dump the stacktrace of all the threads with given pid.\n");
}

static void PrintCommandFailed()
{
    printf("%s\npid and tid must > 0.\n", DUMP_STACK_TAG_FAILED.c_str());
}

static int ParseParamters(int argc, char *argv[], int32_t &pid, int32_t &tid)
{
    int ret = 0;
    if (argc <= 1) {
        return ret;
    }
    LOGDEBUG("argc: %{public}d, argv1: %{public}s", argc, argv[1]);

    int optRet;
    const char *optString = "cmkp:t:";
    while ((optRet = getopt(argc, argv, optString)) != -1) {
        switch (optRet) {
            case 'p':
                if (optarg == nullptr) {
                    break;
                }
                if (atoi(optarg) > 0) {
                    ret = 1;
                    pid = atoi(optarg);
                } else {
                    ret = -1;
                    PrintCommandFailed();
                }
                break;
            case 't':
                if (optarg == nullptr) {
                    break;
                }
                if (atoi(optarg) > 0) {
                    tid = atoi(optarg);
                } else {
                    ret = -1;
                    PrintCommandFailed();
                }
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

    int32_t pid = 0;
    int32_t tid = 0;

    alarm(DUMPCATCHER_TIMEOUT);
    setsid();

    if (ParseParamters(argc, argv, pid, tid) <= 0) {
        return 0;
    }

    LOGDEBUG("pid: %{public}d, tid: %{public}d", pid, tid);
    OHOS::HiviewDFX::DumpCatcher::GetInstance().Dump(pid, tid);
    return 0;
}
