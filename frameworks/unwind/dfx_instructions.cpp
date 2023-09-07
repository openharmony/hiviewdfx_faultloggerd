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
#include "dwarf_op.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxInstructions"
}

uintptr_t DfxInstructions::SaveReg(DfxRegs& regs, uintptr_t cfa, RegLoc loc)
{
    uintptr_t result = 0;
    uintptr_t location;
    switch (loc.type) {
        case REG_LOC_VAL_OFFSET:
            result = cfa + loc.val;
            break;
        case REG_LOC_MEM_OFFSET:
            location = cfa + loc.val;
            result = memory_->Read<uintptr_t>(location);
            break;
        case REG_LOC_REGISTER:
            location = loc.val;
            result = regs[location];
            break;
        case REG_LOC_MEM_EXPRESSION: {
            DwarfOp<uintptr_t> dwarfOp(memory_);
            location = dwarfOp.Eval(regs, cfa, loc.val);
            result = memory_->Read<uintptr_t>(location);
            break;
        }
        case REG_LOC_VAL_EXPRESSION: {
            DwarfOp<uintptr_t> dwarfOp(memory_);
            result = dwarfOp.Eval(regs, cfa, loc.val);
            break;
        }
        default:
            LOGE("Failed to save register.");
            break;
    }
    return result;
}

bool DfxInstructions::Apply(DfxRegs& regs, RegLocState& rsState)
{
    uintptr_t cfa = 0;
    RegLoc cfaLoc;
    if (rsState.cfaReg != 0) {
        cfaLoc.type = REG_LOC_VAL_OFFSET;
        cfaLoc.val = rsState.cfaRegOffset;
        cfa = SaveReg(regs, regs[rsState.cfaReg], cfaLoc);
    } else if (rsState.cfaExprPtr != 0) {
        cfaLoc.type = REG_LOC_VAL_EXPRESSION;
        cfaLoc.val = rsState.cfaExprPtr;
        cfa = SaveReg(regs, 0, cfaLoc);
    } else {
        LOGE("no cfa info exist?");
        return false;
    }
    LOGU("Update cfa : %llx", (uint64_t)cfa);

    auto qutRegs = DfxRegs::GetQutRegs();
    for (size_t i = 0; i < qutRegs.size(); i++) {
        size_t reg = qutRegs[i];
        if (rsState.locs[reg].type != REG_LOC_UNUSED) {
            regs[reg] = SaveReg(regs, cfa, rsState.locs[reg]);
            LOGU("Update reg[%d] : %llx", reg, (uint64_t)regs[reg]);
        }
    }

    regs.SetSp(cfa);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
