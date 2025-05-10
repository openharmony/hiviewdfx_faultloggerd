/*
 * Copyright (c) 2022-2025 Huawei Device Co., Ltd.
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

#include "decorative_dump_info.h"

#include <algorithm>
#include <csignal>
#include <sys/ptrace.h>
#include <sys/stat.h>

#include "dfx_log.h"
#include "dump_utils.h"
#include "process_dump_config.h"
#include "dfx_elf.h"
#include "dfx_buffer_writer.h"

namespace OHOS {
namespace HiviewDFX {
REGISTER_DUMP_INFO_CLASS(FaultStack);

void FaultStack::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    DfxBufferWriter::GetInstance().WriteMsg("FaultStack:\n");
    CollectStackInfo(process.GetKeyThread()->GetThreadInfo().nsTid, process.GetKeyThread()->GetFrames());
    if (blocks_.empty()) {
        return;
    }
    uintptr_t end = 0;
    uintptr_t stackStartAddr = blocks_.at(0).startAddr;
    for (const auto& block : blocks_) {
        if (end != 0 && end < block.startAddr) {
            DfxBufferWriter::GetInstance().WriteMsg("    ...\n");
        }
        end = PrintMemoryBlock(block, stackStartAddr);
    }
    if (ProcessDumpConfig::GetInstance().GetConfig().simplifyVmaPrinting ||
        process.GetCrashLogConfig().simplifyVmaPrinting) {
        process.SetStackValues(GetStackValues());
    }
}

const std::vector<uintptr_t>& FaultStack::GetStackValues()
{
    for (const auto& block : blocks_) {
        for (const auto& value : block.content) {
            stackValues_.emplace_back(value);
        }
    }
    return stackValues_;
}

uintptr_t FaultStack::AdjustAndCreateMemoryBlock(pid_t tid, size_t index, uintptr_t prevSp,
                                                 uintptr_t prevEndAddr, uintptr_t size)
{
    uintptr_t lowAddrLength = ProcessDumpConfig::GetInstance().GetConfig().faultStackLowAddrStep;
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
    MemoryBlockInfo blockInfo = {
        .startAddr = startAddr,
        .nameAddr = prevSp,
        .size = size,
        .name = name,
    };
    CreateMemoryBlock(tid, blockInfo);
    blocks_.push_back(blockInfo);
    return startAddr + size * STEP;
}

void FaultStack::CollectStackInfo(pid_t tid, const std::vector<DfxFrame>& frames, bool needParseStack)
{
    if (frames.empty()) {
        DFXLOGW("null frames.");
        return;
    }
    size_t index = 1;
    uintptr_t minAddr = 4096;
    uintptr_t size = 0;
    uintptr_t prevSp = 0;
    uintptr_t prevEndAddr = 0;
    uintptr_t highAddrLength = ProcessDumpConfig::GetInstance().GetConfig().faultStackHighAddrStep;
    if (needParseStack) {
        highAddrLength = 8192; // 8192 : 32k / STEP
    }

    auto firstFrame = frames.at(0);
    prevSp = static_cast<uintptr_t>(firstFrame.sp);
    constexpr size_t maxFaultStackSz = 4;
    for (index = 1; index < frames.size(); index++) {
        if (index > maxFaultStackSz) {
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

        prevEndAddr = AdjustAndCreateMemoryBlock(tid, index, prevSp, prevEndAddr, size);
        prevSp = curSp;
    }

    if ((blocks_.size() < maxFaultStackSz) && (prevSp > minAddr)) {
        size = highAddrLength;
        prevEndAddr = AdjustAndCreateMemoryBlock(tid, index, prevSp, prevEndAddr, size);
    }

    const uintptr_t minCorruptedStackSz = 1024;
    CreateBlockForCorruptedStack(tid, frames, prevEndAddr, minCorruptedStackSz);
}

bool FaultStack::CreateBlockForCorruptedStack(pid_t tid, const std::vector<DfxFrame>& frames, uintptr_t prevEndAddr,
                                              uintptr_t size)
{
    const auto& frame = frames.back();
    // stack trace should end with libc or ffrt or */bin/*
    if (frame.mapName.find("ld-musl") != std::string::npos ||
        frame.mapName.find("ffrt") != std::string::npos ||
        frame.mapName.find("bin") != std::string::npos) {
        return false;
    }

    AdjustAndCreateMemoryBlock(tid, frame.index, frame.sp, prevEndAddr, size);
    return true;
}

uintptr_t FaultStack::PrintMemoryBlock(const MemoryBlockInfo& info, uintptr_t stackStartAddr) const
{
    uintptr_t targetAddr = info.startAddr;
    for (uint64_t i = 0; i < static_cast<uint64_t>(info.content.size()); i++) {
        if (targetAddr == info.nameAddr) {
            DfxBufferWriter::GetInstance().WriteFormatMsg("%s:" PRINT_FORMAT " " PRINT_FORMAT "\n",
                info.name.c_str(),
                targetAddr,
                info.content.at(i));
        } else {
            DfxBufferWriter::GetInstance().WriteFormatMsg("    " PRINT_FORMAT " " PRINT_FORMAT "\n",
                targetAddr,
                info.content.at(i));
        }
        if (targetAddr - stackStartAddr > STEP * ProcessDumpConfig::GetInstance().GetConfig().faultStackHighAddrStep) {
            break;
        }
        targetAddr += STEP;
    }
    return targetAddr;
}

void FaultStack::CreateMemoryBlock(pid_t tid, MemoryBlockInfo& blockInfo) const
{
    uintptr_t targetAddr = blockInfo.startAddr;
    for (uint64_t i = 0; i < static_cast<uint64_t>(blockInfo.size); i++) {
        uintptr_t value = 0;
        if (DumpUtils::ReadTargetMemory(tid, targetAddr, value)) {
            blockInfo.content.push_back(value);
        } else {
            blockInfo.content.push_back(-1);
        }
        targetAddr += STEP;
    }
}
} // namespace HiviewDFX
} // namespace OHOS
