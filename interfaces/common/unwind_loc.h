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
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
enum RegLocEnum : uint8_t {
    REG_LOC_UNUSED = 0,
    REG_LOC_UNDEFINED,
    REG_LOC_CFA_OFFSET,      // register stored in the offset from cfa
    REG_LOC_REGISTER,        // register stored in register
    REG_LOC_VAL_OFFSET,      // register value is offset from cfa
    REG_LOC_CFA_EXPRESSION,  // register stored in expression result
    REG_LOC_VAL_EXPRESSION,  // register value is expression result
};

struct RegLoc {
    RegLocEnum type;            /* see DWARF_LOC_* macros.  */
    uintptr_t val;
};

// saved register status after running call frame instructions
// it should describe how register saved
struct RegLocState {
    uint32_t cfaRegister;
    int32_t cfaRegisterOffset;
    uintptr_t cfaExpressionPtr;
    int32_t pcOffset; // pc offset of this register state
    RegLocEnum type; // by what mean to get this register state,
    RegLoc loc[DWARF_PRESERVED_REGS_NUM];
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
