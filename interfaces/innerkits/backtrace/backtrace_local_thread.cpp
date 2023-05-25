/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "backtrace_local_thread.h"

#include <sstream>

#include <link.h>
#include <unistd.h>
#include <mutex>
#include <pthread.h>
#include <libunwind.h>
#include <libunwind_i-ohos.h>
#include <securec.h>

#include "backtrace_local_context.h"
#include "dfx_define.h"
#include "fp_unwinder.h"
#include "dwarf_unwinder.h"
#include "dfx_frame_format.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxBacktraceLocal"
}

BacktraceLocalThread::BacktraceLocalThread(int32_t tid) : tid_(tid)
{
    frames_.clear();
}

BacktraceLocalThread::~BacktraceLocalThread()
{
    if (tid_ != BACKTRACE_CURRENT_THREAD) {
        BacktraceLocalContext::GetInstance().CleanUp();
    }
    frames_.clear();
}

bool BacktraceLocalThread::UnwindCurrentThread(unw_addr_space_t as, std::shared_ptr<DfxSymbolsCache> cache,
    size_t skipFrameNum, bool fast)
{
    bool ret = false;
    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);

    if (fast) {
#ifdef __aarch64__
        FpUnwinder unwinder;
        ret = unwinder.UnwindWithContext(context, skipFrameNum + 1);
        unwinder.UpdateFrameInfo();
        frames_ = unwinder.GetFrames();
#endif
    }
    if (!ret) {
        DwarfUnwinder unwinder;
        ret = unwinder.UnwindWithContext(as, context, cache, skipFrameNum + 1);
        frames_ = unwinder.GetFrames();
    }
    return ret;
}

bool BacktraceLocalThread::Unwind(unw_addr_space_t as, std::shared_ptr<DfxSymbolsCache> cache,
    size_t skipFrameNum, bool fast, bool releaseThread)
{
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    bool ret = false;

    if (tid_ == BACKTRACE_CURRENT_THREAD) {
        return UnwindCurrentThread(as, cache, skipFrameNum + 1, fast);
    } else if (tid_ < BACKTRACE_CURRENT_THREAD) {
        return ret;
    }

    auto threadContext = BacktraceLocalContext::GetInstance().GetThreadContext(tid_);
    if (threadContext == nullptr) {
        return ret;
    }

    if (threadContext->ctx == nullptr) {
        // should never happen
        ReleaseThread();
        return ret;
    }

    if (!ret) {
        DwarfUnwinder unwinder;
        ret = unwinder.UnwindWithContext(as, *(threadContext->ctx), cache, skipFrameNum);
        frames_ = unwinder.GetFrames();
    }

    if (releaseThread) {
        ReleaseThread();
    }
    return ret;
}

const std::vector<DfxFrame>& BacktraceLocalThread::GetFrames() const
{
    return frames_;
}

bool BacktraceLocalThread::GetBacktraceFrames(std::vector<DfxFrame>& frames,
    int32_t tid, size_t skipFrameNum, bool fast)
{
    bool ret = false;
    BacktraceLocalThread thread(tid);
    if (fast) {
#ifdef __aarch64__
        ret = thread.Unwind(nullptr, nullptr, skipFrameNum, fast);
#endif
    }
    if (!ret) {
        unw_addr_space_t as;
        unw_init_local_address_space(&as);
        if (as == nullptr) {
            return ret;
        }
        auto cache = std::make_shared<DfxSymbolsCache>();

        ret = thread.Unwind(as, cache, skipFrameNum, fast);

        unw_destroy_local_address_space(as);
    }
    frames.clear();
    frames = thread.GetFrames();
    return ret;
}

bool BacktraceLocalThread::GetBacktraceString(std::string& out,
    int32_t tid, size_t skipFrameNum, bool fast)
{
    bool ret = false;
    BacktraceLocalThread thread(tid);
    if (fast) {
#ifdef __aarch64__
        ret = thread.Unwind(nullptr, nullptr, skipFrameNum, fast);
#endif
    }
    if (!ret) {
        unw_addr_space_t as;
        unw_init_local_address_space(&as);
        if (as == nullptr) {
            return ret;
        }
        auto cache = std::make_shared<DfxSymbolsCache>();

        ret = thread.Unwind(as, cache, skipFrameNum, fast);

        unw_destroy_local_address_space(as);
    }
    out = DfxFrameFormat::GetFramesStr(thread.GetFrames());
    return ret;
}

void BacktraceLocalThread::ReleaseThread()
{
    if (tid_ > BACKTRACE_CURRENT_THREAD) {
        BacktraceLocalContext::GetInstance().ReleaseThread(tid_);
    }
}
} // namespace HiviewDFX
} // namespace OHOS
