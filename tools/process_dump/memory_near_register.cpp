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
#include "decorative_dump_info.h"
#include "dfx_buffer_writer.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "process_dump_config.h"
#include "string_printf.h"
#include "dump_utils.h"
namespace OHOS {
namespace HiviewDFX {
namespace {
#if defined(__arm__)
const char* const NAME_PREFIX = "r";
#else
const char* const NAME_PREFIX = "x";
#endif
}
REGISTER_DUMP_INFO_CLASS(MemoryNearRegister);

void MemoryNearRegister::Print(DfxProcess& process, const ProcessDumpRequest& request, Unwinder& unwinder)
{
    DecorativeDumpInfo::Print(process, request, unwinder);
    pid_t tid = process.GetKeyThread()->GetThreadInfo().nsTid;
    bool extendPcLrPrinting = process.GetCrashLogConfig().extendPcLrPrinting ||
        ProcessDumpConfig::GetInstance().GetConfig().extendPcLrPrinting;
    CollectRegistersBlock(tid, process.GetFaultThreadRegisters(), unwinder.GetMaps(), extendPcLrPrinting);
    DfxBufferWriter::GetInstance().WriteMsg("Memory near registers:\n");
    for (const auto& block : registerBlocks_) {
        uintptr_t targetAddr = block.startAddr;
        DfxBufferWriter::GetInstance().WriteMsg(block.name + ":\n");
        for (size_t i = 0; i < block.content.size(); i++) {
            DfxBufferWriter::GetInstance().WriteFormatMsg("    " PRINT_FORMAT " " PRINT_FORMAT "\n",
                targetAddr,
                block.content.at(i));
            targetAddr += STEP;
        }
    }
}

void MemoryNearRegister::GetMemoryValues(std::set<uintptr_t>& memoryValues)
{
    for (const auto& block : registerBlocks_) {
        for (const auto& value : block.content) {
            memoryValues.emplace(StripPac(value, 0));
        }
    }
}

void MemoryNearRegister::CollectRegistersBlock(pid_t tid, std::shared_ptr<DfxRegs> regs,
    std::shared_ptr<DfxMaps> maps, bool extendPcLrPrinting)
{
    if (regs == nullptr || maps == nullptr) {
        DFXLOGE("%{public}s : regs or maps is null.", __func__);
        return;
    }
    auto regsData = regs->GetRegsData();
    int index = 0;
    for (auto data : regsData) {
#if defined(ENABLE_HWASAN)
        constexpr uintptr_t tbiMask = 0xff00000000000000; // the upper eight bits are TBI
        data &= ~tbiMask;
#endif
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
            name = NAME_PREFIX + std::to_string(index - 1);
        }
        constexpr size_t size = sizeof(uintptr_t);
        uintptr_t count = 32;
        uintptr_t forwardSize = 2;
        if (extendPcLrPrinting && (name == "lr" || name == "pc")) {
            forwardSize = 31; // 31 : forward count of print value
            count = 64; // // 64 : total count of print value
        }
        auto mapName = map->name;
        if (!mapName.empty()) {
            name.append("(" + mapName + ")");
        }
        data = data & ~(size - 1);
        data -= (forwardSize * size);
        MemoryBlockInfo blockInfo = {
            .startAddr = data,
            .nameAddr = 0,
            .size = count,
            .name = name,
        };
        CreateMemoryBlock(tid, blockInfo);
        registerBlocks_.push_back(blockInfo);
    }
}

void MemoryNearRegister::CreateMemoryBlock(pid_t tid, MemoryBlockInfo& blockInfo) const
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
}
}