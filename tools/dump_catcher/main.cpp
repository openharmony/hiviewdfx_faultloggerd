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

/* This files contains process dump entry function. */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <securec.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <getopt.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "directory_ex.h"
#include "dump_catcher.h"
#include "dfx_dump_catcher.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif

static const std::string DUMP_STACK_TAG_USAGE = "Usage:";
static const std::string DUMP_STACK_TAG_FAILED = "Failed:";

static void PrintCommandHelp()
{
    std::cout << DUMP_STACK_TAG_USAGE << std::endl;
    std::cout << "-p pid -t tid    dump the stacktrace of the thread with given tid." << std::endl;
    std::cout << "-p pid    dump the stacktrace of all the threads with given pid." << std::endl;
    std::cout << "[-c -m -k]    optional parameter, -c(cpp) -m(mix) -k(kernel)." << std::endl;
}

static void PrintCommandFailed()
{
    std::cout << DUMP_STACK_TAG_FAILED << std::endl;
    std::cout << "pid and tid must > 0." << std::endl;
}

static int ParseParamters(int argc, char *argv[], int &type, int32_t &pid, int32_t &tid)
{
    int ret = 0;
    if (argc <= 1) {
        return ret;
    }
    DFXLOG_DEBUG("argc: %d, argv1: %s", argc, argv[1]);

    int optRet;
    const char *optString = "cmkp:t:";
    while ((optRet = getopt(argc, argv, optString)) != -1) {
        switch (optRet) {
            case 'c':
                if ((type != OHOS::HiviewDFX::DUMP_TYPE_KERNEL) && (type != OHOS::HiviewDFX::DUMP_TYPE_MIX)) {
                    type = OHOS::HiviewDFX::DUMP_TYPE_NATIVE;
                }
                break;
            case 'm':
                if (type != OHOS::HiviewDFX::DUMP_TYPE_KERNEL) {
                    type = OHOS::HiviewDFX::DUMP_TYPE_MIX;
                }
                break;
            case 'k':
                type = OHOS::HiviewDFX::DUMP_TYPE_KERNEL;
                break;
            case 'p':
                ret = 0;
                if (optarg != nullptr) {
                    if (atoi(optarg) > 0) {
                        ret = 1;
                        pid = atoi(optarg);
                    } else {
                        ret = -1;
                        PrintCommandFailed();
                    }
                }
                break;
            case 't':
                if (optarg != nullptr) {
                    if (atoi(optarg) > 0) {
                        tid = atoi(optarg);
                    } else {
                        ret = -1;
                        PrintCommandFailed();
                    }
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

    int32_t type = OHOS::HiviewDFX::DUMP_TYPE_NATIVE;
    int32_t pid = 0;
    int32_t tid = 0;

    alarm(PROCESSDUMP_TIMEOUT); // wait 30s for process dump done
    setsid();

    if (ParseParamters(argc, argv, type, pid, tid) <= 0) {
        return 0;
    }

    DFXLOG_DEBUG("type: %d, pid: %d, tid: %d", type, pid, tid);
    OHOS::HiviewDFX::DumpCatcher::GetInstance().Dump(type, pid, tid);
    return 0;
}
