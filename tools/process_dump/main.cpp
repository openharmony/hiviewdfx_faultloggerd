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

#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "lite_perf_dumper.h"
#include "process_dumper.h"

#include "lite_process_dumper.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif

static const int DUMP_ARG_ONE = 1;
static const std::string DUMP_STACK_TAG_USAGE = "usage:";
static const std::string DUMP_STACK_TAG_FAILED = "failed:";

static long g_pid = 0;

static void PrintCommandHelp()
{
    printf("%s\nplease use dumpcatcher\n", DUMP_STACK_TAG_USAGE.c_str());
}

static bool StartLitePerf(int argc, char *argv[])
{
    if (argc < 3) { // 3 : contain type requestFd
        return false;
    }
    long requestFd;
    if (!SafeStrtol(argv[2], &requestFd, DECIMAL_BASE)) { // 2 : the index of requestFd
        return false;
    }
#ifdef DFX_ENABLE_LPERF
    OHOS::HiviewDFX::LitePerfDumper::GetInstance().Perf(static_cast<int>(requestFd));
#endif
    return true;
}

static bool ParseParameters(int argc, char *argv[], bool &isSignalHdlr, bool &isRender)
{
    if (argc <= DUMP_ARG_ONE) {
        return false;
    }
    DFXLOGD("[%{public}d]: argc: %{public}d, argv1: %{public}s", __LINE__, argc, argv[1]);

    if (!strcmp("-signalhandler", argv[DUMP_ARG_ONE])) {
        isSignalHdlr = true;
        return true;
    }
    if (!strcmp("-liteperf", argv[DUMP_ARG_ONE])) {
        return StartLitePerf(argc, argv);
    }
    if (!strcmp("-render", argv[DUMP_ARG_ONE])) {
        if (argc < 3) { // 3 : contain type pid
            return false;
        }
        SafeStrtol(argv[2], &g_pid, DECIMAL_BASE); // 2 : the index of pid
        isRender = true;
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    DFXLOGI("Start main function of processdump");
#if defined(DEBUG_CRASH_LOCAL_HANDLER) && !defined(DFX_ALLOCATE_ASAN)
    DFX_InstallLocalSignalHandler();
#endif
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        DFXLOGE("Processdump ignore SIGCHLD failed.");
    }

    bool isSignalHdlr = false;
    bool isRender = false;

    setsid();

    if (!ParseParameters(argc, argv, isSignalHdlr, isRender)) {
        PrintCommandHelp();
        return 0;
    }

    if (isSignalHdlr) {
        alarm(PROCESSDUMP_TIMEOUT);
        OHOS::HiviewDFX::ProcessDumper::GetInstance().Dump();
    } else if (isRender) {
        DFXLOGI("start lite processdump");
        OHOS::HiviewDFX::LiteProcessDumper liteProcessDumper;
        liteProcessDumper.Dump(static_cast<int>(g_pid));
    } else {
        DFXLOGI("invalid param");
    }
#ifndef CLANG_COVERAGE
    _exit(0);
#endif
    return 0;
}
