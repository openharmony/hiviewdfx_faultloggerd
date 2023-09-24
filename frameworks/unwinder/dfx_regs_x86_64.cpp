/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#if defined(__x86_64__)
#include "dfx_regs.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include "dfx_define.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
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

    SetRegsData(regs);
}

std::string DfxRegsX86_64::PrintRegs() const
{
    char buf[REGS_PRINT_LEN] = {0};
    std::vector<uintptr_t> regs = GetRegsData();

    BufferPrintf(buf, sizeof(buf), "  rax:%016lx rdx:%016lx rcx:%016lx rbx:%016lx\n", \
        regs[REG_X86_64_RAX], regs[REG_X86_64_RDX], regs[REG_X86_64_RCX], regs[REG_X86_64_RBX]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  rsi:%016lx rdi:%016lx rbp:%016lx rsp:%016lx\n", \
        regs[REG_X86_64_RSI], regs[REG_X86_64_RDI], regs[REG_X86_64_RBP], regs[REG_X86_64_RSP]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "  r8:%016lx r9:%016lx r10:%016lx r11:%016lx\n", \
        regs[REG_X86_64_R8], regs[REG_X86_64_R9], regs[REG_X86_64_R10], regs[REG_X86_64_R11]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), \
        "  r12:%016lx r13:%016lx r14:%016lx r15:%016lx rip:%016lx\n", \
        regs[REG_X86_64_R12], regs[REG_X86_64_R13], regs[REG_X86_64_R14], regs[REG_X86_64_R15], regs[REG_X86_64_RIP]);

    std::string regString = StringPrintf("Registers:\n%s", buf);
    return regString;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
