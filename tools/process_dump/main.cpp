/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include <cstring>
#include <securec.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "process_dumper.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif

static const int DUMP_ARG_ONE = 1;
static const std::string DUMP_STACK_TAG_USAGE = "usage:";
static const std::string DUMP_STACK_TAG_FAILED = "failed:";

static void PrintCommandHelp()
{
    printf("%s\nplease use dumpcatcher\n", DUMP_STACK_TAG_USAGE.c_str());
}

static bool ParseParameters(int argc, char *argv[], bool &isSignalHdlr)
{
    if (argc <= DUMP_ARG_ONE) {
        return false;
    }
    LOGDEBUG("argc: %{public}d, argv1: %{public}s", argc, argv[1]);

    if (!strcmp("-signalhandler", argv[DUMP_ARG_ONE])) {
        isSignalHdlr = true;
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    LOGINFO("Start main function of processdump");
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
    DFX_InstallLocalSignalHandler();
#endif
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        LOGERROR("Processdump ignore SIGCHLD failed.");
    }

    bool isSignalHdlr = false;

    alarm(PROCESSDUMP_TIMEOUT);
    LOGINFO("Start alarm %{public}d seconds for processdump", PROCESSDUMP_TIMEOUT);
    setsid();

    if (!ParseParameters(argc, argv, isSignalHdlr)) {
        PrintCommandHelp();
        return 0;
    }

    if (isSignalHdlr) {
        OHOS::HiviewDFX::ProcessDumper::GetInstance().Dump();
    }
#ifndef CLANG_COVERAGE
    _exit(0);
#endif
    return 0;
}
