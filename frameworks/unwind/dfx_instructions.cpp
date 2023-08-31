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

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxInstructions"
}

uintptr_t DfxInstructions::SaveReg(uintptr_t cfa, struct RegLoc loc, std::vector<uintptr_t> regs)
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
            result = memory_->Read<uintptr_t>(location);
            break;
        case REG_LOC_MEM_EXPRESSION:
            break;
        case REG_LOC_VAL_EXPRESSION:
            break;
        default:
            LOGE("Failed to save register.");
            break;
    }
    return result;
}

bool DfxInstructions::Apply(std::shared_ptr<DfxRegs> dfxRegs, RegLocState &rsState)
{
    uintptr_t cfa = 0;
    std::vector<uintptr_t> regs = dfxRegs->GetRegsData();
    if (rsState.cfaReg != 0) {
        cfa = regs[rsState.cfaReg] + rsState.cfaRegOffset;
    } else {
        LOGE("no cfa info exist ?\n");
        return false;
    }
    LOGU("Update cfa : %llx \n", (uint64_t)cfa);

    std::vector<uintptr_t> oldRegs = regs;
    for (size_t i = 0; i < QUT_MINI_REGS_SIZE; i++) {
        if (rsState.locs[i].type != REG_LOC_UNUSED) {
            regs[i] = SaveReg(cfa, rsState.locs[i], oldRegs);
            LOGU("Update reg[%d] : %llx \n", REGS_MAP[i], (uint64_t)regs[i]);
        }
    }

    regs[REG_SP] = cfa;
    dfxRegs->SetRegsData(regs);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
