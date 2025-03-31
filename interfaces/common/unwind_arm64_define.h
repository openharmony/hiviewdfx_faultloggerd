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
#ifndef UNWIND_ARM64_DEFINE_H
#define UNWIND_ARM64_DEFINE_H

#include <cinttypes>
#include <string>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 1024
#define DWARF_PRESERVED_REGS_NUM 97

enum class RegsEnumArm64 : uint16_t {
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
    REG_AARCH64_LAST,

    REG_SP = REG_AARCH64_X31,
    REG_LR = REG_AARCH64_X30,
    REG_FP = REG_AARCH64_X29,
    REG_PC = REG_AARCH64_PC,
    REG_EH = REG_AARCH64_X0,
    REG_LAST = REG_AARCH64_LAST,
};

static const std::vector<uint16_t> QUT_REGS {
    REG_AARCH64_X0,
    REG_AARCH64_X29,
    REG_SP,
    REG_PC,
    REG_LR,
};

struct RegsUserArm64 {
    uint64_t regs[31]; // 31
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

typedef struct UnwindUContext {
    RegsUserArm64 userRegs;
} UnwindUContext_t;
} // namespace HiviewDFX
} // namespace OHOS
#endif
