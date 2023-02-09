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

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include "dfx_define.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
enum UnWindRegIndex {
    RAX = 0,
    RDX,
    RCX,
    RBX,
    RSI,
    RDI,
    RBP,
    RSP,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,
    RIP
};
std::string DfxRegsX86_64::GetSpecialRegisterName(uintptr_t val) const
{
    return "";
}

DfxRegsX86_64::DfxRegsX86_64(const ucontext_t &context)
{
    std::vector<uintptr_t> regs {};

    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RAX]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RDX]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RCX]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RBX]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RSI]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RDI]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RBP]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RSP]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R8]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R9]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R10]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R11]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R12]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R13]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R14]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_R15]));
    regs.push_back((uintptr_t)(context.uc_mcontext.gregs[REG_RIP]));

    SetRegs(regs);
}

std::string DfxRegsX86_64::PrintRegs() const
{
    std::string regString = "";
    char buf[REGS_PRINT_LEN_X86] = {0};

    regString = regString + "Registers:\n";

    std::vector<uintptr_t> regs = GetRegsData();

    PrintFormat(buf, sizeof(buf), "  rax:%016lx rdx:%016lx rcx:%016lx rbx:%016lx\n", \
                regs[RAX], regs[RDX], regs[RCX], regs[RBX]);

    PrintFormat(buf + strlen(buf), sizeof(buf) - strlen(buf), "  rsi:%016lx rdi:%016lx rbp:%016lx rsp:%016lx\n", \
                regs[RSI], regs[RDI], regs[RBP], regs[RSP]);

    PrintFormat(buf + strlen(buf), sizeof(buf) - strlen(buf), "  r8:%016lx r9:%016lx r10:%016lx r11:%016lx\n", \
                regs[R8], regs[R9], regs[R10], regs[R11]);

    PrintFormat(buf + strlen(buf), sizeof(buf) - strlen(buf), \
                "  r12:%016lx r13:%016lx r14:%016lx r15:%016lx rip:%016lx\n", \
                regs[R12], regs[R13], regs[R14], regs[R15], regs[RIP]);
    
    regString = regString + std::string(buf);
    return regString;
}

uintptr_t DfxRegsX86_64::GetPC() const
{
    return regsData_[RIP];
}

uintptr_t DfxRegsX86_64::GetLR() const
{
    return 0;
}
} // namespace HiviewDFX
} // namespace OHOS
