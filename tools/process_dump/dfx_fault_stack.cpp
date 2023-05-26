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
#define PRINT_FORMAT "%08x"
#elif defined(__aarch64__)
constexpr uint64_t STEP = 8;
#define PRINT_FORMAT "%016llx"
#else
constexpr uint64_t STEP = 8;
#define PRINT_FORMAT "%016llx"
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
    uintptr_t lowAddrLength = DfxConfig::GetConfig().lowAddressStep;
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
    auto block = CreateMemoryBlock(startAddr, prevSp, size, name);
    blocks_.push_back(block);
    return startAddr + size * STEP;
}

bool FaultStack::CollectStackInfo(const std::vector<std::shared_ptr<DfxFrame>> &frames)
{
    if (frames.empty()) {
        DFXLOG_WARN("null frames.");
        return false;
    }

    size_t index = 1;
    uintptr_t minAddr = 4096;
    uintptr_t size = 0;
    uintptr_t prevSp = 0;
    uintptr_t curSp = 0;
    uintptr_t prevEndAddr = 0;
    uintptr_t highAddrLength = DfxConfig::GetConfig().highAddressStep;
    auto firstFrame = frames.at(0);
    if (firstFrame != nullptr) {
        prevSp = static_cast<uintptr_t>(firstFrame->sp);
    }
    constexpr size_t MAX_FAULT_STACK_SZ = 4;
    for (index = 1; index < frames.size(); index++) {
        if (index > MAX_FAULT_STACK_SZ) {
            break;
        }

        auto frame = frames.at(index);
        if (frame != nullptr) {
            curSp = static_cast<uintptr_t>(frame->sp);
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
    PrintRegisterMemory();

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

MemoryBlockInfo FaultStack::CreateMemoryBlock(uintptr_t addr, uintptr_t offset, uintptr_t size, std::string name)
{
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
    return info;
}

void FaultStack::CollectRegistersBlock(std::shared_ptr<DfxRegs> regs, std::shared_ptr<DfxElfMaps> maps)
{
    if (regs == nullptr || maps == nullptr) {
        return;
    }

    auto regData = regs->GetRegsData();
    int index = 0;
    for (auto data : regData) {
        index++;
        std::shared_ptr<DfxElfMap> map;
        if (!maps->FindMapByAddr(data, map)) {
            continue;
        }

        if (map->GetMapPerms().find("r") == std::string::npos) {
            continue;
        }

        std::string name = regs->GetSpecialRegisterName(data);
        if (name.empty()) {
#if defined(__arm__)
#define NAME_PREFIX "r"
#elif defined(__aarch64__)
#define NAME_PREFIX "x"
#else
#define NAME_PREFIX "x"
#endif
            name = NAME_PREFIX + std::to_string(index - 1);
        }

        constexpr size_t SIZE = sizeof(uintptr_t);
        constexpr int COUNT = 32;
        constexpr int FORWARD_SZ = 2;
        auto mapName = map->GetMapPath();
        if (!mapName.empty()) {
            name.append("(" + mapName + ")");
        }

        data = data & ~(SIZE - 1);
        data -= (FORWARD_SZ * SIZE);
        auto block = CreateMemoryBlock(data, 0, COUNT, name);
        registerBlocks_.push_back(block);
    }
}

void FaultStack::PrintRegisterMemory() const
{
    if (registerBlocks_.empty()) {
        return;
    }

    DfxRingBufferWrapper::GetInstance().AppendMsg("Memory near registers:\n");
    for (const auto& block : registerBlocks_) {
        uintptr_t targetAddr = block.startAddr;
        DfxRingBufferWrapper::GetInstance().AppendMsg(block.name + ":\n");
        for (size_t i = 0; i < block.content.size(); i++) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("    " PRINT_FORMAT " " PRINT_FORMAT "\n",
                targetAddr,
                block.content.at(i));
            targetAddr += STEP;
        }
    }
}
} // namespace HiviewDFX
} // namespace OHOS
