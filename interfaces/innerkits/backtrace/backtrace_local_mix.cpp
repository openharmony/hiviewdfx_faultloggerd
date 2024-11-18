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

#include "backtrace_local_mix.h"

#include <cstring>
#include <dirent.h>
#include <mutex>
#include <unistd.h>
#include <vector>

#include "backtrace_local_thread.h"
#include "dfx_frame_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "directory_ex.h"
#include "procinfo.h"
#include "thread_context_mix.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktrace"
#define LOG_DOMAIN 0xD002D11

std::string GetThreadHead(int32_t tid)
{
    std::string threadName;
    if (tid == BACKTRACE_CURRENT_THREAD) {
        tid = gettid();
    }
    ReadThreadName(tid, threadName);
    std::string threadHead = "Tid:" + std::to_string(tid) + ", Name:" + threadName + "\n";
    return threadHead;
}

int GetMapByPcForOtherThread(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg)
{
    UnwindContextMix* unwindContextMix = static_cast<UnwindContextMix *>(arg);
    if (unwindContextMix == nullptr) {
        return -1;
    }
    return unwindContextMix->maps->FindMapByAddr(pc, map) ? 0 : -1;
}

int FindUnwindTableForOtherThread(uintptr_t pc, UnwindTableInfo& outTableInfo, void *arg)
{
    UnwindContextMix* unwindContextMix = static_cast<UnwindContextMix *>(arg);
    if (unwindContextMix == nullptr) {
        return -1;
    }

    std::shared_ptr<DfxMap> dfxMap;
    if (unwindContextMix->maps->FindMapByAddr(pc, dfxMap)) {
        if (dfxMap == nullptr) {
            return -1;
        }
        auto elf = dfxMap->GetElf(getpid());
        if (elf != nullptr) {
            return elf->FindUnwindTableInfo(pc, dfxMap, outTableInfo);
        }
    }
    return -1;
}

int AccessMemForOtherThread(uintptr_t addr, uintptr_t *val, void *arg)
{
    UnwindContextMix* unwindContextMix = static_cast<UnwindContextMix *>(arg);
    if (unwindContextMix == nullptr) {
        return -1;
    }

    *val = 0;
    if (addr < unwindContextMix->context->sp ||
        addr + sizeof(uintptr_t) >= unwindContextMix->context->sp + STACK_BUFFER_SIZE) {
        std::shared_ptr<DfxMap> map;
        if (unwindContextMix->maps->FindMapByAddr(addr, map)) {
            if (map == nullptr) {
                return -1;
            }
            auto elf = map->GetElf(getpid());
            if (elf != nullptr) {
                uint64_t foff = addr - map->begin + map->offset - elf->GetBaseOffset();
                if (elf->Read(foff, val, sizeof(uintptr_t))) {
                    return 0;
                }
            }
        }
        return -1;
    } else {
        size_t stackOffset = addr - unwindContextMix->context->sp;
        if (stackOffset >= STACK_BUFFER_SIZE) {
            return -1;
        }
        *val = *(reinterpret_cast<uintptr_t *>(&unwindContextMix->context->buffer[stackOffset]));
    }
    return 0;
}
}

bool GetBacktraceStringByTidWithMix(std::string& out, int32_t tid, size_t skipFrameNum, bool fast,
                             size_t maxFrameNums, bool enableKernelStack)
{
    std::vector<DfxFrame> frames;
    std::shared_ptr<Unwinder> unwinder = nullptr;
    if ((tid == gettid()) || (tid == BACKTRACE_CURRENT_THREAD)) {
        unwinder = std::make_shared<Unwinder>();
    } else {
        std::shared_ptr<UnwindAccessors> accssors = std::make_shared<UnwindAccessors>();
        accssors->AccessReg = nullptr;
        accssors->AccessMem = &AccessMemForOtherThread;
        accssors->GetMapByPc = &GetMapByPcForOtherThread;
        accssors->FindUnwindTable = &FindUnwindTableForOtherThread;
        unwinder = std::make_shared<Unwinder>(accssors, true);
    }
    BacktraceLocalThread thread(tid, unwinder);
    bool ret = thread.UnwindSupportMix(fast, maxFrameNums, skipFrameNum + 1);
    frames = thread.GetFrames();

    if (!ret && enableKernelStack) {
        std::string msg = "";
        DfxThreadStack threadStack;
        if (DfxGetKernelStack(tid, msg) == 0 && FormatThreadKernelStack(msg, threadStack)) {
            frames = threadStack.frames;
            ret = true;
            DFXLOGI("Failed to get tid(%{public}d) user stack, try kernel", tid);
        }
    }
    if (ret) {
        out.clear();
        std::string threadHead = GetThreadHead(tid);
        out = threadHead + Unwinder::GetFramesStr(frames);
    }
    return ret;
}
} // namespace HiviewDFX
} // namespace OHOS
