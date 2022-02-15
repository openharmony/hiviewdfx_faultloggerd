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
#ifndef DFX_DUMPCATCH_LOCAL_DUMPER_H
#define DFX_DUMPCATCH_LOCAL_DUMPER_H
#include <cinttypes>
#include <csignal>
#include <cstring>
#include <string>
#include <unistd.h>
#include <ucontext.h>

#include "dfx_define.h"
#include "dfx_dump_writer.h"
#include "dfx_frames.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_thread.h"
#include "dfx_util.h"

static const int NUMBER_ONE = 1;
static const int NUMBER_TWO = 2;
static const int SLEEP_TIME_TEN_S = 10;
static const int SLEEP_TIME_TWENTY_S = 20;

namespace OHOS {
namespace HiviewDFX {
struct LocalDumperRequest {
    int32_t type;
    int32_t tid;
    int32_t pid;
    int32_t uid;
    uint64_t reserved;
    uint64_t timeStamp;
    siginfo_t siginfo;
    ucontext_t context;
};
class DfxDumpCatcherLocalDumper {
public:
    DfxDumpCatcherLocalDumper();
    ~DfxDumpCatcherLocalDumper();
    static void DFX_InstallLocalDumper(int sig);
    static void DFX_UninstallLocalDumper(int sig);
    static bool ExecLocalDump(const int pid, const int tid, const int skipFramNum);
    static long WriteDumpInfo(long current_position, size_t index, std::shared_ptr<DfxFrames> frame);

    static char* g_StackInfo_;
    static long long g_CurrentPosition;
    static void DFX_LocalDumperUnwindLocal(int sig, siginfo_t *si, void *context);
    static void DFX_LocalDumper(int sig, siginfo_t *si, void *context);
};
}
}
#endif