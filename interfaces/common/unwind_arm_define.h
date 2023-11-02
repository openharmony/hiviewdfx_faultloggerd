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
#ifndef UNWIND_ARM_DEFINE_H
#define UNWIND_ARM_DEFINE_H

#include <cinttypes>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 256
#define DWARF_PRESERVED_REGS_NUM 128

enum RegsEnumArm : uint16_t {
    REG_ARM_R0 = 0,
    REG_ARM_R1,
    REG_ARM_R2,
    REG_ARM_R3,
    REG_ARM_R4,
    REG_ARM_R5,
    REG_ARM_R6,
    REG_ARM_R7,
    REG_ARM_R8,
    REG_ARM_R9,
    REG_ARM_R10,
    REG_ARM_R11,
    REG_ARM_R12,
    REG_ARM_R13,
    REG_ARM_R14,
    REG_ARM_R15,
    REG_ARM_LAST,

    REG_FP = REG_ARM_R11,
    REG_SP = REG_ARM_R13,
    REG_LR = REG_ARM_R14,
    REG_PC = REG_ARM_R15,
    REG_EH = REG_ARM_R0,
    REG_LAST = REG_ARM_LAST,
};

static const std::vector<uint16_t> QUT_REGS {
    REG_ARM_R7,
    REG_ARM_R11,
    REG_SP,
    REG_PC,
    REG_LR,
};

struct RegsUserArm {
    uint32_t regs[16]; // 16
};

typedef struct UnwindUContext {
    RegsUserArm userRegs;
} UnwindUContext_t;
} // namespace HiviewDFX
} // namespace OHOS
#endif
