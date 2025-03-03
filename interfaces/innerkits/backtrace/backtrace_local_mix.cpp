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

namespace OtherThread {
int GetMapByPc(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    return instance.GetMapByPc(pc, map);
}

int FindUnwindTable(uintptr_t pc, UnwindTableInfo& outTableInfo, void *arg)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    return instance.FindUnwindTable(pc, outTableInfo);
}

int AccessMem(uintptr_t addr, uintptr_t *val, void *arg)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    return instance.AccessMem(addr, val);
}
}

bool GetBacktraceStringByTidWithMix(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
    size_t maxFrameNums, bool enableKernelStack)
{
#if defined(__arm__)
    fast = false;
#endif
    std::vector<DfxFrame> frames;
    std::shared_ptr<Unwinder> unwinder = nullptr;
    if ((tid == gettid()) || (tid == BACKTRACE_CURRENT_THREAD)) {
        unwinder = std::make_shared<Unwinder>();
    } else {
        std::shared_ptr<UnwindAccessors> accssors = std::make_shared<UnwindAccessors>();
        accssors->AccessReg = nullptr;
        accssors->AccessMem = &OtherThread::AccessMem;
        accssors->GetMapByPc = &OtherThread::GetMapByPc;
        accssors->FindUnwindTable = &OtherThread::FindUnwindTable;
        unwinder = std::make_shared<Unwinder>(accssors, true);
    }
    BacktraceLocalThread thread(tid, unwinder);
    bool ret = thread.UnwindOtherThreadMix(fast, maxFrameNums, skipFrameNum + 1);
    frames = thread.GetFrames();

    if (!ret && enableKernelStack) {
        std::string msg = "";
        DfxThreadStack threadStack;
        if (DfxGetKernelStack(tid, msg) == 0 && FormatThreadKernelStack(msg, threadStack)) {
            frames = threadStack.frames;
            ret = true;
            DFXLOGI("Failed to get tid(%{public}d) user stack, try kernel", tid);
            if (IsBetaVersion()) {
                DFXLOGI("%{public}s", msg.c_str());
            }
        }
    }
    if (ret) {
        std::string threadHead = GetThreadHead(tid);
        out = threadHead + Unwinder::GetFramesStr(frames);
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
