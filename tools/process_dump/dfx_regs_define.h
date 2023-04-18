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

enum RegisterArm : uint16_t {
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

    REG_ARM_SP = REG_ARM_R13,
    REG_ARM_LR = REG_ARM_R14,
    REG_ARM_PC = REG_ARM_R15,
};

enum RegisterArm64 : uint16_t {
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
    REG_AARCH64_PSTATE,
    REG_AARCH64_LAST,

    REG_AARCH64_SP = REG_AARCH64_X31,
    REG_AARCH64_LR = REG_AARCH64_X30,
    REG_AARCH64_FP = REG_AARCH64_X29,
};

enum RegisterX86_64 : uint16_t {
  REG_X86_64_RAX = 0,
  REG_X86_64_RDX = 1,
  REG_X86_64_RCX = 2,
  REG_X86_64_RBX = 3,
  REG_X86_64_RSI = 4,
  REG_X86_64_RDI = 5,
  REG_X86_64_RBP = 6,
  REG_X86_64_RSP = 7,
  REG_X86_64_R8 = 8,
  REG_X86_64_R9 = 9,
  REG_X86_64_R10 = 10,
  REG_X86_64_R11 = 11,
  REG_X86_64_R12 = 12,
  REG_X86_64_R13 = 13,
  REG_X86_64_R14 = 14,
  REG_X86_64_R15 = 15,
  REG_X86_64_RIP = 16,
  REG_X86_64_LAST,

  REG_X86_64_SP = REG_X86_64_RSP,
  REG_X86_64_PC = REG_X86_64_RIP,
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
