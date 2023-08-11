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
#ifndef UNWINDER_ARM64_DEFINE_H
#define UNWINDER_ARM64_DEFINE_H

#include <cinttypes>
#include <string>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 1024
#define UNWIND_CURSOR_LEN 1024
#define DWARF_PRESERVED_REGS_NUM 97

enum RegsEnumArm64 : uint16_t {
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

    REG_SP = REG_AARCH64_X31,
    REG_LR = REG_AARCH64_X30,
    REG_FP = REG_AARCH64_X29,
    REG_PC = REG_AARCH64_PC,
    REG_EH = REG_AARCH64_X0,
    REG_LAST = REG_AARCH64_LAST,
};

struct RegsUserArm64 {
    uint64_t regs[31]; // 31
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

typedef struct UContext
{
    RegsUserArm64 userRegs;
} UContext_t;

struct UnwindFrameInfo {
    uint64_t virtualAddress;
    int64_t frameType     : 2;
    int64_t lastFrame     : 1;  /* non-zero if last frame in chain */
    int64_t cfaRegSp      : 1;  /* cfa dwarf base register is sp vs. fp */
    int64_t cfaRegOffset : 30; /* cfa is at this offset from base register value */
    int64_t r7CfaOffset  : 30; /* r7 saved at this offset from cfa (-1 = not saved) */
    int64_t lrCfaOffset  : 30; /* lr saved at this offset from cfa (-1 = not saved) */
    int64_t spCfaOffset  : 30; /* sp saved at this offset from cfa (-1 = not saved) */
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
