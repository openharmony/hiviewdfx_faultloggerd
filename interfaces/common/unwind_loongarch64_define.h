/*
 * Copyright 2024 Institute of Software, Chinese Academy of Sciences.
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
#ifndef UNWIND_LOONGARCH64_DEFINE_H
#define UNWIND_LOONGARCH64_DEFINE_H

#include <cinttypes>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 1024
#define DWARF_PRESERVED_REGS_NUM 97

enum RegsEnumLoongArch64 : uint16_t {
    REG_LOONGARCH64_R0 = 0,
    REG_LOONGARCH64_R1,
    REG_LOONGARCH64_R2,
    REG_LOONGARCH64_R3,
    REG_LOONGARCH64_R4,
    REG_LOONGARCH64_R5,
    REG_LOONGARCH64_R6,
    REG_LOONGARCH64_R7,
    REG_LOONGARCH64_R8,
    REG_LOONGARCH64_R9,
    REG_LOONGARCH64_R10,
    REG_LOONGARCH64_R11,
    REG_LOONGARCH64_R12,
    REG_LOONGARCH64_R13,
    REG_LOONGARCH64_R14,
    REG_LOONGARCH64_R15,
    REG_LOONGARCH64_R16,
    REG_LOONGARCH64_R17,
    REG_LOONGARCH64_R18,
    REG_LOONGARCH64_R19,
    REG_LOONGARCH64_R20,
    REG_LOONGARCH64_R21,
    REG_LOONGARCH64_R22,
    REG_LOONGARCH64_R23,
    REG_LOONGARCH64_R24,
    REG_LOONGARCH64_R25,
    REG_LOONGARCH64_R26,
    REG_LOONGARCH64_R27,
    REG_LOONGARCH64_R28,
    REG_LOONGARCH64_R29,
    REG_LOONGARCH64_R30,
    REG_LOONGARCH64_R31,
    REG_LOONGARCH64_PC,
    REG_LOONGARCH64_LAST,

    REG_SP = REG_LOONGARCH64_R3,
    REG_LR = REG_LOONGARCH64_R1,
    REG_FP = REG_LOONGARCH64_R22,
    REG_PC = REG_LOONGARCH64_PC,
    REG_EH = REG_LOONGARCH64_R0,
    REG_LAST = REG_LOONGARCH64_LAST,
};

static const std::vector<uint16_t> QUT_REGS {
    REG_LOONGARCH64_R0,
    REG_LOONGARCH64_R31,
    REG_SP,
    REG_PC,
    REG_LR,
};

struct RegsUserLoongArch64 {
    uint64_t regs[31]; // 31
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

typedef struct UnwindUContext {
    RegsUserLoongArch64 userRegs;
} UnwindUContext_t;
} // namespace HiviewDFX
} // namespace OHOS
#endif
