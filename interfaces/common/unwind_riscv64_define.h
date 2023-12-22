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

#ifndef UNWIND_RISCV64_DEFINE_H
#define UNWIND_RISCV64_DEFINE_H

#include <cinttypes>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 1024
#define DWARF_PRESERVED_REGS_NUM 97

#ifdef REG_SP
#undef REG_SP
#endif
#ifdef REG_PC
#undef REG_PC
#endif

enum RegsEnumRiscv64 : uint16_t {
    REG_RISCV64_X0 = 0,
    REG_RISCV64_X1,
    REG_RISCV64_X2,
    REG_RISCV64_X3,
    REG_RISCV64_X4,
    REG_RISCV64_X5,
    REG_RISCV64_X6,
    REG_RISCV64_X7,
    REG_RISCV64_X8,
    REG_RISCV64_X9,
    REG_RISCV64_X10,
    REG_RISCV64_X11,
    REG_RISCV64_X12,
    REG_RISCV64_X13,
    REG_RISCV64_X14,
    REG_RISCV64_X15,
    REG_RISCV64_X16,
    REG_RISCV64_X17,
    REG_RISCV64_X18,
    REG_RISCV64_X19,
    REG_RISCV64_X20,
    REG_RISCV64_X21,
    REG_RISCV64_X22,
    REG_RISCV64_X23,
    REG_RISCV64_X24,
    REG_RISCV64_X25,
    REG_RISCV64_X26,
    REG_RISCV64_X27,
    REG_RISCV64_X28,
    REG_RISCV64_X29,
    REG_RISCV64_X30,
    REG_RISCV64_X31,
    REG_RISCV64_PC,
    REG_RISCV64_LAST,

    REG_SP = REG_RISCV64_X2,
    REG_LR = REG_RISCV64_X0,
    REG_FP = REG_RISCV64_X8,
    REG_PC = REG_RISCV64_PC,
    REG_EH = REG_RISCV64_X0,
    REG_LAST = REG_RISCV64_LAST,
};

static const std::vector<uint16_t> QUT_REGS {
    REG_RISCV64_X0,
    REG_RISCV64_X31,
    REG_SP,
    REG_PC,
    REG_LR,
};

struct RegsUserRiscv64 {
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

typedef struct UnwindUContext {
    RegsUserRiscv64 userRegs;
} UnwindUContext_t;
} // namespace HiviewDFX
} // namespace OHOS
#endif
