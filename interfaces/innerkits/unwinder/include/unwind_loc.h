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

#ifndef UNWIND_LOC_H
#define UNWIND_LOC_H

#include <cinttypes>
#include <string>
#include <vector>

#include "dfx_regs_qut.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
enum RegLocEnum : uint8_t {
    REG_LOC_UNUSED = 0,     // register has same value as in prev. frame
    REG_LOC_UNDEFINED,      // register isn't saved at all
    REG_LOC_VAL_OFFSET,     // register that is the value, cfa = cfa + off
    REG_LOC_MEM_OFFSET,     // register saved at CFA-relative address, cfa = [r14 + off], r14 = [cfa + off]
    REG_LOC_REGISTER,       // register saved in another register, cfa = [r14], pc = [lr]
    REG_LOC_VAL_EXPRESSION, // register value is expression result, r11 = cfa + expr_result
    REG_LOC_MEM_EXPRESSION, // register stored in expression result, r11 = [cfa + expr_result]
};

struct RegLoc {
    RegLocEnum type = REG_LOC_UNUSED; /* see DWARF_LOC_* macros. */
    intptr_t val = 0;
};

// saved register status after running call frame instructions
// it should describe how register saved
struct RegLocState {
    RegLocState() { locs.resize(DfxRegsQut::GetQutRegsSize()); }
    explicit RegLocState(size_t locsSize) { locs.resize(locsSize); }
    ~RegLocState() = default;

    uintptr_t pcStart = 0;
    uintptr_t pcEnd = 0;
    uint32_t cfaReg = 0; // cfa = [r14]
    union {
        int32_t cfaRegOffset = 0; // cfa = cfa + offset
        uintptr_t cfaExprPtr; // cfa = expr
    };
    bool returnAddressUndefined = false;
    bool returnAddressSame = false;
    uint16_t returnAddressRegister = 0; // lr
    uintptr_t pseudoReg = 0;
    std::vector<RegLoc> locs;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
