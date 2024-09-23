/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#include "dfx_fault_stack.h"

#include <algorithm>
#include <csignal>
#include <sys/ptrace.h>
#include <sys/stat.h>

#include "dfx_config.h"
#include "dfx_elf.h"
#include "dfx_logger.h"
#include "dfx_ring_buffer_wrapper.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
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
            if (errno != prevErrno_) {
                LOGERROR("read target mem by ptrace failed, errno(%{public}s).", strerror(errno));
                prevErrno_ = errno;
            }
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

    if (size == 0 || index == 0) {
        return prevEndAddr;
    }

    std::string name = "sp" + std::to_string(index - 1);
    auto block = CreateMemoryBlock(startAddr, prevSp, size, name);
    blocks_.push_back(block);
    return startAddr + size * STEP;
}

bool FaultStack::CollectStackInfo(const std::vector<DfxFrame>& frames, bool needParseStack)
{
    if (frames.empty()) {
        LOGWARN("null frames.");
        return false;
    }

    size_t index = 1;
    uintptr_t minAddr = 4096;
    uintptr_t size = 0;
    uintptr_t prevSp = 0;
    uintptr_t prevEndAddr = 0;
    uintptr_t highAddrLength = DfxConfig::GetConfig().highAddressStep;
    if (needParseStack) {
        highAddrLength = 8192; // 8192 : 32k / STEP
    }

    auto firstFrame = frames.at(0);
    prevSp = static_cast<uintptr_t>(firstFrame.sp);
    constexpr size_t MAX_FAULT_STACK_SZ = 4;
    for (index = 1; index < frames.size(); index++) {
        if (index > MAX_FAULT_STACK_SZ) {
            break;
        }

        auto frame = frames.at(index);
        uintptr_t curSp = static_cast<uintptr_t>(frame.sp);

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
        prevEndAddr = AdjustAndCreateMemoryBlock(index, prevSp, prevEndAddr, size);
    }

    const uintptr_t minCorruptedStackSz = 1024;
    CreateBlockForCorruptedStack(frames, prevEndAddr, minCorruptedStackSz);
    return true;
}

bool FaultStack::CreateBlockForCorruptedStack(const std::vector<DfxFrame>& frames, uintptr_t prevEndAddr,
                                              uintptr_t size)
{
    const auto& frame = frames.back();
    // stack trace should end with libc or ffrt or */bin/*
    if (frame.mapName.find("ld-musl") != std::string::npos ||
        frame.mapName.find("ffrt") != std::string::npos ||
        frame.mapName.find("bin") != std::string::npos) {
        return false;
    }

    AdjustAndCreateMemoryBlock(frame.index, frame.sp, prevEndAddr, size);
    return true;
}

uintptr_t FaultStack::PrintMemoryBlock(const MemoryBlockInfo& info, uintptr_t stackStartAddr) const
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
        if (targetAddr - stackStartAddr > STEP * DfxConfig::GetConfig().highAddressStep) {
            break;
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
    uintptr_t stackStartAddr = blocks_.at(0).startAddr;
    for (const auto& block : blocks_) {
        if (end != 0 && end < block.startAddr) {
            DfxRingBufferWrapper::GetInstance().AppendMsg("    ...\n");
        }
        end = PrintMemoryBlock(block, stackStartAddr);
    }
}

MemoryBlockInfo FaultStack::CreateMemoryBlock(
    uintptr_t addr,
    uintptr_t offset,
    uintptr_t size,
    const std::string& name) const
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

void FaultStack::CollectRegistersBlock(std::shared_ptr<DfxRegs> regs, std::shared_ptr<DfxMaps> maps)
{
    if (regs == nullptr || maps == nullptr) {
        LOGERROR("%{public}s : regs or maps is null.", __func__);
        return;
    }

    auto regsData = regs->GetRegsData();
    int index = 0;
    for (auto data : regsData) {
        index++;
        std::shared_ptr<DfxMap> map;
        if (!maps->FindMapByAddr(data, map)) {
            continue;
        }

        if ((map->prots & PROT_READ) == 0) {
            continue;
        }

        std::string name = regs->GetSpecialRegsNameByIndex(index - 1);
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
        auto mapName = map->name;
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

bool FaultStack::ParseUnwindStack(std::shared_ptr<DfxMaps> maps, std::vector<DfxFrame>& frames)
{
    if (maps == nullptr) {
        LOGERROR("%{public}s : maps is null.", __func__);
        return false;
    }
    size_t index = frames.size();
    for (const auto& block : blocks_) {
        std::shared_ptr<DfxMap> map;
        for (size_t i = 0; i < block.content.size(); i++) {
            if (!maps->FindMapByAddr(block.content[i], map) ||
                (map->prots & PROT_EXEC) == 0) {
                continue;
            }
            DfxFrame frame;
            frame.index = index;
            frame.pc = block.content[i];
            frame.map = map;
            frame.mapName = map->name;
            int64_t loadBias = 0;
            struct stat st;
            if (stat(map->name.c_str(), &st) == 0 && (st.st_mode & S_IFREG)) {
                auto elf = DfxElf::Create(frame.mapName);
                if (elf == nullptr || !elf->IsValid()) {
                    LOGERROR("%{public}s : Failed to create DfxElf, elf path(%{public}s).", __func__,
                        frame.mapName.c_str());
                    return false;
                }
                loadBias = elf->GetLoadBias();
                frame.buildId = elf->GetBuildId();
            } else {
                LOGWARN("%{public}s : mapName(%{public}s) is not file.", __func__, frame.mapName.c_str());
            }

            frame.relPc = frame.pc - map->begin + map->offset + static_cast<uint64_t>(loadBias);
            frames.emplace_back(frame);
            constexpr int MAX_VALID_ADDRESS_NUM = 32;
            if (++index >= MAX_VALID_ADDRESS_NUM) {
                return true;
            }
        }
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
