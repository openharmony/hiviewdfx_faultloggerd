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
#ifndef QUT_DEFINE_H
#define QUT_DEFINE_H

#include <cinttypes>
#include <string>

namespace OHOS {
namespace HiviewDFX {
enum QutInstructionDefine : uint64_t {
    // Do not change this order!!!
    QUT_INSTRUCTION_R4_OFFSET = 0,      // r4_offset
    QUT_INSTRUCTION_R7_OFFSET = 1,      // r7_offset
    QUT_INSTRUCTION_R10_OFFSET = 2,     // r10_offset
    QUT_INSTRUCTION_R11_OFFSET = 3,     // r11_offset
    QUT_INSTRUCTION_SP_OFFSET = 4,      // sp_offset
    QUT_INSTRUCTION_LR_OFFSET = 5,      // lr_offset
    QUT_INSTRUCTION_PC_OFFSET = 6,      // pc_offset
    QUT_INSTRUCTION_X20_OFFSET = 7,     // x20_offset
    QUT_INSTRUCTION_X28_OFFSET = 8,     // x28_offset
    QUT_INSTRUCTION_X29_OFFSET = 9,     // x29_offset
    QUT_INSTRUCTION_VSP_OFFSET = 10,     // vsp_offset
    QUT_INSTRUCTION_VSP_SET_IMM = 11,    // vsp_set_imm
    QUT_INSTRUCTION_VSP_SET_BY_R7 = 12,  // vsp_set_by_r7
    QUT_INSTRUCTION_VSP_SET_BY_R11 = 13, // vsp_set_by_r11
    QUT_INSTRUCTION_VSP_SET_BY_X29 = 14, // vsp_set_by_x29
    QUT_INSTRUCTION_VSP_SET_BY_SP = 15,  // vsp_set_by_sp
    QUT_INSTRUCTION_VSP_SET_BY_JNI_SP = 16, // vsp_set_by_jni_sp
    QUT_INSTRUCTION_DEX_PC_SET = 17,        // dex_pc_set
    QUT_INSTRUCTION_DEX_PC_SET_FOR_SWITCH_INTERPRETER = 18,  // dex_pc_set for arm64 switch interpreter. Not implemented.
    QUT_END_OF_INS = 19,
    QUT_FIN = 20,
    QUT_NOP = 0xFF, // No meaning.
};

enum QutStatisticType : uint32_t {
    InstructionOp = 0,
    InstructionOpOverflowR7 = 1,
    InstructionOpOverflowR11 = 2,
    InstructionOpOverflowJNISP = 3,
    InstructionOpOverflowR4 = 4,
    InstructionOpImmNotAligned = 5,
    IgnoreUnsupportedArmExidx = 10,
    IgnoreInstructionOpOverflowX29 = 11,
    IgnoreInstructionOpOverflowX20 = 12,
    InstructionEntriesArmExidx = 20,
    InstructionEntriesEhFrame = 21,
    InstructionEntriesDebugFrame = 22,
    IgnoreUnsupportedDwarfLocation = 30,
    IgnoreUnsupportedDwarfLocationOffset = 31,
    IgnoreUnsupportedCfaDwarfLocationRegister = 32,
    UnsupportedDwarfOp_OpBreg_Reg = 33,
    UnsupportedDwarfOp_OpBregx_Reg = 34,
    UnsupportedDwarfLocationValOffset = 35,
    UnsupportedDwarfLocationRegister = 36,
    UnsupportedDwarfLocationExpression = 37,
    UnsupportedDwarfLocationValExpressionWithoutDexPc = 38,
    UnsupportedCfaDwarfLocationValExpression = 39,
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
