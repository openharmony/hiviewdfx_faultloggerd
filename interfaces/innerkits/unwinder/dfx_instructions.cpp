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

#include "dfx_instructions.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "dfx_define.h"
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
    uintptr_t location;
    switch (loc.type) {
        case REG_LOC_VAL_OFFSET:
            val = cfa + static_cast<uintptr_t>(loc.val);
            break;
        case REG_LOC_MEM_OFFSET:
            location = cfa + loc.val;
            memory->ReadUptr(location, &val);
            break;
        case REG_LOC_REGISTER:
            location = loc.val;
            if (location >= regs.RegsSize()) {
                LOGE("illegal register location");
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
            LOGE("Failed to save register.");
            return false;
    }
    return true;
}

bool DfxInstructions::Apply(std::shared_ptr<DfxMemory> memory, DfxRegs& regs, RegLocState& rsState)
{
    uintptr_t cfa = 0;
    RegLoc cfaLoc;
    if (rsState.cfaReg != 0) {
        cfa = regs[rsState.cfaReg] + static_cast<uint32_t>(rsState.cfaRegOffset);
    } else if (rsState.cfaExprPtr != 0) {
        cfaLoc.type = REG_LOC_VAL_EXPRESSION;
        cfaLoc.val = static_cast<intptr_t>(rsState.cfaExprPtr);
        if (!Flush(regs, memory, 0, cfaLoc, cfa)) {
            LOGE("Failed to update cfa.");
            return false;
        }
    } else {
        LOGE("no cfa info exist?");
        INSTR_STATISTIC(UnsupportedDefCfa, rsState.cfaReg, UNW_ERROR_NOT_SUPPORT);
        return false;
    }
    LOGU("Update cfa : %llx", (uint64_t)cfa);

    for (size_t i = 0; i < rsState.locs.size(); i++) {
        if (rsState.locs[i].type != REG_LOC_UNUSED) {
            size_t reg = DfxRegsQut::GetQutRegs()[i];
            if (Flush(regs, memory, cfa, rsState.locs[i], regs[reg])) {
                LOGU("Update reg[%d] : %llx", reg, (uint64_t)regs[reg]);
            }
        }
    }

    regs.SetSp(cfa);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
