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
#include "lite_process_dumper.h"
#ifndef is_ohos_lite
#include "minidump_dumper.h"
#endif
#include "process_dumper.h"

#if defined(DEBUG_CRASH_LOCAL_HANDLER)
#include "dfx_signal_local_handler.h"
#endif

static const int DUMP_ARG_ONE = 1;
static const std::string DUMP_STACK_TAG_USAGE = "usage:";

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

static bool StartMinidump(int argc, char *argv[])
{
    if (argc < 3) { // 3 : contain param
        return false;
    }
    int pid = 0;
    int pipeFd = -1;
    int enableMinidump = 0;
    int enableMinidumpToCrashLog = 0;
    int ret = sscanf_s(argv[2], "%d %d %d %d", &pid, &pipeFd, &enableMinidump, &enableMinidumpToCrashLog);
    if (ret != 4) { // 4 : four params
        return false;
    }
#ifndef is_ohos_lite
    OHOS::HiviewDFX::MinidumpDumper minidumpDumper;
    return minidumpDumper.Dump(pid, pipeFd, static_cast<bool>(enableMinidump),
        static_cast<bool>(enableMinidumpToCrashLog));
#endif
    return true;
}

static bool ParseParameters(int argc, char *argv[])
{
    if (argc <= DUMP_ARG_ONE) {
        return false;
    }
    DFXLOGD("[%{public}d]: argc: %{public}d, argv1: %{public}s", __LINE__, argc, argv[1]);

    if (!strcmp("-signalhandler", argv[DUMP_ARG_ONE])) {
        alarm(PROCESSDUMP_TIMEOUT);
        OHOS::HiviewDFX::ProcessDumper::GetInstance().Dump();
        return true;
    }
    if (!strcmp("-liteperf", argv[DUMP_ARG_ONE])) {
        return StartLitePerf(argc, argv);
    }
    if (!strcmp("-render", argv[DUMP_ARG_ONE])) {
        if (argc < 3) { // 3 : contain type pid
            return false;
        }
        long pid = 0;
        if (!SafeStrtol(argv[2], &pid, DECIMAL_BASE)) { // 2 : the index of pid
            return false;
        }
        DFXLOGI("start lite processdump");
        OHOS::HiviewDFX::LiteProcessDumper liteProcessDumper;
        liteProcessDumper.Dump(static_cast<int>(pid));
        return true;
    }
    if (!strcmp("-minidump", argv[DUMP_ARG_ONE])) {
        alarm(PROCESSDUMP_TIMEOUT);
        return StartMinidump(argc, argv);
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
    setsid();

    if (!ParseParameters(argc, argv)) {
        DFXLOGI("invalid param");
        PrintCommandHelp();
        return 0;
    }
#ifndef CLANG_COVERAGE
    _exit(0);
#endif
    return 0;
}
