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
#include <sys/uio.h>

#include "dfx_ark.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_symbols.h"
#include "dfx_util.h"
#include "unwinder.h"
#endif

namespace OHOS {
namespace HiviewDFX {

#if is_ohos && !is_mingw && __aarch64__
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "FpBacktrace"
#define LOG_DOMAIN 0xD002D11

class FpBacktraceImpl : public FpBacktrace {
public:
    bool Init();
    uint32_t BacktraceFromFp(void* startFp, void** pcArray, uint32_t size) override;
    DfxFrame* SymbolicAddress(void* pc) override;
private:
    bool ReadProcMem(const uint64_t addr, void* data, size_t size) const;
    std::shared_ptr<DfxMaps> maps_ = nullptr;
    Unwinder unwinder_{false};
    uintptr_t arkStubBegin_ = 0;
    uintptr_t arkStubEnd_ = 0;
    std::map<void*, std::unique_ptr<DfxFrame>> cachedFrames_;
    bool withArk_ = false;
    pid_t pid_ = 0;
    std::mutex mutex_;
};

bool FpBacktraceImpl::ReadProcMem(const uint64_t addr, void* data, size_t size) const
{
    uint64_t currentAddr = addr;
    if (__builtin_add_overflow(currentAddr, size, &currentAddr)) {
        return false;
    }
    struct iovec remoteIov = {
        .iov_base = reinterpret_cast<void*>(addr),
        .iov_len = size,
    };
    struct iovec dataIov = {
        .iov_base = static_cast<uint8_t*>(data),
        .iov_len = size,
    };
    ssize_t readCount = process_vm_readv(pid_, &dataIov, 1, &remoteIov, 1, 0);
    return static_cast<size_t>(readCount) == size;
}

bool FpBacktraceImpl::Init()
{
    maps_ = DfxMaps::Create(0, false);
    if (maps_ == nullptr) {
        DFXLOGI("failed creat maps");
        return false;
    }
    withArk_ = maps_->GetArkStackRange(arkStubBegin_, arkStubEnd_) &&
        DfxArk::Instance().InitArkFunction(ArkFunction::STEP_ARK);
    pid_ = getpid();
    return true;
}

uint32_t FpBacktraceImpl::BacktraceFromFp(void* startFp, void** pcArray, uint32_t size)
{
    uint32_t index = 0;
    bool isJsFrame = false;
    constexpr auto pcIndex = 1;
    constexpr auto fpIndex = 0;
    uintptr_t registerState[] = {reinterpret_cast<uintptr_t>(startFp), 0};
    uintptr_t sp = 0 ;
    while (index < size) {
        uintptr_t preFp = registerState[fpIndex] ;
        if (isJsFrame ||
            (withArk_ && registerState[pcIndex] >= arkStubBegin_ && registerState[pcIndex] < arkStubEnd_)) {
            ArkStepParam arkParam(&registerState[fpIndex], &sp, &registerState[pcIndex], &isJsFrame);
            DfxArk::Instance().StepArkFrame(this, [](void* fpBacktrace, uintptr_t addr, uintptr_t* val) {
                return reinterpret_cast<FpBacktraceImpl*>(fpBacktrace)->ReadProcMem(addr, val, sizeof(uintptr_t));
            }, &arkParam);
        } else {
            if (!ReadProcMem(registerState[fpIndex], registerState, sizeof(registerState))) {
                break;
            }
        }
        constexpr auto pcBound = 0x1000;
        if (registerState[pcIndex] <= pcBound) {
            break;
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
    frame->mapName = frame->map->GetElfName();
    frame->relPc = frame->map->GetRelPc(frame->pc);
    frame->mapOffset = frame->map->offset;
    auto elf = frame->map->GetElf();
    if (elf == nullptr) {
        unwinder_.FillJsFrame(*frame);
        if (!frame->funcName.empty()) {
            return frame.get();
        }
    }
    DfxSymbols::GetFuncNameAndOffsetByPc(frame->relPc, elf, frame->funcName, frame->funcOffset);
    frame->buildId = elf->GetBuildId();
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
