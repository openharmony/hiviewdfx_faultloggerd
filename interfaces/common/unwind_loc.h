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
#ifndef UNWIND_LOC_H
#define UNWIND_LOC_H

#include <cinttypes>
#include <string>
#include <vector>
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
enum RegLocEnum : uint8_t {
    REG_LOC_UNUSED = 0,
    REG_LOC_UNDEFINED,
    REG_LOC_VAL_OFFSET,      // register value is offset from cfa, cfa = cfa + off
    REG_LOC_MEM_OFFSET,      // register stored in the offset from cfa, cfa = [r14 + off], r14 = [cfa + off]
    REG_LOC_REGISTER,        // register stored in register, cfa = [r14], pc = [lr]
    REG_LOC_VAL_EXPRESSION,  // register value is expression result, r11 = cfa + expr_result
    REG_LOC_MEM_EXPRESSION,  // register stored in expression result, r11 = [cfa + expr_result]
};

struct RegLoc {
    RegLocEnum type;            /* see DWARF_LOC_* macros.  */
    intptr_t val;
};

// saved register status after running call frame instructions
// it should describe how register saved
struct RegLocState {
    uint32_t cfaReg; // cfa = [r14]
    union {
        int32_t cfaRegOffset; // cfa = cfa + offset
        uintptr_t cfaExprPtr; // cfa = expr
    };
    int32_t pcOffset; // pc offset of this register state
    RegLoc locs[REGS_MAX_SIZE];
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
