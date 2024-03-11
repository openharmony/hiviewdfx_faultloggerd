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

#include "catchframe_local.h"

#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

#include <dirent.h>
#include <unistd.h>

#include <sys/types.h>

#include "backtrace_local_context.h"
#include "backtrace_local_thread.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_symbols.h"
#include "dfx_util.h"
#include "securec.h"
#include "strings.h"
#include "libunwind.h"
#include "libunwind_i-ohos.h"


// forward declaration
struct unw_addr_space;
typedef struct unw_addr_space *unw_addr_space_t;
namespace OHOS {
namespace HiviewDFX {
unw_addr_space_t as_ {nullptr};
std::shared_ptr<DfxSymbols> symbol_ {nullptr};

DfxCatchFrameLocal::DfxCatchFrameLocal()
{
    pid_ = 0;
    (void)GetProcStatus(procInfo_);
}

DfxCatchFrameLocal::DfxCatchFrameLocal(int32_t pid) : pid_(pid)
{
    (void)GetProcStatus(procInfo_);
}

DfxCatchFrameLocal::~DfxCatchFrameLocal()
{
    BacktraceLocalContext::GetInstance().CleanUp();
}

bool DfxCatchFrameLocal::InitFrameCatcher()
{
    std::unique_lock<std::mutex> lck(mutex_);
    if (as_ != nullptr) {
        return true;
    }

    unw_init_local_address_space(&as_);
    if (as_ == nullptr) {
        return false;
    }

    symbol_ = std::make_shared<DfxSymbols>();
    return true;
}

void DfxCatchFrameLocal::DestroyFrameCatcher()
{
    std::unique_lock<std::mutex> lck(mutex_);
    if (as_ != nullptr) {
        unw_destroy_local_address_space(as_);
        as_ = nullptr;
    }
    symbol_ = nullptr;
}

bool DfxCatchFrameLocal::ReleaseThread(int tid)
{
    BacktraceLocalContext::GetInstance().ReleaseThread(tid);
    return true;
}

bool DfxCatchFrameLocal::CatchFrame(std::map<int, std::vector<DfxFrame>>& mapFrames, bool releaseThread)
{
    if (as_ == nullptr || symbol_ == nullptr) {
        return false;
    }

    if (pid_ != procInfo_.pid) {
        DFXLOG_ERROR("%s", "CatchFrame :: only support local pid.");
        return false;
    }

    std::vector<int> tids;
    std::vector<int> nstids;
    if (!GetTidsByPid(pid_, tids, nstids)) {
        return false;
    }

    std::vector<DfxFrame> frames;
    for (size_t i = 0; i < nstids.size(); ++i) {
        if (tids[i] == gettid()) {
            CatchFrameCurrTid(frames, 0, releaseThread);
        } else {
            CatchFrameLocalTid(nstids[i], frames, 0, releaseThread);
        }
        mapFrames[nstids[i]] = frames;
    }
    return (mapFrames.size() > 0);
}

bool DfxCatchFrameLocal::CatchFrame(int tid, std::vector<DfxFrame>& frames, int skipFrames, bool releaseThread)
{
    if (as_ == nullptr || symbol_ == nullptr) {
        return false;
    }

    if (tid <= 0 || pid_ != procInfo_.pid) {
        DFXLOG_ERROR("%s", "CatchFrame :: only support local pid.");
        return false;
    }

    if (tid == gettid()) {
        return CatchFrameCurrTid(frames, skipFrames, releaseThread);
    }

    int nstid = tid;
    if (procInfo_.ns) {
        TidToNstid(pid_, tid, nstid);
    } else {
        if (!IsThreadInPid(pid_, nstid)) {
            DFXLOG_ERROR("%s", "CatchFrame :: target tid is not in our task.");
            return false;
        }
    }
    return CatchFrameLocalTid(nstid, frames, skipFrames, releaseThread);
}

bool DfxCatchFrameLocal::CatchFrameCurrTid(std::vector<DfxFrame>& frames, int skipFrames, bool releaseThread)
{
    std::unique_lock<std::mutex> lck(mutex_);

    int skipFrameNum = 1; // skip current frame
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
    if (!thread.Unwind(as_, symbol_, skipFrameNum + skipFrames, false, releaseThread)) {
        return false;
    }

    frames.clear();
    frames = thread.GetFrames();
    return true;
}

bool DfxCatchFrameLocal::CatchFrameLocalTid(int tid, std::vector<DfxFrame>& frames, int skipFrames, bool releaseThread)
{
    std::unique_lock<std::mutex> lck(mutex_);

    int skipFrameNum = 1; // skip current frame
    BacktraceLocalThread thread(tid);
    if (!thread.Unwind(as_, symbol_, skipFrameNum + skipFrames, false, releaseThread)) {
        return false;
    }

    frames.clear();
    frames = thread.GetFrames();
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
