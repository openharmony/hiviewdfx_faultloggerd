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

#include "dfx_instructions.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_log.h"
#include "dfx_instr_statistic.h"
#include "dfx_regs_qut.h"
#include "dwarf_op.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxInstructions"
}

bool DfxInstructions::Flush(DfxRegs& regs, std::shared_ptr<DfxMemory> memory, uintptr_t cfa, RegLoc loc, uintptr_t& val)
{
    if (memory == nullptr) {
        return false;
    }
    uintptr_t location;
    switch (loc.type) {
        case REG_LOC_VAL_OFFSET:
            val = cfa + static_cast<uintptr_t>(loc.val);
            break;
        case REG_LOC_MEM_OFFSET:
            location = cfa + static_cast<uintptr_t>(loc.val);
            memory->ReadUptr(location, &val);
            break;
        case REG_LOC_REGISTER:
            location = static_cast<uintptr_t>(loc.val);
            if (location >= regs.RegsSize()) {
                LOGERROR("Illegal register location");
                return false;
            }
            val = regs[location];
            break;
        case REG_LOC_MEM_EXPRESSION: {
            DwarfOp<uintptr_t> dwarfOp(memory);
            location = dwarfOp.Eval(regs, cfa, loc.val);
            memory->ReadUptr(location, &val);
            break;
        }
        case REG_LOC_VAL_EXPRESSION: {
            DwarfOp<uintptr_t> dwarfOp(memory);
            val = dwarfOp.Eval(regs, cfa, loc.val);
            break;
        }
        default:
            LOGERROR("Failed to save register.");
            return false;
    }
    return true;
}

bool DfxInstructions::Apply(std::shared_ptr<DfxMemory> memory, DfxRegs& regs, RegLocState& rsState, uint16_t& errCode)
{
    uintptr_t cfa = 0;
    RegLoc cfaLoc;
    if (rsState.cfaReg != 0) {
        cfa = regs[rsState.cfaReg] + static_cast<uint32_t>(rsState.cfaRegOffset);
    } else if (rsState.cfaExprPtr != 0) {
        cfaLoc.type = REG_LOC_VAL_EXPRESSION;
        cfaLoc.val = static_cast<intptr_t>(rsState.cfaExprPtr);
        if (!Flush(regs, memory, 0, cfaLoc, cfa)) {
            LOGERROR("Failed to update cfa.");
            return false;
        }
    } else {
        LOGERROR("no cfa info exist?");
        INSTR_STATISTIC(UnsupportedDefCfa, rsState.cfaReg, UNW_ERROR_NOT_SUPPORT);
        return false;
    }
    LOGUNWIND("Update cfa : %{public}" PRIx64 "", (uint64_t)cfa);

    for (size_t i = 0; i < rsState.locs.size(); i++) {
        if (rsState.locs[i].type != REG_LOC_UNUSED) {
            size_t reg = DfxRegsQut::GetQutRegs()[i];
            if (Flush(regs, memory, cfa, rsState.locs[i], regs[reg])) {
                LOGUNWIND("Update reg[%{public}zu] : %{public}" PRIx64 "", reg, (uint64_t)regs[reg]);
            }
        }
    }

    regs.SetSp(cfa);

    if (rsState.returnAddressUndefined) {
        regs.SetPc(0);
        errCode = UNW_ERROR_RETURN_ADDRESS_UNDEFINED;
        return false;
    } else {
        if (rsState.returnAddressRegister >= REG_EH && rsState.returnAddressRegister < REG_LAST) {
            LOGUNWIND("returnAddressRegister: %{public}d", (int)rsState.returnAddressRegister);
            regs.SetPc(regs[rsState.returnAddressRegister]);
        } else {
            LOGERROR("returnAddressRegister: %{public}d error", (int)rsState.returnAddressRegister);
            errCode = UNW_ERROR_ILLEGAL_VALUE;
            return false;
        }
    }
    if (rsState.returnAddressSame) {
        errCode = UNW_ERROR_RETURN_ADDRESS_SAME;
        return false;
    }

    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
