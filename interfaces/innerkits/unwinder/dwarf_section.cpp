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
#include "dfx_trace_dlsym.h"
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
    DFX_TRACE_SCOPED_DLSYM("DwarfSectionLinearSearchEntry");
    uintptr_t fdeCount = uti.tableLen;
    uintptr_t tableData = uti.tableData;
    DFXLOGU("LinearSearchEntry tableData:%{public}p, tableLen: %{public}u", (void*)tableData, (uint32_t)fdeCount);
    uintptr_t i = 0, ptr = tableData;
    FrameDescEntry fdeInfo;
    while (i++ < fdeCount && ptr < uti.endPc) {
        uintptr_t fdeAddr = ptr;
        if (GetCieOrFde(ptr, fdeInfo)) {
            if (pc >= fdeInfo.pcStart && pc < fdeInfo.pcEnd) {
                DFXLOGU("Fde entry addr: %{public}" PRIx64 "", (uint64_t)fdeAddr);
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
    DFX_TRACE_SCOPED_DLSYM("DwarfSectionSearchEntry");
    MAYBE_UNUSED auto segbase = uti.segbase;
    uintptr_t fdeCount = uti.tableLen;
    uintptr_t tableData = uti.tableData;
    DFXLOGU("SearchEntry pc: %{public}p segbase:%{public}p, tableData:%{public}p, tableLen: %{public}u",
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
        if (!memory_->Read<int32_t>(ptr, &dwarfTableEntry.startPc, true)) {
            lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_INVALID_MEMORY);
            return false;
        }
        uintptr_t startPc = static_cast<uintptr_t>(dwarfTableEntry.startPc) + segbase;
        if (startPc == pc) {
            if (!memory_->Read<int32_t>(ptr, &dwarfTableEntry.fdeOffset, true)) {
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
            ptr = static_cast<uintptr_t>(tableData) + (high - 1) * sizeof(DwarfTableEntry) + 4; // 4 : four bytes
            if (!memory_->Read<int32_t>(ptr, &dwarfTableEntry.fdeOffset, true)) {
                lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_INVALID_MEMORY);
                return false;
            }
            entry = static_cast<uintptr_t>(dwarfTableEntry.fdeOffset) + segbase;
        } else {
            return false;
        }
    }

    DFXLOGU("Fde entry addr: %{public}" PRIx64 "", (uint64_t)entry);
    uei.unwindInfo = (void *)(entry);
    uei.format = UNW_INFO_FORMAT_REMOTE_TABLE;
    return true;
}

bool DwarfSection::Step(uintptr_t pc, uintptr_t fdeAddr, std::shared_ptr<RegLocState> rs)
{
    DFX_TRACE_SCOPED_DLSYM("DwarfSectionStep");
    FrameDescEntry fdeInfo;
    if (!ParseFde(fdeAddr, fdeAddr, fdeInfo)) {
        DFXLOGE("Failed to parse fde?");
        lastErrorData_.SetAddrAndCode(fdeAddr, UNW_ERROR_DWARF_INVALID_FDE);
        return false;
    }

    if (pc < fdeInfo.pcStart || pc >= fdeInfo.pcEnd) {
        DFXLOGU("pc: %{public}p, FDE start: %{public}p, end: %{public}p",
            (void*)pc, (void*)fdeInfo.pcStart, (void*)fdeInfo.pcEnd);
        return false;
    }
    DwarfCfaInstructions dwarfInstructions(memory_);
    if (!dwarfInstructions.Parse(pc, fdeInfo, *(rs.get()))) {
        DFXLOGE("Failed to parse dwarf instructions?");
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
            DFXLOGE("Failed to Parse CIE?");
            return false;
        }
        addr = fdeInfo.cie.instructionsEnd;
    } else {
        if (!ParseFde(addr, ptr, fdeInfo)) {
            DFXLOGE("Failed to Parse FDE?");
            return false;
        }
        addr = fdeInfo.instructionsEnd;
    }
    return true;
}

void DwarfSection::ParseCieOrFdeHeader(uintptr_t& ptr, FrameDescEntry &fdeInfo, bool& isCieEntry)
{
    uint32_t value32 = 0;
    memory_->Read<uint32_t>(ptr, &value32, true);
    uintptr_t ciePtr = 0;
    uintptr_t instructionsEnd = 0;
    if (value32 == static_cast<uint32_t>(-1)) {
        uint64_t value64;
        memory_->Read<uint64_t>(ptr, &value64, true);
        instructionsEnd = ptr + value64;

        memory_->Read<uint64_t>(ptr, &value64, true);
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
        memory_->Read<uint32_t>(ptr, &value32, false);
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
    DFXLOGU("fdeAddr: %{public}" PRIx64 "", (uint64_t)fdeAddr);
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
            DFXLOGE("ParseFde error, is Cie Entry?");
            return false;
        }
    }
    if (!FillInFde(fdePtr, fdeInfo)) {
        DFXLOGE("ParseFde error, failed to fill FDE?");
        fdeEntries_.erase(fdeAddr);
        return false;
    }
    fdeEntries_[fdeAddr] = fdeInfo;
    return true;
}

bool DwarfSection::FillInFde(uintptr_t ptr, FrameDescEntry &fdeInfo)
{
    if (!ParseCie(fdeInfo.cieAddr, fdeInfo.cieAddr, fdeInfo.cie)) {
        DFXLOGE("Failed to parse CIE?");
        return false;
    }

    if (fdeInfo.cie.segmentSize != 0) {
        // Skip over the segment selector for now.
        ptr += fdeInfo.cie.segmentSize;
    }
    // Parse pc begin and range.
    DFXLOGU("pointerEncoding: %{public}02x", fdeInfo.cie.pointerEncoding);
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
    DFXLOGU("FDE pcStart: %{public}p, pcEnd: %{public}p", (void*)(fdeInfo.pcStart), (void*)(fdeInfo.pcEnd));
    return true;
}

bool DwarfSection::ParseCie(uintptr_t cieAddr, uintptr_t ciePtr, CommonInfoEntry &cieInfo)
{
    DFXLOGU("cieAddr: %{public}" PRIx64 "", (uint64_t)cieAddr);
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
            DFXLOGE("ParseCie error, is not Cie Entry?");
            return false;
        }
        cieInfo = fdeInfo.cie;
    }
    if (!FillInCie(ciePtr, cieInfo)) {
        DFXLOGE("ParseCie error, failed to fill Cie?");
        cieEntries_.erase(cieAddr);
        return false;
    }
    cieEntries_[cieAddr] = cieInfo;
    return true;
}

void DwarfSection::SaveAugStr(uintptr_t& ptr, std::vector<char>& augStr)
{
    uint8_t ch;
    augStr.clear();
    while (memory_->Read<uint8_t>(ptr, &ch, true) && ch != '\0') {
        augStr.push_back(ch);
    }
}

void DwarfSection::ParseAugData(uintptr_t& ptr, CommonInfoEntry &cieInfo, const std::vector<char>& augStr)
{
    MAYBE_UNUSED uintptr_t augSize = memory_->ReadUleb128(ptr);
    DFXLOGU("augSize: %{public}" PRIxPTR "", augSize);
    cieInfo.instructionsOff = ptr + augSize;

    for (size_t i = 1; i < augStr.size(); ++i) {
        switch (augStr[i]) {
            case 'P':
                uint8_t personalityEncoding;
                memory_->Read<uint8_t>(ptr, &personalityEncoding, true);
                cieInfo.personality = memory_->ReadEncodedValue(ptr, personalityEncoding);
                break;
            case 'L':
                memory_->Read<uint8_t>(ptr, &cieInfo.lsdaEncoding, true);
                DFXLOGU("cieInfo.lsdaEncoding: %{public}x", cieInfo.lsdaEncoding);
                break;
            case 'R':
                memory_->Read<uint8_t>(ptr, &cieInfo.pointerEncoding, true);
                DFXLOGU("cieInfo.pointerEncoding: %{public}x", cieInfo.pointerEncoding);
                break;
            case 'S':
                cieInfo.isSignalFrame = true;
                break;
            default:
                break;
        }
    }
}

bool DwarfSection::FillInCie(uintptr_t ptr, CommonInfoEntry &cieInfo)
{
    uint8_t version;
    memory_->Read<uint8_t>(ptr, &version, true);
    DFXLOGU("Cie version: %{public}d", version);
    if (version != DW_EH_VERSION && version != 3 && version != 4 && version != 5) { // 3 4 5 : cie version
        DFXLOGE("Invalid cie version: %{public}d", version);
        lastErrorData_.SetAddrAndCode(ptr, UNW_ERROR_UNSUPPORTED_VERSION);
        return false;
    }

    // save augmentation string
    std::vector<char> augStr;
    SaveAugStr(ptr, augStr);

    // Segment Size
    if (version == 4 || version == 5) { // 4 5 : cie version
        // Skip the Address Size field since we only use it for validation.
        ptr += 1;
        memory_->Read<uint8_t>(ptr, &cieInfo.segmentSize, true);
    } else {
        cieInfo.segmentSize = 0;
    }

    // parse code alignment factor
    cieInfo.codeAlignFactor = (uint32_t)memory_->ReadUleb128(ptr);
    DFXLOGU("codeAlignFactor: %{public}d", cieInfo.codeAlignFactor);

    // parse data alignment factor
    cieInfo.dataAlignFactor = (int32_t)memory_->ReadSleb128(ptr);
    DFXLOGU("dataAlignFactor: %{public}d", cieInfo.dataAlignFactor);

    // parse return address register
    if (version == DW_EH_VERSION) {
        uint8_t val;
        memory_->Read<uint8_t>(ptr, &val, true);
        cieInfo.returnAddressRegister = static_cast<uintptr_t>(val);
    } else {
        cieInfo.returnAddressRegister = (uintptr_t)memory_->ReadUleb128(ptr);
    }
    DFXLOGU("returnAddressRegister: %{public}d", (int)cieInfo.returnAddressRegister);

    // parse augmentation data based on augmentation string
    if (augStr.empty() || augStr[0] != 'z') {
        cieInfo.instructionsOff = ptr;
        return true;
    }
    cieInfo.hasAugmentationData = true;
    ParseAugData(ptr, cieInfo, augStr);
    
    return true;
}
}   // namespace HiviewDFX
}   // namespace OHOS