/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "backtrace_local.h"

#include <cstring>
#include <unistd.h>

#include <mutex>
#include <vector>

#include "backtrace_local_thread.h"
#include "dfx_frame_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "directory_ex.h"
#include "procinfo.h"
#include "thread_context.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktrace"
#define LOG_DOMAIN 0xD002D11
}

bool GetBacktraceStringByTidWithMix(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
    size_t maxFrameNums, bool enableKernelStack)
{
#if defined(__x86_64__)
    return false;
#else
#if defined(__arm__)
    fast = false;
#endif
    std::shared_ptr<Unwinder> unwinder = nullptr;
    if ((tid == gettid()) || (tid == BACKTRACE_CURRENT_THREAD)) {
        unwinder = std::make_shared<Unwinder>();
    } else {
        unwinder = std::make_shared<Unwinder>(LocalThreadContextMix::CreateAccessors(), true);
    }
    std::vector<DfxFrame> frames{};
    bool ret = false;
    ProcessInfo info;
    std::string processInfoStr = "";
    if (ParseProcInfo(tid, info)) {
        processInfoStr = FomatProcessInfoToString(info) + "\n";
    }
    if (unwinder) {
        BacktraceLocalThread thread(tid);
        ret = thread.UnwindOtherThreadMix(*unwinder, fast, maxFrameNums, skipFrameNum + 1);
        frames = thread.GetFrames();
    }
    if (!ret && enableKernelStack) {
        std::string msg = "";
        DfxThreadStack threadStack;
        if (DfxGetKernelStack(tid, msg) == 0 && FormatThreadKernelStack(msg, threadStack)) {
            frames = std::move(threadStack.frames);
            ret = true;
            DFXLOGI("Failed to get tid(%{public}d) user stack, try kernel", tid);
        }
        if (IsBetaVersion()) {
            DFXLOGI("%{public}s", msg.c_str());
        }
    }
    if (ret) {
        std::string threadHead = GetThreadHead(tid);
        out = threadHead + processInfoStr + Unwinder::GetFramesStr(frames);
    }
    return ret;
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
