/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include <cinttypes>

#include "dfx_log.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpContext::MinidumpContext(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : memoryReader_(memoryReader),
      arm64Context_(nullptr),
      contextFlags_(0),
      isValid_(false)
{
}

uint32_t MinidumpContext::GetContextCPU() const
{
    return isValid_ ? (contextFlags_ & MD_CONTEXT_CPU_MASK) : 0;
}

const std::shared_ptr<MDRawContextARM64> MinidumpContext::GetContextARM64() const
{
    return (GetContextCPU() == MD_CONTEXT_ARM64) ? arm64Context_ : nullptr;
}

bool MinidumpContext::GetProgramCounter(uint64_t& pc) const
{
    if (!isValid_) {
        return false;
    }
    const auto context = GetContextARM64();
    if (context == nullptr) {
        return false;
    }
    pc = context->iregs[MD_CONTEXT_ARM64_GPR_COUNT - 1];
    return true;
}

bool MinidumpContext::GetStackPointer(uint64_t& sp) const
{
    if (!isValid_) {
        return false;
    }
    const auto context = GetContextARM64();
    if (!context) {
        return false;
    }
    int spIndex = MD_CONTEXT_ARM64_GPR_COUNT - 2;
    sp = context->iregs[spIndex];
    return true;
}

bool MinidumpContext::Read(uint32_t expectedSize)
{
    arm64Context_.reset();
    isValid_ = false;
    if (expectedSize < sizeof(uint32_t)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_CONTEXT_READ, "Context size too small", __LINE__);
        return false;
    }
    uint32_t contextFlags;
    if (!memoryReader_->ReadBytes(&contextFlags, sizeof(contextFlags))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_CONTEXT_READ, "Failed to read context flags", __LINE__);
        DFXLOGE("MinidumpContext could not read context flags");
        return false;
    }

    auto context = std::make_shared<MDRawContextARM64>();
    context->contextFlags = contextFlags;
    size_t bytesToRead = std::min(static_cast<size_t>(expectedSize - sizeof(contextFlags)),
        sizeof(*context) - sizeof(context->contextFlags));
    uint8_t* contextAfterFlags = reinterpret_cast<uint8_t*>(context.get()) + sizeof(context->contextFlags);
    if (!memoryReader_->ReadBytes(contextAfterFlags, bytesToRead)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_CONTEXT_READ, "Failed to read ARM64 context", __LINE__);
        DFXLOGE("MinidumpContext cannot read context");
        return false;
    }

    SetContextFlags(contextFlags);
    SetContextARM64(std::move(context));
    isValid_ = true;
    lastError_.Clear();
    return true;
}

void MinidumpContext::Print() const
{
    if (!isValid_) {
        DFXLOGE("MinidumpContext cannot print invalid data");
        return;
    }
    const auto context = GetContextARM64();
    if (!context) {
        DFXLOGE("MinidumpContext has no ARM64 context");
        return;
    }

    DFXLOGI("MDRawContextARM64");
    DFXLOGI("  contextFlags = 0x%{public}" PRIx32, context->contextFlags);
    for (size_t i = 0; i < MD_CONTEXT_ARM64_GPR_COUNT; i++) {
        DFXLOGI("  x%{public}zu = 0x%{public}" PRIx64, i, context->iregs[i]);
    }
    DFXLOGI("  x29 (fp) = 0x%{public}" PRIx64, context->iregs[29]);
    DFXLOGI("  x30 (lr) = 0x%{public}" PRIx64, context->iregs[30]);
    DFXLOGI("  x31 (sp) = 0x%{public}" PRIx64, context->iregs[31]);
    DFXLOGI("  x32 (pc) = 0x%{public}" PRIx64, context->iregs[32]);

    DFXLOGI("  cpsr = 0x%{public}x", context->cpsr);
    DFXLOGI("  fpcr = 0x%{public}x", context->fpcr);
    DFXLOGI("  fpsr = 0x%{public}x", context->fpsr);
}

}
}