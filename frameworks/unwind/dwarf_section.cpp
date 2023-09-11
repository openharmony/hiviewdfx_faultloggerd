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
#include "dfx_log.h"
#include "dwarf_cfa_instructions.h"
#include "dfx_instructions.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxDwarfSection"
}

bool DwarfSection::Step(uintptr_t fdeAddr, std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    LOGU("Step: fdeAddr=%p", (void*)fdeAddr);
    lastErrorData_.code = UNW_ERROR_NONE;
    lastErrorData_.addr = fdeAddr;
    // 3.parse cie and fde
    CommonInfoEntry cieInfo;
    FrameDescEntry fdeInfo;
    if (!ParseFde(fdeAddr, fdeInfo, cieInfo)) {
        LOGE("Failed to parse fde?");
        lastErrorData_.code = UNW_ERROR_DWARF_INVALID_FDE;
        return false;
    }

    LOGU("pc: %p, FDE start: %p", (void*) regs->GetPc(), (void*) (fdeInfo.pcStart) );
    // 4. parse dwarf instructions and get cache rs
    DwarfCfaInstructions dwarfInstructions(memory_);
    if (!dwarfInstructions.Parse(regs->GetPc(), cieInfo, fdeInfo, *(rs.get()))) {
        LOGE("Failed to parse dwarf instructions?");
        lastErrorData_.code = UNW_ERROR_DWARF_INVALID_INSTR;
        return false;
    }

    // 5. update regs and regs state
    DfxInstructions instructions(memory_);
    bool ret = instructions.Apply(*(regs.get()), *(rs.get()));

    regs->SetReg(REG_PC, regs->GetReg(REG_LR));
    return ret;
}

bool DwarfSection::ParseFde(uintptr_t fdeAddr, FrameDescEntry &fdeInfo, CommonInfoEntry &cieInfo)
{
    LOGU("fdeAddr: %llx", (uint64_t)fdeAddr);
    uintptr_t ptr = fdeAddr;
    uintptr_t length = static_cast<uintptr_t>(memory_->Read<uint32_t>(ptr, true));
    if (length == 0xffffffff) {
        length = static_cast<uintptr_t>(memory_->Read<uint64_t>(ptr, true));
    }
    if (length == 0) {
        LOGE("Failed to parse FDE, length?");
        return false;
    }

    uintptr_t nextFdeAddr = ptr + length;
    uint32_t ciePtr = memory_->Read<uint32_t>(ptr, false);
    if (ciePtr == 0) {
        LOGE("Failed to parse FDE, ciePtr?");
        return false;
    }
    uintptr_t cieAddr = static_cast<uintptr_t>(ptr - ciePtr);
    LOGU("cieAddr: %llx", (uint64_t)cieAddr);
    ptr += sizeof(uint32_t);

    fdeInfo.fdeStart = fdeAddr;
    fdeInfo.fdeEnd = nextFdeAddr;
    fdeInfo.cieAddr = cieAddr;
    if (!FillInFde(ptr, fdeInfo, cieInfo)) {
        LOGE("Failed to fill FDE?");
        return false;
    }
    return true;
}

bool DwarfSection::ParseCie(uintptr_t cieAddr, CommonInfoEntry &cieInfo)
{
    LOGU("cieAddr: %p", (void *)cieAddr);
    cieInfo.cieStart = cieAddr;
    uintptr_t ptr = cieAddr;
    if (!FillInCieHeader(ptr, cieInfo) || !FillInCie(ptr, cieInfo)) {
        return false;
    }
    return true;
}

bool DwarfSection::FillInCieHeader(uintptr_t& addr, CommonInfoEntry &cieInfo)
{
    cieInfo.lsdaEncoding = DW_EH_PE_omit;
    uintptr_t length = static_cast<uintptr_t>(memory_->Read<uint32_t>(addr, true));
    if (length == 0xffffffff) {
        length = static_cast<uintptr_t>(memory_->Read<uint64_t>(addr, true));
    }
    if (length == 0) {
        LOGE("Failed to FillInCieHeader, length?");
        return false;
    }
    cieInfo.cieEnd = addr + length;
    uint32_t cieId = memory_->Read<uint32_t>(addr, true);
    if (cieId != cieIdValue_) {
        LOGE("Failed to FillInCieHeader, cieId?");
        lastErrorData_.code = UNW_ERROR_ILLEGAL_VALUE;
        return false;
    }
    return true;
}

bool DwarfSection::FillInCie(uintptr_t& addr, CommonInfoEntry &cieInfo)
{
    uintptr_t ptr = addr;
    uint8_t version = memory_->Read<uint8_t>(ptr, true);
    LOGU("Cie version: %d", version);
    if (version != DW_EH_VERSION) {
        LOGE("Invalid cie version: %d", version);
        lastErrorData_.code = UNW_ERROR_UNSUPPORTED_VERSION;
        return false;
    }

    // save start addr of augmentation and skip it
    uintptr_t strPtr = ptr;
    uintptr_t strLen = 0;
    while (memory_->Read<uint8_t>(ptr, true) != '\0'){
        strLen++; // we may not deref directly in remote case
    }

    // parse code aligment factor
    cieInfo.codeAlignFactor = (uint32_t)memory_->ReadUleb128(ptr);
    LOGU("codeAlignFactor: %d", cieInfo.codeAlignFactor);

    // parse data alignment factor
    cieInfo.dataAlignFactor = (int32_t)memory_->ReadSleb128(ptr);
    LOGU("dataAlignFactor: %d", cieInfo.dataAlignFactor);

    // parse return address register
    cieInfo.returnAddressRegister = (uintptr_t)memory_->ReadUleb128(ptr);
    LOGU("returnAddressRegister: %d", (int)cieInfo.returnAddressRegister);

    // parse augmentation data based on augmentation string
    if (memory_->Read<uint8_t>(strPtr, true) == 'z') {
        cieInfo.hasAugmentationData = true;

        // parse augmentation data length
        uintptr_t dataLen = memory_->ReadUleb128(ptr);
        LOGU("dataLen: %x", dataLen);
        for (; strLen > 0; strLen--) {
            uint8_t augment = memory_->Read<uint8_t>(strPtr, true);
            switch (augment) {
                case 'P':
                    cieInfo.personalityEncoding = memory_->Read<uint8_t>(ptr, true);
                    cieInfo.personality = memory_->ReadEncodedValue(
                        ptr, (DwarfEncoding)cieInfo.personalityEncoding, dataOffset_);
                    break;
                case 'L':
                    cieInfo.lsdaEncoding = memory_->Read<uint8_t>(ptr, true);
                    break;
                case 'R':
                    cieInfo.pointerEncoding = memory_->Read<uint8_t>(ptr, true);
                    LOGU("cieInfo.pointerEncoding: %x", cieInfo.pointerEncoding);
                    break;
                default:
                    break;
            }
        }
    }
    cieInfo.instructions = ptr;
    return true;
}

bool DwarfSection::FillInFde(uintptr_t& addr, FrameDescEntry &fdeInfo, CommonInfoEntry &cieInfo)
{
    // TODO: cie cache?
    if (!ParseCie(fdeInfo.cieAddr, cieInfo)) {
        LOGE("Failed to fill CIE?");
        return false;
    }

    uintptr_t ptr = addr;

    // Parse pc begin and range.
    LOGU("pointerEncoding: %02x", cieInfo.pointerEncoding);
    uintptr_t pcStart = memory_->ReadEncodedValue(ptr, (DwarfEncoding)cieInfo.pointerEncoding);
    LOGU("pcStart: %p", (void *)pcStart);
    uintptr_t pcRange = memory_->ReadEncodedValue(ptr, (DwarfEncoding)(cieInfo.pointerEncoding & 0x0F));
    LOGU("pcRange: %p", (void *)pcRange);

    fdeInfo.lsda = 0;
    // Check for augmentation length.
    if (cieInfo.hasAugmentationData) {
        uintptr_t augLen = memory_->ReadUleb128(ptr);
        uintptr_t instructionsPtr = ptr + augLen;
        if (cieInfo.lsdaEncoding != DW_EH_PE_omit) {
            uintptr_t lsdaPtr = ptr;
            if (memory_->ReadEncodedValue(ptr, (DwarfEncoding)(cieInfo.lsdaEncoding & 0x0F)) != 0) {
                fdeInfo.lsda = memory_->ReadEncodedValue(lsdaPtr, (DwarfEncoding)cieInfo.lsdaEncoding);
            }
        }
        ptr = instructionsPtr;
    }

    fdeInfo.instructions = ptr;
    fdeInfo.pcStart = pcStart;
    fdeInfo.pcEnd = pcStart + pcRange;
    LOGU("FDE pcStart: %p, pcEnd: %p",(void*)(fdeInfo.pcStart), (void*)(fdeInfo.pcEnd));
    return true;
}
}   // namespace HiviewDFX
}   // namespace OHOS