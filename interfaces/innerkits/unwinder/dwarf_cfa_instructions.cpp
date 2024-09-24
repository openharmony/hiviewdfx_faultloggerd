/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <cstring>

#include "dfx_log.h"
#include "dfx_instr_statistic.h"
#include "dfx_regs_qut.h"
#include "dwarf_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxDwarfCfaInstructions"
}

bool DwarfCfaInstructions::Iterate(uintptr_t pc, FrameDescEntry fde,
    uintptr_t instStart, uintptr_t instEnd, RegLocState &rsState)
{
    uintptr_t instPtr = instStart;
    uintptr_t pcOffset = fde.pcStart;
    rsState.pcStart = pcOffset;
    backupRsState_ = rsState;
    const auto& cie = fde.cie;
    while (true) {
        if (pcOffset > pc) {
            rsState.pcEnd = pcOffset;
            break;
        }
        if (instPtr >= instEnd) {
            rsState.pcEnd = fde.pcEnd;
            break;
        }
        rsState.pcStart = pcOffset;

        // Read the cfa information.
        uint8_t opCode;
        if (!memory_->ReadU8(instPtr, &opCode, true)) {
            break;
        }

        if (!DecodeDwCfa(opCode, cie, pcOffset, instPtr, rsState)) {
            break;
        }
    }
    LOGUNWIND("rsState pcStart=%{public}" PRIx64 ", pcEnd=%{public}" PRIx64 "",
        (uint64_t)rsState.pcStart, (uint64_t)rsState.pcEnd);
    return true;
}

bool DwarfCfaInstructions::DecodeDwCfa(uint8_t opCode, CommonInfoEntry cie,
    uintptr_t& pcOffset, uintptr_t& instPtr, RegLocState &rsState)
{
    uintptr_t value = 0;
    int64_t offset = 0;
    uint64_t reg = 0;
    uint64_t reg2 = 0;
    size_t qutIdx = 0;

    switch (opCode) {
        case DW_CFA_nop:
            LOGUNWIND("DW_CFA_nop");
            break;
        case DW_CFA_set_loc:
            value = memory_->ReadEncodedValue(instPtr, (DwarfEncoding)cie.pointerEncoding);
            pcOffset = value;
            LOGUNWIND("DW_CFA_set_loc: new offset=%{public}" PRIu64 "", static_cast<uint64_t>(pcOffset));
            break;
        case DW_CFA_advance_loc1:
            value = memory_->ReadEncodedValue(instPtr, (DwarfEncoding)DW_EH_PE_udata1);
            pcOffset += (value * cie.codeAlignFactor);
            LOGUNWIND("DW_CFA_advance_loc1: new offset=%{public}" PRIu64 "", static_cast<uint64_t>(pcOffset));
            break;
        case DW_CFA_advance_loc2:
            value = memory_->ReadEncodedValue(instPtr, (DwarfEncoding)DW_EH_PE_udata2);
            pcOffset += (value * cie.codeAlignFactor);
            LOGUNWIND("DW_CFA_advance_loc2: %{public}" PRIu64 " to %{public}" PRIx64 "",
                  static_cast<uint64_t>(value * cie.codeAlignFactor), static_cast<uint64_t>(pcOffset));
            break;
        case DW_CFA_advance_loc4:
            value = memory_->ReadEncodedValue(instPtr, (DwarfEncoding)DW_EH_PE_udata4);
            pcOffset += (value * cie.codeAlignFactor);
            LOGUNWIND("DW_CFA_advance_loc4: new offset=%{public}" PRIu64 "", static_cast<uint64_t>(pcOffset));
            break;
        case DW_CFA_offset_extended:
            reg = memory_->ReadUleb128(instPtr);
            offset = (int64_t)(memory_->ReadUleb128(instPtr) * cie.codeAlignFactor);
            LOGUNWIND("DW_CFA_offset_extended: reg=%{public}d", (int)reg);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaOffset, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_MEM_OFFSET;
            rsState.locs[qutIdx].val = offset;
            break;
        case DW_CFA_restore_extended:
            reg = memory_->ReadUleb128(instPtr);
            LOGUNWIND("DW_CFA_restore_extended: reg=%{public}d", (int)reg);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaRestore, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx] = backupRsState_.locs[qutIdx];
            break;
        case DW_CFA_undefined:
            reg = memory_->ReadUleb128(instPtr);
            LOGUNWIND("DW_CFA_undefined: reg=%{public}d", (int)reg);
            if (reg == rsState.returnAddressRegister) {
                rsState.returnAddressUndefined = true;
            }
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaUndefined, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_UNDEFINED;  // cfa offset
            break;
        case DW_CFA_same_value:
            reg = memory_->ReadUleb128(instPtr);
            LOGUNWIND("DW_CFA_same_value: reg=%{public}d", (int)reg);
            if (reg == rsState.returnAddressRegister) {
                rsState.returnAddressSame = true;
            }
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaSame, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_UNUSED;
            break;
        case DW_CFA_register:
            reg = memory_->ReadUleb128(instPtr);
            reg2 = memory_->ReadUleb128(instPtr);
            LOGUNWIND("DW_CFA_register: reg=%{public}d, reg2=%{public}d", (int)reg, (int)reg2);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg2), qutIdx) ||
                !DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaRegister, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_REGISTER;  // register is saved in current register
            rsState.locs[qutIdx].val = static_cast<intptr_t>(reg2);
            break;
        case DW_CFA_remember_state:
            saveRsStates_.push(rsState);
            LOGUNWIND("DW_CFA_remember_state");
            break;
        case DW_CFA_restore_state:
            if (saveRsStates_.size() == 0) {
                LOGUNWIND("DW_CFA_restore_state: Attempt to restore without remember");
            } else {
                rsState = saveRsStates_.top();
                saveRsStates_.pop();
                LOGUNWIND("DW_CFA_restore_state");
            }
            break;
        case DW_CFA_def_cfa:
            reg = memory_->ReadUleb128(instPtr);
            offset = (int64_t)memory_->ReadUleb128(instPtr);
            rsState.cfaReg = (uint32_t)reg;
            rsState.cfaRegOffset = (int32_t)offset;
            LOGUNWIND("DW_CFA_def_cfa: reg=%{public}d, offset=%{public}" PRIu64 "", (int)reg, offset);
            break;
        case DW_CFA_def_cfa_register:
            reg = memory_->ReadUleb128(instPtr);
            rsState.cfaReg = (uint32_t)reg;
            LOGUNWIND("DW_CFA_def_cfa_register: reg=%{public}d", (int)reg);
            break;
        case DW_CFA_def_cfa_offset:
            rsState.cfaRegOffset = (int32_t)memory_->ReadUleb128(instPtr);
            LOGUNWIND("DW_CFA_def_cfa_offset: cfaRegOffset=%{public}d", rsState.cfaRegOffset);
            break;
        case DW_CFA_offset_extended_sf:
            reg = memory_->ReadUleb128(instPtr);
            offset = (int64_t)(memory_->ReadSleb128(instPtr)) * cie.dataAlignFactor;
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaOffset, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_MEM_OFFSET;
            rsState.locs[qutIdx].val = offset;
            break;
        case DW_CFA_def_cfa_sf:
            reg = memory_->ReadUleb128(instPtr);
            offset = (int64_t)(memory_->ReadSleb128(instPtr)) * cie.dataAlignFactor;
            LOGUNWIND("DW_CFA_def_cfa_sf: reg=%{public}d, offset=%{public}d", rsState.cfaReg, rsState.cfaRegOffset);
            rsState.cfaReg = (uint32_t)reg;
            rsState.cfaRegOffset = (int32_t)offset;
            break;
        case DW_CFA_def_cfa_offset_sf:
            offset = (int64_t)(memory_->ReadSleb128(instPtr)) * cie.dataAlignFactor;
            rsState.cfaRegOffset = (int32_t)offset;
            LOGUNWIND("DW_CFA_def_cfa_offset_sf: offset=%{public}d", rsState.cfaRegOffset);
            break;
        case DW_CFA_val_offset:
            reg = memory_->ReadUleb128(instPtr);
            offset = (int64_t)(memory_->ReadUleb128(instPtr) * cie.codeAlignFactor);
            LOGUNWIND("DW_CFA_val_offset: reg=%{public}d, offset=%{public}" PRIu64 "", (int)reg, offset);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaValOffset, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_VAL_OFFSET;
            rsState.locs[qutIdx].val = offset;
            break;
        case DW_CFA_val_offset_sf:
            reg = memory_->ReadUleb128(instPtr);
            offset = memory_->ReadSleb128(instPtr) * static_cast<int64_t>(cie.codeAlignFactor);
            LOGUNWIND("DW_CFA_val_offset_sf: reg=%{public}d, offset=%{public}" PRIu64 "", (int)reg, offset);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaValOffset, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_VAL_OFFSET;
            rsState.locs[qutIdx].val = offset;
            break;
        case DW_CFA_def_cfa_expression:
            rsState.cfaReg = 0;
            rsState.cfaExprPtr = instPtr;
            instPtr += static_cast<uintptr_t>(memory_->ReadUleb128(instPtr));
            break;
        case DW_CFA_expression:
            reg = memory_->ReadUleb128(instPtr);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaExpr, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
            } else {
                rsState.locs[qutIdx].type = REG_LOC_MEM_EXPRESSION;
                rsState.locs[qutIdx].val = static_cast<intptr_t>(instPtr);
            }
            instPtr += static_cast<uintptr_t>(memory_->ReadUleb128(instPtr));
            break;
        case DW_CFA_val_expression:
            reg = memory_->ReadUleb128(instPtr);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaValExpr, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
            } else {
                rsState.locs[qutIdx].type = REG_LOC_VAL_EXPRESSION;
                rsState.locs[qutIdx].val = static_cast<intptr_t>(instPtr);
            }
            instPtr += static_cast<uintptr_t>(memory_->ReadUleb128(instPtr));
            break;
#if defined(__aarch64__)
        case DW_CFA_AARCH64_negate_ra_state:
            LOGUNWIND("DW_CFA_AARCH64_negate_ra_state");
            break;
#endif
        case DW_CFA_GNU_negative_offset_extended:
            reg = memory_->ReadUleb128(instPtr);
            offset = -(int64_t)memory_->ReadUleb128(instPtr);
            LOGUNWIND("DW_CFA_GNU_negative_offset_extended: reg=%{public}d, offset=%{public}" PRIu64 "",
                (int)reg, offset);
            if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                INSTR_STATISTIC(UnsupportedDwCfaOffset, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                break;
            }
            rsState.locs[qutIdx].type = REG_LOC_MEM_OFFSET;
            rsState.locs[qutIdx].val = offset;
            break;

        default:
            uint8_t operand = opCode & 0x3F;
            // Check the 2 high bits.
            switch (opCode & 0xC0) {
                case DW_CFA_advance_loc:
                    pcOffset += operand * cie.codeAlignFactor;
                    LOGUNWIND("DW_CFA_advance_loc: pcOffset=%{public}" PRIu64 "", static_cast<uint64_t>(pcOffset));
                    break;
                case DW_CFA_offset:
                    reg = operand;
                    offset = (int64_t)memory_->ReadUleb128(instPtr) * cie.dataAlignFactor;
                    LOGUNWIND("DW_CFA_offset: reg=%{public}d, offset=%{public}" PRId64 "", (int)reg, offset);
                    if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                        INSTR_STATISTIC(UnsupportedDwCfaOffset, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                        break;
                    }
                    rsState.locs[qutIdx].type = REG_LOC_MEM_OFFSET;
                    rsState.locs[qutIdx].val = offset;
                    break;
                case DW_CFA_restore:
                    reg = operand;
                    LOGUNWIND("DW_CFA_restore: reg=%{public}d", (int)reg);
                    if (!DfxRegsQut::IsQutReg(static_cast<uint16_t>(reg), qutIdx)) {
                        INSTR_STATISTIC(UnsupportedDwCfaRestore, reg, UNW_ERROR_UNSUPPORTED_QUT_REG);
                        break;
                    }
                    rsState.locs[qutIdx] = backupRsState_.locs[qutIdx];
                    break;
                default:
                    LOGUNWIND("DW_CFA_unknown: opcode=0x%{public}02x", opCode);
                    break;
            }
    }
    return true;
}

bool DwarfCfaInstructions::Parse(uintptr_t pc, FrameDescEntry fde, RegLocState &rsState)
{
    const auto& cie = fde.cie;
    rsState.returnAddressRegister = cie.returnAddressRegister;

    LOGUNWIND("Iterate cie operations");
    if (!Iterate(pc, fde, cie.instructionsOff, cie.instructionsEnd, rsState)) {
        LOGERROR("Failed to run cie inst");
        return false;
    }

    LOGUNWIND("Iterate fde operations");
    if (!Iterate(pc, fde, fde.instructionsOff, fde.instructionsEnd, rsState)) {
        LOGERROR("Failed to run fde inst");
        return false;
    }
    return true;
}
}   // namespace HiviewDFX
}   // namespace OHOS
