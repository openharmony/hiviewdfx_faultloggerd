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

#include "dwarf_cfa_instructions.h"
#include <stdio.h>
#include <string.h>
#include "dfx_log.h"
#include "dwarf_define.h"
#include "dwarf_expression.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxDwarfCfaInstructions"
}

bool DwarfCfaInstructions::Iterate(CommonInfoEntry &cieInfo, uintptr_t instStart, uintptr_t instEnd,
    uintptr_t pc, uintptr_t pcStart, RegLocState &rsState)
{
    uintptr_t instPtr = instStart;
    uintptr_t codeOffset = 0;
    RegLocState prevRs = rsState;
    while ((instPtr < instEnd) && (codeOffset < pc - pcStart)) {
        LOGU("instPtr:%p instEnd:%p codeOffset:%d relPc:%p\n",
            (void*)instPtr , (void*)instEnd , (int)codeOffset, (void*)(pc - pcStart));
        int64_t offset = 0;
        uint64_t reg = 0;
        uint64_t reg2 = 0;
        // Read the cfa information.
        uint8_t opCode = memory_->Read<uint8_t>(instPtr, true);
        switch (opCode) {
            case DW_CFA_nop:
                LOGU("DW_CFA_nop");
                break;
            case DW_CFA_set_loc:
                codeOffset = memory_->ReadEncodedValue(instPtr, (DwarfEncoding)cieInfo.pointerEncoding);
                LOGU("DW_CFA_set_loc");
                break;
            case DW_CFA_advance_loc1:
                codeOffset += (memory_->Read<uint8_t>(instPtr, true) * cieInfo.codeAlignFactor);
                LOGU("DW_CFA_advance_loc1: new offset=%" PRIu64 "", static_cast<uint64_t>(codeOffset));
                break;
            case DW_CFA_advance_loc2:
                codeOffset += (memory_->Read<uint16_t>(instPtr, true) * cieInfo.codeAlignFactor);
                LOGU("DW_CFA_advance_loc2: new offset=%" PRIu64 "", static_cast<uint64_t>(codeOffset));
                break;
            case DW_CFA_advance_loc4:
                codeOffset += (memory_->Read<uint32_t>(instPtr, true) * cieInfo.codeAlignFactor);
                LOGU("DW_CFA_advance_loc4: new offset=%" PRIu64 "", static_cast<uint64_t>(codeOffset));
                break;
            case DW_CFA_offset_extended:
                reg = memory_->ReadUleb128(instPtr);
                offset = (int64_t)memory_->ReadUleb128(instPtr) * cieInfo.codeAlignFactor;
                rsState.locs[reg].type = REG_LOC_MEM_OFFSET;
                rsState.locs[reg].val = offset;
                break;
            case DW_CFA_restore_extended:
                reg = memory_->ReadUleb128(instPtr);
                rsState.locs[reg] = prevRs.locs[reg];
                break;
            case DW_CFA_undefined:
                reg = memory_->ReadUleb128(instPtr);
                rsState.locs[reg].type = REG_LOC_UNDEFINED;  // cfa offset
                break;
            case DW_CFA_same_value:
                reg = memory_->ReadUleb128(instPtr);
                rsState.locs[reg].type = REG_LOC_UNUSED;
                break;
            case DW_CFA_register:
                reg = memory_->ReadUleb128(instPtr);
                reg2 = memory_->ReadUleb128(instPtr);
                rsState.locs[reg].type = REG_LOC_REGISTER;  // register is saved in current register
                rsState.locs[reg].val = reg2;
                break;
            case DW_CFA_def_cfa:
                reg = memory_->ReadUleb128(instPtr);
                offset = (int64_t)memory_->ReadUleb128(instPtr);
                rsState.cfaReg = (uint32_t)reg;
                rsState.cfaRegOffset = (int32_t)offset;
                LOGU("DW_CFA_def_cfa(reg=%" PRIu64 ", offset=%" PRIu64 ")", reg, offset);
                break;
            case DW_CFA_def_cfa_register:
                reg = memory_->ReadUleb128(instPtr);
                rsState.cfaReg = (uint32_t)reg;
                LOGU("DW_CFA_def_cfa_register(%d)", rsState.cfaReg);
                break;
            case DW_CFA_def_cfa_offset:
                rsState.cfaRegOffset = (int32_t)memory_->ReadUleb128(instPtr);
                LOGU("DW_CFA_def_cfa_offset(%d)", rsState.cfaRegOffset);
                break;
            case DW_CFA_offset_extended_sf:
                reg = memory_->ReadUleb128(instPtr);
                offset = (int64_t)memory_->ReadSleb128(instPtr) * cieInfo.dataAlignFactor;
                rsState.locs[reg].type = REG_LOC_MEM_OFFSET;
                rsState.locs[reg].val = offset;
                break;
            case DW_CFA_def_cfa_sf:
                reg = memory_->ReadUleb128(instPtr);
                offset = (int64_t)memory_->ReadSleb128(instPtr) * cieInfo.dataAlignFactor;
                rsState.cfaReg = (uint32_t)reg;
                rsState.cfaRegOffset = (int32_t)offset;
                LOGU("DW_CFA_def_cfa_sf(%d, %d)", rsState.cfaReg, rsState.cfaRegOffset);
                break;
            case DW_CFA_def_cfa_offset_sf:
                offset = (int64_t)memory_->ReadSleb128(instPtr) * cieInfo.dataAlignFactor;
                rsState.cfaRegOffset = (int32_t)offset;
                LOGU("DW_CFA_def_cfa_offset_sf(%d)", rsState.cfaRegOffset);
                break;
            case DW_CFA_val_offset:
                reg = memory_->ReadUleb128(instPtr);
                offset = (int64_t)memory_->ReadUleb128(instPtr) * cieInfo.codeAlignFactor;
                rsState.locs[reg].type = REG_LOC_VAL_OFFSET;
                rsState.locs[reg].val = offset;
                break;
            case DW_CFA_val_offset_sf:
                reg = memory_->ReadUleb128(instPtr);
                offset = (int64_t)memory_->ReadSleb128(instPtr) * cieInfo.codeAlignFactor;
                rsState.locs[reg].type = REG_LOC_VAL_OFFSET;
                rsState.locs[reg].val = offset;
                break;
            case DW_CFA_def_cfa_expression:
                rsState.cfaReg = 0;
                rsState.cfaExprPtr = instPtr;
                instPtr += static_cast<uintptr_t>(memory_->ReadUleb128(instPtr));
                break;
            case DW_CFA_expression:
                reg = memory_->ReadUleb128(instPtr);
                rsState.locs[reg].type = REG_LOC_MEM_EXPRESSION;
                rsState.locs[reg].val = instPtr;
                instPtr += static_cast<uintptr_t>(memory_->ReadUleb128(instPtr));
                break;
            case DW_CFA_val_expression:
                reg = memory_->ReadUleb128(instPtr);
                rsState.locs[reg].type = REG_LOC_VAL_EXPRESSION;
                rsState.locs[reg].val = instPtr;
                instPtr += static_cast<uintptr_t>(memory_->ReadUleb128(instPtr));
                break;

            default:
                uint8_t operand = opCode & 0x3F;
                // Check the 2 high bits.
                switch (opCode & 0xC0) {
                    case DW_CFA_advance_loc:
                        codeOffset += operand * cieInfo.codeAlignFactor;
                        LOGU("DW_CFA_advance_loc: codeOffset=%" PRIu64 "", static_cast<uint64_t>(codeOffset));
                        break;
                    case DW_CFA_offset:
                        reg = operand;
                        offset = (int64_t)memory_->ReadUleb128(instPtr) * cieInfo.dataAlignFactor;
                        rsState.locs[reg].type = REG_LOC_MEM_OFFSET;
                        rsState.locs[reg].val = offset;
                        LOGU("DW_CFA_offset: reg=%d, offset=%" PRId64 "", (int)reg, offset);
                        break;
                    case DW_CFA_restore:
                        reg = operand;
                        rsState.locs[reg] = prevRs.locs[reg];
                        LOGU("DW_CFA_restore: reg=%d", (int)reg);
                        break;
                    default:
                        LOGU("Unknown DW_CFA opcode 0x%02x", opCode);
                        break;
                }
        }
    }
    return true;
}

bool DwarfCfaInstructions::Parse(uintptr_t pc, CommonInfoEntry &cie, FrameDescEntry &fde, RegLocState &rsState)
{
    LOGU("Iterate cie operations");
    if (!Iterate(cie, cie.instructions, cie.cieEnd, pc, fde.pcStart, rsState)) {
        LOGE("Failed to run cie inst");
        return false;
    }

    LOGU("Iterate fde operations");
    if (!Iterate(cie, fde.instructions, fde.fdeEnd, pc, fde.pcStart, rsState)) {
        LOGE("Failed to run fde inst");
        return false;
    }
    return true;
}
}   // namespace HiviewDFX
}   // namespace OHOS
