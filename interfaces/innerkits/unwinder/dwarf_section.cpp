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

DwarfSection::DwarfSection(std::shared_ptr<DfxMemory> memory) : memory_(memory)
{
    (void)memset_s(&lastErrorData_, sizeof(UnwindErrorData), 0, sizeof(UnwindErrorData));
}

bool DwarfSection::LinearSearchEntry(uintptr_t pc, struct UnwindTableInfo uti, struct UnwindEntryInfo& uei)
{
    uintptr_t fdeCount = uti.tableLen;
    uintptr_t tableData = uti.tableData;
    LOGU("LinearSearchEntry tableData:%p, tableLen: %u", (void*)tableData, (uint32_t)fdeCount);
    uintptr_t i = 0, ptr = tableData;
    FrameDescEntry fdeInfo;
    while (i++ < fdeCount && ptr < uti.endPc) {
        uintptr_t fdeAddr = ptr;
        if (GetCieOrFde(ptr, fdeInfo)) {
            if (pc >= fdeInfo.pcStart && pc < fdeInfo.pcEnd) {
                LOGU("Fde entry addr: %" PRIx64 "", (uint64_t)fdeAddr);
                uei.unwindInfo = (void *)(fdeAddr);
                uei.format = UNW_INFO_FORMAT_REMOTE_TABLE;
                return true;
            }
        } else {
            break;
        }
    }
    return false;
}

bool DwarfSection::SearchEntry(uintptr_t pc, struct UnwindTableInfo uti, struct UnwindEntryInfo& uei)
{
    MAYBE_UNUSED auto segbase = uti.segbase;
    uintptr_t fdeCount = uti.tableLen;
    uintptr_t tableData = uti.tableData;
    LOGU("SearchEntry pc: %p segbase:%p, tableData:%p, tableLen: %u",
        (void*)pc, (void*)segbase, (void*)tableData, (uint32_t)fdeCount);

    // do binary search, encode is stored in symbol file, we have no means to find?
    // hard code for 1b DwarfEncoding
    uintptr_t ptr = 0;
    uintptr_t entry = 0;
    uintptr_t low = 0;
    uintptr_t high = fdeCount;
    DwarfTableEntry dwarfTableEntry;
    while (low < high) {
        uintptr_t cur = (low + high) / 2; // 2 : binary search divided parameter
        ptr = (uintptr_t) tableData + cur * sizeof(DwarfTableEntry);
        if (!memory_->ReadS32(ptr, &dwarfTableEntry.startPc, true)) {
            lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_INVALID_MEMORY);
            return false;
        }
        uintptr_t startPc = static_cast<uintptr_t>(dwarfTableEntry.startPc) + segbase;
        if (startPc == pc) {
            if (!memory_->ReadS32(ptr, &dwarfTableEntry.fdeOffset, true)) {
                lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_INVALID_MEMORY);
                return false;
            }
            entry = static_cast<uintptr_t>(dwarfTableEntry.fdeOffset) + segbase;
            break;
        } else if (pc < startPc) {
            high = cur;
        } else {
            low = cur + 1;
        }
    }

    if (entry == 0) {
        if (high != 0) {
            ptr = (uintptr_t) tableData + (high - 1) * sizeof(DwarfTableEntry);
            ptr += 4; // 4 : four bytes
            if (!memory_->ReadS32(ptr, &dwarfTableEntry.fdeOffset, true)) {
                lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_INVALID_MEMORY);
                return false;
            }
            entry = static_cast<uintptr_t>(dwarfTableEntry.fdeOffset) + segbase;
        } else {
            return false;
        }
    }

    LOGU("Fde entry addr: %" PRIx64 "", (uint64_t)entry);
    uei.unwindInfo = (void *)(entry);
    uei.format = UNW_INFO_FORMAT_REMOTE_TABLE;
    return true;
}

bool DwarfSection::Step(uintptr_t fdeAddr, std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    FrameDescEntry fdeInfo;
    if (!ParseFde(fdeAddr, fdeAddr, fdeInfo)) {
        LOGE("%s", "Failed to parse fde?");
        lastErrorData_.SetAddrAndCode(fdeAddr, UNW_ERROR_DWARF_INVALID_FDE);
        return false;
    }
    uintptr_t pc = regs->GetPc();
    DfxRegs::DoPcAdjust(memory_, pc);
    if (pc < fdeInfo.pcStart || pc >= fdeInfo.pcEnd) {
        return false;
    }
    LOGU("pc: %p, FDE start: %p", (void*)pc, (void*)fdeInfo.pcStart);
    DwarfCfaInstructions dwarfInstructions(memory_);
    if (!dwarfInstructions.Parse(pc, fdeInfo, *(rs.get()))) {
        LOGE("%s", "Failed to parse dwarf instructions?");
        lastErrorData_.SetAddrAndCode(pc, UNW_ERROR_DWARF_INVALID_INSTR);
        return false;
    }
    return true;
}

bool DwarfSection::GetCieOrFde(uintptr_t &addr, FrameDescEntry &fdeInfo)
{
    uintptr_t ptr = addr;
    bool isCieEntry = false;
    ParseCieOrFdeHeader(ptr, fdeInfo, isCieEntry);

    if (isCieEntry) {
        if (!ParseCie(addr, ptr, fdeInfo.cie)) {
            LOGE("%s", "Failed to Parse CIE?");
            return false;
        }
        addr = fdeInfo.cie.instructionsEnd;
    } else {
        if (!ParseFde(addr, ptr, fdeInfo)) {
            LOGE("%s", "Failed to Parse FDE?");
            return false;
        }
        addr = fdeInfo.instructionsEnd;
    }
    return true;
}

void DwarfSection::ParseCieOrFdeHeader(uintptr_t& ptr, FrameDescEntry &fdeInfo, bool& isCieEntry)
{
    uint32_t value32 = 0;
    memory_->ReadU32(ptr, &value32, true);
    uintptr_t ciePtr = 0;
    uintptr_t instructionsEnd = 0;
    if (value32 == static_cast<uint32_t>(-1)) {
        uint64_t value64;
        memory_->ReadU64(ptr, &value64, true);
        instructionsEnd = ptr + value64;

        memory_->ReadU64(ptr, &value64, true);
        ciePtr = static_cast<uintptr_t>(value64);
        if (ciePtr == cie64Value_) {
            isCieEntry = true;
            fdeInfo.cie.instructionsEnd = instructionsEnd;
            fdeInfo.cie.pointerEncoding = DW_EH_PE_sdata8;
        } else {
            fdeInfo.instructionsEnd = instructionsEnd;
            fdeInfo.cieAddr = static_cast<uintptr_t>(ptr - ciePtr);
        }
        ptr += sizeof(uint64_t);
    } else {
        instructionsEnd = ptr + value32;
        memory_->ReadU32(ptr, &value32, false);
        ciePtr = static_cast<uintptr_t>(value32);
        if (ciePtr == cie32Value_) {
            isCieEntry = true;
            fdeInfo.cie.instructionsEnd = instructionsEnd;
            fdeInfo.cie.pointerEncoding = DW_EH_PE_sdata4;
        } else {
            fdeInfo.instructionsEnd = instructionsEnd;
            fdeInfo.cieAddr = static_cast<uintptr_t>(ptr - ciePtr);
        }
        ptr += sizeof(uint32_t);
    }
}

bool DwarfSection::ParseFde(uintptr_t fdeAddr, uintptr_t fdePtr, FrameDescEntry &fdeInfo)
{
    LOGU("fdeAddr: %" PRIx64 "", (uint64_t)fdeAddr);
    if (!fdeEntries_.empty()) {
        auto iter = fdeEntries_.find(fdeAddr);
        if (iter != fdeEntries_.end()) {
            fdeInfo = iter->second;
            return true;
        }
    }

    if (fdeAddr == fdePtr) {
        bool isCieEntry = false;
        ParseCieOrFdeHeader(fdePtr, fdeInfo, isCieEntry);
        if (isCieEntry) {
            LOGE("%s", "ParseFde error, is Cie Entry?");
            return false;
        }
    }
    if (!FillInFde(fdePtr, fdeInfo)) {
        LOGE("%s", "ParseFde error, failed to fill FDE?");
        fdeEntries_.erase(fdeAddr);
        return false;
    }
    fdeEntries_[fdeAddr] = fdeInfo;
    return true;
}

bool DwarfSection::FillInFde(uintptr_t ptr, FrameDescEntry &fdeInfo)
{
    if (!ParseCie(fdeInfo.cieAddr, fdeInfo.cieAddr, fdeInfo.cie)) {
        LOGE("%s", "Failed to parse CIE?");
        return false;
    }

    if (fdeInfo.cie.segmentSize != 0) {
        // Skip over the segment selector for now.
        ptr += fdeInfo.cie.segmentSize;
    }
    // Parse pc begin and range.
    LOGU("pointerEncoding: %02x", fdeInfo.cie.pointerEncoding);
    uintptr_t pcStart = memory_->ReadEncodedValue(ptr, fdeInfo.cie.pointerEncoding);
    uintptr_t pcRange = memory_->ReadEncodedValue(ptr, (fdeInfo.cie.pointerEncoding & 0x0F));

    fdeInfo.lsda = 0;
    // Check for augmentation length.
    if (fdeInfo.cie.hasAugmentationData) {
        uintptr_t augLen = memory_->ReadUleb128(ptr);
        uintptr_t instructionsPtr = ptr + augLen;
        if (fdeInfo.cie.lsdaEncoding != DW_EH_PE_omit) {
            uintptr_t lsdaPtr = ptr;
            if (memory_->ReadEncodedValue(ptr, (fdeInfo.cie.lsdaEncoding & 0x0F)) != 0) {
                fdeInfo.lsda = memory_->ReadEncodedValue(lsdaPtr, fdeInfo.cie.lsdaEncoding);
            }
        }
        ptr = instructionsPtr;
    }

    fdeInfo.instructionsOff = ptr;
    fdeInfo.pcStart = pcStart;
    fdeInfo.pcEnd = pcStart + pcRange;
    LOGU("FDE pcStart: %p, pcEnd: %p", (void*)(fdeInfo.pcStart), (void*)(fdeInfo.pcEnd));
    return true;
}

bool DwarfSection::ParseCie(uintptr_t cieAddr, uintptr_t ciePtr, CommonInfoEntry &cieInfo)
{
    LOGU("cieAddr: %" PRIx64 "", (uint64_t)cieAddr);
    if (!cieEntries_.empty()) {
        auto iter = cieEntries_.find(cieAddr);
        if (iter != cieEntries_.end()) {
            cieInfo = iter->second;
            return true;
        }
    }
    if (cieAddr == ciePtr) {
        cieInfo.lsdaEncoding = DW_EH_PE_omit;
        bool isCieEntry = false;
        FrameDescEntry fdeInfo;
        ParseCieOrFdeHeader(ciePtr, fdeInfo, isCieEntry);
        if (!isCieEntry) {
            LOGE("%s", "ParseCie error, is not Cie Entry?");
            return false;
        }
        cieInfo = fdeInfo.cie;
    }
    if (!FillInCie(ciePtr, cieInfo)) {
        LOGE("%s", "ParseCie error, failed to fill Cie?");
        cieEntries_.erase(cieAddr);
        return false;
    }
    cieEntries_[cieAddr] = cieInfo;
    return true;
}

bool DwarfSection::FillInCie(uintptr_t ptr, CommonInfoEntry &cieInfo)
{
    uint8_t version;
    memory_->ReadU8(ptr, &version, true);
    LOGU("Cie version: %d", version);
    if (version != DW_EH_VERSION && version != 3 && version != 4 && version != 5) { // 3 4 5 : cie version
        LOGE("Invalid cie version: %d", version);
        lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_UNSUPPORTED_VERSION);
        return false;
    }

    // save augmentation string
    uint8_t ch;
    std::vector<char> augStr;
    augStr.clear();
    while (true) {
        memory_->ReadU8(ptr, &ch, true);
        if (ch == '\0') {
            break;
        }
        augStr.push_back(ch);
    }

    // Segment Size
    if (version == 4 || version == 5) { // 4 5 : cie version
        // Skip the Address Size field since we only use it for validation.
        ptr += 1;
        memory_->ReadU8(ptr, &cieInfo.segmentSize, true);
    } else {
        cieInfo.segmentSize = 0;
    }

    // parse code alignment factor
    cieInfo.codeAlignFactor = (uint32_t)memory_->ReadUleb128(ptr);
    LOGU("codeAlignFactor: %d", cieInfo.codeAlignFactor);

    // parse data alignment factor
    cieInfo.dataAlignFactor = (int32_t)memory_->ReadSleb128(ptr);
    LOGU("dataAlignFactor: %d", cieInfo.dataAlignFactor);

    // parse return address register
    if (version == DW_EH_VERSION) {
        uint8_t val;
        memory_->ReadU8(ptr, &val, true);
        cieInfo.returnAddressRegister = static_cast<uintptr_t>(val);
    } else {
        cieInfo.returnAddressRegister = (uintptr_t)memory_->ReadUleb128(ptr);
    }
    LOGU("returnAddressRegister: %d", (int)cieInfo.returnAddressRegister);

    // parse augmentation data based on augmentation string
    if (augStr.empty() || augStr[0] != 'z') {
        cieInfo.instructionsOff = ptr;
        return true;
    }
    cieInfo.hasAugmentationData = true;
    // parse augmentation data length
    MAYBE_UNUSED uintptr_t augSize = memory_->ReadUleb128(ptr);
    LOGU("augSize: %" PRIxPTR "", augSize);
    cieInfo.instructionsOff = ptr + augSize;

    for (size_t i = 1; i < augStr.size(); ++i) {
        switch (augStr[i]) {
            case 'P':
                uint8_t personalityEncoding;
                memory_->ReadU8(ptr, &personalityEncoding, true);
                cieInfo.personality = memory_->ReadEncodedValue(ptr, personalityEncoding);
                break;
            case 'L':
                memory_->ReadU8(ptr, &cieInfo.lsdaEncoding, true);
                LOGU("cieInfo.lsdaEncoding: %x", cieInfo.lsdaEncoding);
                break;
            case 'R':
                memory_->ReadU8(ptr, &cieInfo.pointerEncoding, true);
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