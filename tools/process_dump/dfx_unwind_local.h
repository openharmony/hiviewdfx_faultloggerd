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
#ifndef DFX_UNWIND_LOCAL_H
#define DFX_UNWIND_LOCAL_H
#include <chrono>
#include <cinttypes>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <libunwind-ptrace.h>
#include <libunwind.h>

#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_frame.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_thread.h"
#include "dfx_process.h"
#include "dfx_symbols_cache.h"
#include "dfx_util.h"
#include "nocopyable.h"

namespace OHOS {
namespace HiviewDFX {
class DfxUnwindLocal {
public:
    static DfxUnwindLocal &GetInstance();
    ~DfxUnwindLocal() = default;

    bool Init();
    void Destroy();

    void InstallLocalDumper(int sig);
    void UninstallLocalDumper(int sig);
    void LocalDumperUnwind(int sig, siginfo_t *si, void *context);
    bool ExecLocalDumpUnwind(int tid, size_t skipFramNum);
    void ResolveFrameInfo(size_t index, DfxFrame& frame);
    std::string CollectUnwindResult(int32_t tid);
    int WriteUnwindResult(int fd, const std::string msg);
    void CollectUnwindFrames(std::vector<std::shared_ptr<DfxFrame>>& frames);
    bool SendLocalDumpRequest(int32_t tid);
    bool WaitLocalDumpRequest();

private:
    DfxUnwindLocal();
    DISALLOW_COPY_AND_MOVE(DfxUnwindLocal);

    static void LocalDumpering(int sig, siginfo_t *si, void *context);

private:
    unw_addr_space_t as_;
    std::vector<DfxFrame> frames_;
    uint32_t curIndex_;
    std::unique_ptr<DfxSymbolsCache> cache_;
    sigset_t mask_;
    struct sigaction oldSigaction_;
    struct LocalDumperRequest localDumpRequest_;
    std::condition_variable localDumperCV_;
    std::mutex localDumperMutex_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // DFX_UNWIND_LOCAL_H
