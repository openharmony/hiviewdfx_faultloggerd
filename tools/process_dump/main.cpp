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
#include "dfx_define.h"
#include "dfx_log.h"
#include "faultloggerd_client.h"
#include "process_dumper.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif

static const int DUMP_ARG_ONE = 1;
static const std::string DUMP_STACK_TAG_USAGE = "usage:";
static const std::string DUMP_STACK_TAG_FAILED = "failed:";

static void PrintCommandHelp()
{
    std::cout << DUMP_STACK_TAG_USAGE << std::endl;
    std::cout << "please use dumpcatcher" << std::endl;
}

static bool ParseParameters(int argc, char *argv[], bool &isSignalHdlr)
{
    if (argc <= DUMP_ARG_ONE) {
        return false;
    }
    DFXLOG_DEBUG("argc: %d, argv1: %s", argc, argv[1]);

    if (!strcmp("-signalhandler", argv[DUMP_ARG_ONE])) {
        isSignalHdlr = true;
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
#if defined(DEBUG_CRASH_LOCAL_HANDLER)
    DFX_InstallLocalSignalHandler();
#endif
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        DFXLOG_ERROR("Processdump ignore SIGCHLD failed.");
    }

    bool isSignalHdlr = false;

    alarm(PROCESSDUMP_TIMEOUT); // wait 30s for process dump done
    setsid();

    if (!ParseParameters(argc, argv, isSignalHdlr)) {
        PrintCommandHelp();
        return 0;
    }

    if (isSignalHdlr) {
        OHOS::HiviewDFX::ProcessDumper::GetInstance().Dump();
    }
    return 0;
}
