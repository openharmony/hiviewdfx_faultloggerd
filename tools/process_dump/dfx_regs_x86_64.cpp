/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <securec.h>
#include "dfx_define.h"

namespace OHOS {
namespace HiviewDFX {
enum RegisterSeqNum {
    REG_X86_64_R0 = 0,
    REG_X86_64_R1,
    REG_X86_64_R2,
    REG_X86_64_R3,
    REG_X86_64_R4,
    REG_X86_64_R5,
    REG_X86_64_R6,
    REG_X86_64_R7,
    REG_X86_64_R8,
    REG_X86_64_R9,
    REG_X86_64_R10,
    REG_X86_64_R11,
    REG_X86_64_R12,
    REG_X86_64_R13,
    REG_X86_64_R14,
    REG_X86_64_R15,
    REG_X86_64_R16
};

std::string DfxRegsX86_64::GetSpecialRegisterName(uintptr_t val) const
{
    return "";
}

DfxRegsX86_64::DfxRegsX86_64(const ucontext_t &context)
{
    std::vector<uintptr_t> regs {};

    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R0]));   // 0:rax
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R1]));   // 1:rdx
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R2]));   // 2:rcx
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R3]));   // 3:rbx
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R4]));   // 4:rsi
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R5]));   // 5:rdi
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R6]));   // 6:rbp
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R7]));   // 7:rsp
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R8]));   // 8:r8
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R9]));   // 9:r9
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R10])); // 10:r10
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R11])); // 11:r11
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R12])); // 12:r12
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R13])); // 13:r13
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R14])); // 14:r14
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R15])); // 15:r15
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_X86_64_R16])); // 16:rip

    SetRegs(regs);
}

std::string DfxRegsX86_64::PrintRegs() const
{
    std::string regString = "";
    char buf[REGS_PRINT_LEN_X86] = {0};

    regString = regString + "Registers:\n";

    std::vector<uintptr_t> regs = GetRegsData();

    PrintFormat(buf, sizeof(buf), "  rax:%016lx rdx:%016lx rcx:%016lx rbx:%016lx\n", \
                regs[REG_X86_64_R0], regs[REG_X86_64_R1], regs[REG_X86_64_R2], regs[REG_X86_64_R3]);

    PrintFormat(buf + strlen(buf), sizeof(buf) - strlen(buf), "  rsi:%016lx rdi:%016lx rbp:%016lx rsp:%016lx\n", \
                regs[REG_X86_64_R4], regs[REG_X86_64_R5], regs[REG_X86_64_R6], regs[REG_X86_64_R7]);

    PrintFormat(buf + strlen(buf), sizeof(buf) - strlen(buf), "  r8:%016lx r9:%016lx r10:%016lx r11:%016lx\n", \
                regs[REG_X86_64_R8], regs[REG_X86_64_R9], regs[REG_X86_64_R10], regs[REG_X86_64_R11]);

    PrintFormat(buf + strlen(buf), sizeof(buf) - strlen(buf), \
                "  r12:%016lx r13:%016lx r14:%016lx r15:%016lx rip:%016lx\n", \
                regs[REG_X86_64_R12], regs[REG_X86_64_R13], regs[REG_X86_64_R14], \
                regs[REG_X86_64_R15], regs[REG_X86_64_R16]);
    
    regString = regString + std::string(buf);
    return regString;
}
} // namespace HiviewDFX
} // namespace OHOS
