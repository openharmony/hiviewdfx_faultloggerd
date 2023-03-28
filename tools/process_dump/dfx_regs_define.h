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
#ifndef DFX_REGS_DEFINE_H
#define DFX_REGS_DEFINE_H

#include <cstdint>

namespace OHOS {
namespace HiviewDFX {
static const int REGS_PRINT_LEN_ARM = 256;
static const int REGS_PRINT_LEN_ARM64 = 1024;
static const int REGS_PRINT_LEN_X86 = 512;

enum RegisterArm {
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

    REG_ARM_SP = REG_ARM_R13,
    REG_ARM_LR = REG_ARM_R14,
    REG_ARM_PC = REG_ARM_R15,
};

enum RegisterArm64 {
    REG_AARCH64_X0 = 0,
    REG_AARCH64_X1,
    REG_AARCH64_X2,
    REG_AARCH64_X3,
    REG_AARCH64_X4,
    REG_AARCH64_X5,
    REG_AARCH64_X6,
    REG_AARCH64_X7,
    REG_AARCH64_X8,
    REG_AARCH64_X9,
    REG_AARCH64_X10,
    REG_AARCH64_X11,
    REG_AARCH64_X12,
    REG_AARCH64_X13,
    REG_AARCH64_X14,
    REG_AARCH64_X15,
    REG_AARCH64_X16,
    REG_AARCH64_X17,
    REG_AARCH64_X18,
    REG_AARCH64_X19,
    REG_AARCH64_X20,
    REG_AARCH64_X21,
    REG_AARCH64_X22,
    REG_AARCH64_X23,
    REG_AARCH64_X24,
    REG_AARCH64_X25,
    REG_AARCH64_X26,
    REG_AARCH64_X27,
    REG_AARCH64_X28,
    REG_AARCH64_X29,
    REG_AARCH64_X30,
    REG_AARCH64_X31,
    REG_AARCH64_PC,

    REG_AARCH64_SP = REG_AARCH64_X31,
    REG_AARCH64_LR = REG_AARCH64_X30,
    REG_AARCH64_FP = REG_AARCH64_X29,
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
