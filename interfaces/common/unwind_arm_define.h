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
#ifndef UNWINDER_ARM_DEFINE_H
#define UNWINDER_ARM_DEFINE_H

#include <cinttypes>
#include <string>

namespace OHOS {
namespace HiviewDFX {
#define REGS_PRINT_LEN 256
#define UNWIND_CURSOR_LEN 4096
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

struct RegsUserArm {
    uint32_t regs[16]; // 16
};

typedef struct UContext {
    RegsUserArm userRegs;
} UContext_t;

struct UnwindFrameInfo {
    uint32_t virtualAddress;
    int32_t frameType     : 3;
    int32_t lastFrame     : 1;  /* non-zero if last frame in chain */
    int32_t cfaRegSp      : 1;  /* cfa dwarf base register is sp vs. r7 */
    int32_t cfaRegOffset : 30; /* cfa is at this offset from base register value */
    int32_t r7CfaOffset  : 30; /* r7 saved at this offset from cfa (-1 = not saved) */
    int32_t lrCfaOffset  : 30; /* lr saved at this offset from cfa (-1 = not saved) */
    int32_t spCfaOffset  : 30; /* sp saved at this offset from cfa (-1 = not saved) */
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
