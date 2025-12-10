/*
* Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "fp_backtrace.h"

#if is_ohos && !is_mingw && __aarch64__
#include <mutex>
#include "dfx_ark.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_symbols.h"
#include "dfx_util.h"
#include "memory_reader.h"
#include "smart_fd.h"
#include "stack_utils.h"
#include "unwinder.h"
#endif

namespace OHOS {
namespace HiviewDFX {

#if is_ohos && !is_mingw && __aarch64__
namespace {

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "FpBacktrace"
#define LOG_DOMAIN 0xD002D11
}

extern "C" __attribute__((weak)) bool ffrt_get_current_coroutine_stack(void** stackAddr, size_t* size)
{
    return false;
}

class FpBacktraceImpl : public FpBacktrace {
public:
    bool Init();
    uint32_t BacktraceFromFp(void* startFp, void** pcArray, uint32_t size) override;
    DfxFrame* SymbolicAddress(void* pc) override;
private:
    uint32_t BacktraceFromFp(void* startFp, void** pcArray, uint32_t size, MemoryReader& memoryReader) const;
    bool GetCurrentThreadRange(uintptr_t startFp, uintptr_t& threadBegin, uintptr_t& threadEnd);
    std::shared_ptr<DfxMaps> maps_ = nullptr;
    Unwinder unwinder_{false};
    uintptr_t mainStackBegin_{0};
    uintptr_t mainStackEnd_{0};
    std::map<void*, std::unique_ptr<DfxFrame>> cachedFrames_;
    std::mutex mutex_;
};

bool FpBacktraceImpl::Init()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        void* stackBegin = 0;
        size_t stackSize = 0;
        ffrt_get_current_coroutine_stack(&stackBegin, &stackSize);
        DfxArk::Instance().InitArkFunction(ArkFunction::STEP_ARK);
    });
    maps_ = DfxMaps::Create(0, false);
    if (maps_ == nullptr) {
        DFXLOGI("failed creat maps");
        return false;
    }
    maps_->GetStackRange(mainStackBegin_, mainStackEnd_);
    return true;
}

uint32_t FpBacktraceImpl::BacktraceFromFp(void* startFp, void** pcArray, uint32_t size)
{
    if (!maps_ || startFp == nullptr || pcArray == nullptr || size == 0) {
        return 0;
    }
    uintptr_t stackBegin = 0;
    uintptr_t stackEnd = 0;
    if (GetCurrentThreadRange(reinterpret_cast<uintptr_t>(startFp), stackBegin, stackEnd)) {
        ThreadMemoryReader memoryReader(stackBegin, stackEnd);
        return BacktraceFromFp(startFp, pcArray, size, memoryReader);
    }
    ProcessMemoryReader memoryReader;
    return BacktraceFromFp(startFp, pcArray, size, memoryReader);
}

uint32_t FpBacktraceImpl::BacktraceFromFp(void* startFp, void** pcArray, uint32_t size,
    MemoryReader& memoryReader) const
{
    uint32_t index = 0;
    bool isJsFrame = false;
    uintptr_t registerState[] = {reinterpret_cast<uintptr_t>(startFp), 0};
    uintptr_t sp = 0 ;
    uintptr_t arkStubBegin{0};
    uintptr_t arkStubEnd{0};
    maps_->GetArkStackRange(arkStubBegin, arkStubEnd);
    while (index < size) {
        constexpr auto fpIndex = 0;
        constexpr auto pcIndex = 1;
        uintptr_t preFp = registerState[fpIndex] ;
        if (isJsFrame || (registerState[pcIndex] < arkStubEnd && registerState[pcIndex] >= arkStubBegin)) {
            ArkStepParam arkParam(&registerState[fpIndex], &sp, &registerState[pcIndex], &isJsFrame);
            DfxArk::Instance().StepArkFrame(&memoryReader, [](void* memoryReader, uintptr_t addr, uintptr_t* val) {
                return reinterpret_cast<MemoryReader*>(memoryReader)->ReadMemory(addr, val, sizeof(uintptr_t));
            }, &arkParam);
        } else {
            if (!memoryReader.ReadMemory(registerState[fpIndex], registerState, sizeof(registerState))) {
                break;
            }
        }
        constexpr auto pcBound = 0x1000;
        if (registerState[pcIndex] <= pcBound) {
            if (index != 0) {
                break;
            }
            // when fp back trace first frame failed, use the ark interface try again
            registerState[fpIndex] = reinterpret_cast<uintptr_t>(startFp);
            ArkStepParam arkParam(&registerState[fpIndex], &sp, &registerState[pcIndex], &isJsFrame);
            DfxArk::Instance().StepArkFrame(&memoryReader, [](void* memoryReader, uintptr_t addr, uintptr_t* val) {
                return reinterpret_cast<MemoryReader*>(memoryReader)->ReadMemory(addr, val, sizeof(uintptr_t));
            }, &arkParam);
        }
        auto realPc = reinterpret_cast<void *>(StripPac(registerState[pcIndex], 0));
        if (realPc != nullptr) {
            pcArray[index++] = realPc;
        }
        if (registerState[fpIndex] <= preFp || registerState[fpIndex] == 0) {
            break;
        }
    }
    return index;
}

bool FpBacktraceImpl::GetCurrentThreadRange(uintptr_t startFp, uintptr_t& threadBegin, uintptr_t& threadEnd)
{
    if (getpid() == gettid() && startFp >= mainStackBegin_ && startFp < mainStackEnd_) {
        threadBegin = mainStackBegin_;
        threadEnd = mainStackEnd_;
        return true;
    }
    if (StackUtils::GetSelfStackRange(threadBegin, threadEnd)) {
        if (startFp >= threadBegin && startFp < threadEnd) {
            return true;
        }
    }
    size_t stackSize = 0;
    if (ffrt_get_current_coroutine_stack(reinterpret_cast<void**>(&threadBegin), &stackSize)) {
        if (startFp >= threadBegin && startFp < threadBegin + stackSize) {
            threadEnd = threadBegin + stackSize;
            return true;
        }
    }
    return false;
}

DfxFrame* FpBacktraceImpl::SymbolicAddress(void* pc)
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto cachedFrame = cachedFrames_.emplace(pc, nullptr);
    auto& frame = cachedFrame.first->second;
    if (!cachedFrame.second) {
        return frame.get();
    }
    frame = std::unique_ptr<DfxFrame>(new (std::nothrow) DfxFrame());
    if (!frame) {
        return nullptr;
    }
    unwinder_.GetFrameByPc(reinterpret_cast<uintptr_t>(pc), maps_, *(frame));
    if (frame->map == nullptr) {
        frame = nullptr;
        return nullptr;
    }
    
    frame->relPc = frame->map->GetRelPc(frame->pc);
    frame->mapOffset = frame->map->offset;
    auto elf = frame->map->GetElf();
    if (elf == nullptr) {
        unwinder_.FillJsFrame(*frame);
    } else {
        DfxSymbols::GetFuncNameAndOffsetByPc(frame->relPc, elf, frame->funcName, frame->funcOffset);
        frame->buildId = elf->GetBuildId();
    }
    return frame.get();
}
#endif

FpBacktrace* FpBacktrace::CreateInstance()
{
#if is_ohos && !is_mingw && __aarch64__
    auto fpBacktraceImpl =  new (std::nothrow) FpBacktraceImpl();
    if (fpBacktraceImpl == nullptr) {
        return nullptr;
    }
    if (fpBacktraceImpl->Init()) {
        return fpBacktraceImpl;
    }
    delete fpBacktraceImpl;
#endif
    return nullptr;
}
}
}
