/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

/* This files contains process dump elf module. */

#include "dfx_fault_stack.h"

#include <algorithm>
#include <csignal>
#include <sys/ptrace.h>
#include "dfx_config.h"
#include "dfx_logger.h"
#include "dfx_ring_buffer_wrapper.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr size_t MEM_BLOCK_BUF_SZ = 64;
#if defined(__arm__)
constexpr uint64_t STEP = 4;
#elif defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
constexpr uint64_t STEP = 8;
#else
constexpr uint64_t STEP = 8;
#endif
}
bool FaultStack::ReadTargetMemory(uintptr_t addr, uintptr_t &value) const
{
    uintptr_t targetAddr = addr;
    auto retAddr = reinterpret_cast<long*>(&value);
    for (size_t i = 0; i < sizeof(uintptr_t) / sizeof(long); i++) {
        *retAddr = ptrace(PTRACE_PEEKTEXT, tid_, reinterpret_cast<void*>(targetAddr), nullptr);
        if (*retAddr == -1) {
            return false;
        }
        targetAddr += sizeof(long);
        retAddr += 1;
    }

    return true;
}

uintptr_t FaultStack::AdjustAndCreateMemoryBlock(size_t index, uintptr_t prevSp, uintptr_t prevEndAddr, uintptr_t size)
{
    uintptr_t lowAddrLength = DfxConfig::GetInstance().GetFaultStackLowAddressStep();
    uintptr_t startAddr = prevSp - lowAddrLength * STEP;
    if (prevEndAddr != 0 && startAddr <= prevEndAddr) {
        startAddr = prevEndAddr;
    } else {
        size += lowAddrLength;
    }

    if (size == 0) {
        return prevEndAddr;
    }

    std::string name = "sp" + std::to_string(index - 1);
    CreateMemoryBlock(startAddr, prevSp, size, name);
    return startAddr + size * STEP;
}

bool FaultStack::CollectStackInfo(std::shared_ptr<DfxRegs> reg, const std::vector<std::shared_ptr<DfxFrame>> &frames)
{
    if (reg == nullptr && frames.empty()) {
        DfxLogWarn("null register or frames.");
        return false;
    }

    size_t index = 1;
    uintptr_t minAddr = 4096;
    uintptr_t size = 0;
    uintptr_t prevSp = 0;
    uintptr_t curSp = 0;
    uintptr_t prevEndAddr = 0;
    uintptr_t highAddrLength = DfxConfig::GetInstance().GetFaultStackHighAddressStep();
    auto firstFrame = frames.at(0);
    if (firstFrame != nullptr) {
        prevSp = static_cast<uintptr_t>(firstFrame->GetFrameSp());
    }
    constexpr size_t MAX_FAULT_STACK_SZ = 4;
    for (index = 1; index < frames.size(); index++) {
        if (index > MAX_FAULT_STACK_SZ) {
            break;
        }

        auto frame = frames.at(index);
        if (frame != nullptr) {
            curSp = static_cast<uintptr_t>(frame->GetFrameSp());
        }

        size = 0;
        if (curSp > prevSp) {
            size = std::min(highAddrLength, static_cast<uintptr_t>(((curSp - prevSp) / STEP) - 1));
        } else {
            break;
        }

        prevEndAddr = AdjustAndCreateMemoryBlock(index, prevSp, prevEndAddr, size);
        prevSp = curSp;
    }

    if ((blocks_.size() < MAX_FAULT_STACK_SZ) && (prevSp > minAddr)) {
        size = highAddrLength;
        AdjustAndCreateMemoryBlock(index, prevSp, prevEndAddr, size);
    }
    return true;
}

uintptr_t FaultStack::PrintMemoryBlock(const MemoryBlockInfo& info) const
{
#if defined(__arm__)
#define PRINT_FORMAT "%08x"
#elif defined(__aarch64__) || (defined(__riscv) && __riscv_xlen == 64)
#define PRINT_FORMAT "%016llx"
#else
#define PRINT_FORMAT "%016llx"
#endif
    uintptr_t targetAddr = info.startAddr;
    for (uint64_t i = 0; i < static_cast<uint64_t>(info.content.size()); i++) {
        if (targetAddr == info.nameAddr) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("%s:" PRINT_FORMAT " " PRINT_FORMAT "\n",
                info.name.c_str(),
                targetAddr,
                info.content.at(i));
        } else {
            DfxRingBufferWrapper::GetInstance().AppendBuf("    " PRINT_FORMAT " " PRINT_FORMAT "\n",
                targetAddr,
                info.content.at(i));
        }
        targetAddr += STEP;
    }
    return targetAddr;
}

void FaultStack::Print() const
{
    if (blocks_.empty()) {
        return;
    }

    DfxRingBufferWrapper::GetInstance().AppendMsg("FaultStack:\n");
    uintptr_t end = 0;
    for (const auto& block : blocks_) {
        if (end != 0 && end < block.startAddr) {
            DfxRingBufferWrapper::GetInstance().AppendMsg("    ...\n");
        }
        end = PrintMemoryBlock(block);
    }
}

void FaultStack::CreateMemoryBlock(uintptr_t addr, uintptr_t offset, uintptr_t size, std::string name)
{
    DfxLogDebug("CreateMemoryBlock %p %p %llu %s.", addr, offset, size, name.c_str());
    MemoryBlockInfo info;
    info.startAddr = addr;
    info.nameAddr = offset;
    info.size = size;
    info.name = name;
    uintptr_t targetAddr = addr;
    for (uint64_t i = 0; i < static_cast<uint64_t>(size); i++) {
        uintptr_t value = 0;
        if (ReadTargetMemory(targetAddr, value)) {
            info.content.push_back(value);
        } else {
            info.content.push_back(-1);
        }
        targetAddr += STEP;
    }
    blocks_.push_back(info);
}
} // namespace HiviewDFX
} // namespace OHOS
