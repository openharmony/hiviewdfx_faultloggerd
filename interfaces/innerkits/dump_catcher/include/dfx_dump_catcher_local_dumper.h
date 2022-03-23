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
#include <chrono>
#include <cinttypes>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <string>
#include <thread>
#include <sstream>

#include <unistd.h>
#include <ucontext.h>

#include "dfx_define.h"
#include "dfx_dump_writer.h"
#include "dfx_dump_catcher_frame.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_thread.h"
#include "dfx_util.h"

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
    static bool ExecLocalDump(int pid, int tid, size_t skipFramNum);
    static void DFX_InstallLocalDumper(int sig);
    static void DFX_LocalDumper(int sig, siginfo_t *si, void *context);
    static void DFX_LocalDumperUnwindLocal(int sig, siginfo_t *si, void *context);
    static void DFX_UninstallLocalDumper(int sig);
    static void WriteFrameInfo(std::ostringstream& ss, size_t index, DfxDumpCatcherFrame& frame);
    static void ResolveFrameInfo(DfxDumpCatcherFrame& frame);
    static bool SendLocalDumpRequest(int32_t tid);
    static std::string CollectUnwindResult();
    static void CollectUnwindFrames(std::vector<std::shared_ptr<DfxDumpCatcherFrame>>& frames);

    static bool InitLocalDumper();
    static void DestroyLocalDumper();

    static bool g_isLocalDumperInited;
    static uint32_t g_curIndex;
    static std::condition_variable g_localDumperCV;
    static std::mutex g_localDumperMutx;
    static std::shared_ptr<DfxElfMaps> g_localDumperMaps;
    static std::vector<DfxDumpCatcherFrame> g_FrameV;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
