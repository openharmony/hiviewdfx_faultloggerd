/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "dwarf_section.h"
#include <securec.h>
#include "dfx_log.h"
#include "dwarf_cfa_instructions.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxDwarfSection"
}

bool DwarfSection::SearchEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti, uintptr_t pc)
{
    MAYBE_UNUSED auto segbase = uti.segbase;
    auto fdeCount = uti.tableLen;
    uintptr_t tableData = uti.tableData;
    LOGU("DwarfSearchUnwindTable segbase:%p, tableData:%p, tableLen: %d",
        (void*)segbase, (void*)tableData, fdeCount);

    // do binary search, encode is stored in symbol file, we have no means to find?
    // hard code for 1b DwarfEncoding
    uintptr_t entry;
    uintptr_t low = 0;
    DwarfTableEntry dwarfTableEntry;
    for (uintptr_t len = fdeCount; len > 1;) {
        uintptr_t cur = low + (len / 2);
        entry = (uintptr_t) tableData + cur * sizeof(DwarfTableEntry);
        LOGU("cur:%d, entry:%llx", (int)cur, (uint64_t)entry);
        DfxMemoryCpy::GetInstance().ReadS32(entry, &dwarfTableEntry.startPcOffset, true);
        uintptr_t startPc = static_cast<uintptr_t>(dwarfTableEntry.startPcOffset + segbase);
        LOGU("target Pc:%p, startPc:%p", (void *)pc, (void *)startPc);

        if (startPc == pc) {
            low = cur;
            break;
        } else if (startPc < pc) {
            low = cur;
            len -= (len / 2);
        } else {
            len /= 2;
        }
    }

    entry = (uintptr_t) tableData + low * sizeof(DwarfTableEntry);
    entry += 4;

    DfxMemoryCpy::GetInstance().ReadS32(entry, &dwarfTableEntry.fdeOffset, true);
    uintptr_t fdeAddr = static_cast<uintptr_t>(dwarfTableEntry.fdeOffset + segbase);
    pi.unwindInfo = (void *)(fdeAddr);
    LOGU("fde offset entry: %llx", (uint64_t)pi.unwindInfo);
    pi.format = UNW_INFO_FORMAT_REMOTE_TABLE;
    pi.gp = uti.gp;
    return true;
}

bool DwarfSection::Step(uintptr_t fdeAddr, std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    lastErrorData_.addr = static_cast<uint64_t>(fdeAddr);
    FrameDescEntry fdeInfo;
    if (!ParseFde(fdeAddr, fdeInfo)) {
        LOGE("Failed to parse fde?");
        lastErrorData_.code = UNW_ERROR_DWARF_INVALID_FDE;
        return false;
    }

    LOGU("pc: %p, FDE start: %p", (void*)regs->GetPc(), (void*)fdeInfo.pcStart);
    DwarfCfaInstructions dwarfInstructions(memory_);
    if (!dwarfInstructions.Parse(regs->GetPc(), fdeInfo, *(rs.get()))) {
        LOGE("Failed to parse dwarf instructions?");
        lastErrorData_.code = UNW_ERROR_DWARF_INVALID_INSTR;
        return false;
    }
    return true;
}

bool DwarfSection::ParseFde(uintptr_t fdeAddr, FrameDescEntry &fdeInfo)
{
    LOGU("fdeAddr: %llx", (uint64_t)fdeAddr);
    if (!fdeEntries_.empty()) {
        auto iter = fdeEntries_.find(fdeAddr);
        if (iter != fdeEntries_.end()) {
            fdeInfo = iter->second;
            return true;
        }
    }
    fdeInfo.fdeStart = fdeAddr;
    uintptr_t ptr = fdeAddr;
    if (!FillInFdeHeader(ptr, fdeInfo) || !FillInFde(ptr, fdeInfo)) {
        LOGE("Failed to fill FDE?");
        fdeEntries_.erase(fdeAddr);
        return false;
    }
    fdeEntries_[fdeAddr] = fdeInfo;
    return true;
}

bool DwarfSection::FillInFdeHeader(uintptr_t& ptr, FrameDescEntry &fdeInfo)
{
    uint32_t value32 = 0;
    DfxMemoryCpy::GetInstance().ReadU32(ptr, &value32, true);
    uintptr_t ciePtr = 0;
    if (value32 == static_cast<uint32_t>(-1)) {
        uint64_t value64;
        DfxMemoryCpy::GetInstance().ReadU64(ptr, &value64, true);
        fdeInfo.fdeEnd = ptr + value64;

        DfxMemoryCpy::GetInstance().ReadU64(ptr, &value64, true);
        ciePtr = static_cast<uintptr_t>(value64);
        if (ciePtr == cie64Value_) {
            LOGE("Failed to parse FDE, ciePtr?");
            return false;
        }
        fdeInfo.cieAddr = static_cast<uintptr_t>(ptr - ciePtr);
        ptr += sizeof(uint64_t);
    } else {
        fdeInfo.fdeEnd = ptr + value32;
        DfxMemoryCpy::GetInstance().ReadU32(ptr, &value32, false);
        ciePtr = static_cast<uintptr_t>(value32);
        if (ciePtr == cie32Value_) {
            LOGE("Failed to parse FDE, ciePtr?");
            return false;
        }
        fdeInfo.cieAddr = static_cast<uintptr_t>(ptr - ciePtr);
        ptr += sizeof(uint32_t);
    }
    return true;
}

bool DwarfSection::FillInFde(uintptr_t& ptr, FrameDescEntry &fdeInfo)
{
    if (!ParseCie(fdeInfo.cieAddr, fdeInfo.cie)) {
        LOGE("Failed to fill CIE?");
        return false;
    }

    if (fdeInfo.cie.segmentSize != 0) {
        // Skip over the segment selector for now.
        ptr += fdeInfo.cie.segmentSize;
    }
    // Parse pc begin and range.
    LOGU("pointerEncoding: %02x", fdeInfo.cie.pointerEncoding);
    uintptr_t pcStart = DfxMemoryCpy::GetInstance().ReadEncodedValue(ptr, fdeInfo.cie.pointerEncoding);
    uintptr_t pcRange = DfxMemoryCpy::GetInstance().ReadEncodedValue(ptr, (fdeInfo.cie.pointerEncoding & 0x0F));

    fdeInfo.lsda = 0;
    // Check for augmentation length.
    if (fdeInfo.cie.hasAugmentationData) {
        uintptr_t augLen = DfxMemoryCpy::GetInstance().ReadUleb128(ptr);
        uintptr_t instructionsPtr = ptr + augLen;
        if (fdeInfo.cie.lsdaEncoding != DW_EH_PE_omit) {
            uintptr_t lsdaPtr = ptr;
            if (DfxMemoryCpy::GetInstance().ReadEncodedValue(ptr, (fdeInfo.cie.lsdaEncoding & 0x0F)) != 0) {
                fdeInfo.lsda = DfxMemoryCpy::GetInstance().ReadEncodedValue(lsdaPtr, fdeInfo.cie.lsdaEncoding);
            }
        }
        ptr = instructionsPtr;
    }

    fdeInfo.instructions = ptr;
    fdeInfo.pcStart = pcStart;
    fdeInfo.pcEnd = pcStart + pcRange;
    LOGU("FDE pcStart: %p, pcEnd: %p", (void*)(fdeInfo.pcStart), (void*)(fdeInfo.pcEnd));
    return true;
}

bool DwarfSection::ParseCie(uintptr_t cieAddr, CommonInfoEntry &cieInfo)
{
    LOGU("cieAddr: %llx", (uint64_t)cieAddr);
    if (!cieEntries_.empty()) {
        auto iter = cieEntries_.find(cieAddr);
        if (iter != cieEntries_.end()) {
            cieInfo = iter->second;
            return true;
        }
    }
    cieInfo.cieStart = cieAddr;
    uintptr_t ptr = cieAddr;
    if (!FillInCieHeader(ptr, cieInfo) || !FillInCie(ptr, cieInfo)) {
        cieEntries_.erase(cieAddr);
        return false;
    }
    cieEntries_[cieAddr] = cieInfo;
    return true;
}

bool DwarfSection::FillInCieHeader(uintptr_t& ptr, CommonInfoEntry &cieInfo)
{
    cieInfo.lsdaEncoding = DW_EH_PE_omit;
    uint32_t value32 = 0;
    DfxMemoryCpy::GetInstance().ReadU32(ptr, &value32, true); //length
    if (value32 == static_cast<uint32_t>(-1)) {
        uint64_t value64 = 0;
        DfxMemoryCpy::GetInstance().ReadU64(ptr, &value64, true); //length64
        cieInfo.cieEnd = ptr + value64;
        cieInfo.pointerEncoding = DW_EH_PE_sdata8;

        DfxMemoryCpy::GetInstance().ReadU64(ptr, &value64, true); //cieid
        if (value64 != cie64Value_) {
            LOGE("Failed to FillInCieHeader, cieId?");
            lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
            return false;
        }
    } else {
        cieInfo.cieEnd = ptr + value32;
        cieInfo.pointerEncoding = DW_EH_PE_sdata4;
        DfxMemoryCpy::GetInstance().ReadU32(ptr, &value32, true);
        if (value32 != cie32Value_) {
            LOGE("Failed to FillInCieHeader, cieId?");
            lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
            return false;
        }
    }
    return true;
}

bool DwarfSection::FillInCie(uintptr_t& ptr, CommonInfoEntry &cieInfo)
{
    uint8_t version;
    DfxMemoryCpy::GetInstance().ReadU8(ptr, &version, true);
    LOGU("Cie version: %d", version);
    if (version != DW_EH_VERSION && version != 3 && version != 4 && version != 5) {
        LOGE("Invalid cie version: %d", version);
        lastErrorData_.code = UNW_ERROR_UNSUPPORTED_VERSION;
        return false;
    }

    // save augmentation string
    uint8_t ch;
    std::vector<char> augStr;
    augStr.clear();
    while (true) {
        DfxMemoryCpy::GetInstance().ReadU8(ptr, &ch, true);
        if (ch == '\0') {
            break;
        }
        augStr.push_back(ch);
    }

    // Segment Size
    if (version == 4 || version == 5) {
        // Skip the Address Size field since we only use it for validation.
        ptr += 1;
        DfxMemoryCpy::GetInstance().ReadU8(ptr, &cieInfo.segmentSize, true);
    } else {
        cieInfo.segmentSize = 0;
    }

    // parse code aligment factor
    cieInfo.codeAlignFactor = (uint32_t)DfxMemoryCpy::GetInstance().ReadUleb128(ptr);
    LOGU("codeAlignFactor: %d", cieInfo.codeAlignFactor);

    // parse data alignment factor
    cieInfo.dataAlignFactor = (int32_t)DfxMemoryCpy::GetInstance().ReadSleb128(ptr);
    LOGU("dataAlignFactor: %d", cieInfo.dataAlignFactor);

    // parse return address register
    if (version == DW_EH_VERSION) {
        uint8_t val;
        DfxMemoryCpy::GetInstance().ReadU8(ptr, &val, true);
        cieInfo.returnAddressRegister = static_cast<uintptr_t>(val);
    } else {
        cieInfo.returnAddressRegister = (uintptr_t)DfxMemoryCpy::GetInstance().ReadUleb128(ptr);
    }
    LOGU("returnAddressRegister: %d", (int)cieInfo.returnAddressRegister);

    // parse augmentation data based on augmentation string
    if (augStr.empty() || augStr[0] != 'z') {
        cieInfo.instructions = ptr;
        return true;
    }
    cieInfo.hasAugmentationData = true;
    // parse augmentation data length
    MAYBE_UNUSED uintptr_t augSize = DfxMemoryCpy::GetInstance().ReadUleb128(ptr);
    LOGU("augSize: %x", augSize);
    cieInfo.instructions = ptr + augSize;

    for (size_t i = 1; i < augStr.size(); ++i) {
        switch (augStr[i]) {
            case 'P': {
                uint8_t personalityEncoding;
                DfxMemoryCpy::GetInstance().ReadU8(ptr, &personalityEncoding, true);
                cieInfo.personality = DfxMemoryCpy::GetInstance().ReadEncodedValue(ptr, personalityEncoding);
            }
                break;
            case 'L':
                DfxMemoryCpy::GetInstance().ReadU8(ptr, &cieInfo.lsdaEncoding, true);
                LOGU("cieInfo.lsdaEncoding: %x", cieInfo.lsdaEncoding);
                break;
            case 'R':
                DfxMemoryCpy::GetInstance().ReadU8(ptr, &cieInfo.pointerEncoding, true);
                LOGU("cieInfo.pointerEncoding: %x", cieInfo.pointerEncoding);
                break;
            case 'S':
                cieInfo.isSignalFrame = true;
                break;
            default:
                break;
        }
    }
    return true;
}
}   // namespace HiviewDFX
}   // namespace OHOS