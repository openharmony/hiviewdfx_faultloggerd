/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains process dump x86 64 regs module. */

#include "dfx_regs.h"

#include <cstdio>
#include <cstdlib>

#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
enum RegisterSeqNum {
    REGISTER_ZERO = 0,
    REGISTER_ONE,
    REGISTER_TWO,
    REGISTER_THREE,
    REGISTER_FOUR,
    REGISTER_FIVE,
    REGISTER_SIX,
    REGISTER_SEVEN,
    REGISTER_EIGHT,
    REGISTER_NINE,
    REGISTER_TEN,
    REGISTER_ELEVEN,
    REGISTER_TWELVE,
    REGISTER_THIRTEEN,
    REGISTER_FOURTEEN,
    REGISTER_FIFTEEN,
    REGISTER_SIXTEEN
};


DfxRegsX86_64::DfxRegsX86_64(const ucontext_t &context)
{
    DfxLogDebug("Enter %s.", __func__);
    std::vector<uintptr_t> regs {};

    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_ZERO]));   // 0:rax
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_ONE]));   // 1:rdx
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_TWO]));   // 2:rcx
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_THREE]));   // 3:rbx
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_FOUR]));   // 4:rsi
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_FIVE]));   // 5:rdi
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_SIX]));   // 6:rbp
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_SEVEN]));   // 7:rsp
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_EIGHT]));   // 8:r8
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_NINE]));   // 9:r9
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_TEN])); // 10:r10
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_ELEVEN])); // 11:r11
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_TWELVE])); // 12:r12
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_THIRTEEN])); // 13:r13
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_FOURTEEN])); // 14:r14
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_FIFTEEN])); // 15:r15
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REGISTER_SIXTEEN])); // 16:rip

    SetRegs(regs);
    DfxLogDebug("Exit %s.", __func__);
}

void DfxRegsX86_64::PrintRegs(int32_t fd) const
{
    DfxLogDebug("Enter %s.", __func__);
    if (fd < 0) {
        return;
    }
    WriteLog(fd, "Registers:\n");
    std::vector<uintptr_t> regs = GetRegsData();

    dprintf(fd, "  rax:%016lx rdx:%016lx rcx:%016lx rbx:%016lx\n", regs[REGISTER_ZERO], regs[REGISTER_ONE],
        regs[REGISTER_TWO], regs[REGISTER_THREE]);
    dprintf(fd, "  rsi:%016lx rdi:%016lx rbp:%016lx rsp:%016lx\n", regs[REGISTER_FOUR], regs[REGISTER_FIVE],
        regs[REGISTER_SIX], regs[REGISTER_SEVEN]);
    dprintf(fd, "  r8:%016lx r9:%016lx r10:%016lx r11:%016lx\n", regs[REGISTER_EIGHT], regs[REGISTER_NINE],
        regs[REGISTER_TEN], regs[REGISTER_ELEVEN]);
    dprintf(fd, "  r12:%016lx r13:%016lx r14:%016lx r15:%016lx rip:%016lx \n", regs[REGISTER_TWELVE],
        regs[REGISTER_THIRTEEN], regs[REGISTER_FOURTEEN], regs[REGISTER_FIFTEEN], regs[REGISTER_SIXTEEN]);
    DfxLogDebug("Exit %s.", __func__);
}
} // namespace HiviewDFX
} // namespace OHOS
