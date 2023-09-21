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

#include <elf.h>
#include <securec.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_elf.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
void DfxRegsX86_64::SetFromUcontext(const ucontext_t &context)
{
    std::vector<uintptr_t> regs;
    regs[REG_X86_64_RAX] = context.uc_mcontext.gregs[REG_RAX];
    regs[REG_X86_64_RDX] = context.uc_mcontext.gregs[REG_RDX];
    regs[REG_X86_64_RCX] = context.uc_mcontext.gregs[REG_RCX];
    regs[REG_X86_64_RBX] = context.uc_mcontext.gregs[REG_RBX];
    regs[REG_X86_64_RSI] = context.uc_mcontext.gregs[REG_RSI];
    regs[REG_X86_64_RDI] = context.uc_mcontext.gregs[REG_RDI];
    regs[REG_X86_64_RBP] = context.uc_mcontext.gregs[REG_RBP];
    regs[REG_X86_64_RSP] = context.uc_mcontext.gregs[REG_RSP];
    regs[REG_X86_64_R8] = context.uc_mcontext.gregs[REG_R8];
    regs[REG_X86_64_R9] = context.uc_mcontext.gregs[REG_R9];
    regs[REG_X86_64_R10] = context.uc_mcontext.gregs[REG_R10];
    regs[REG_X86_64_R11] = context.uc_mcontext.gregs[REG_R11];
    regs[REG_X86_64_R12] = context.uc_mcontext.gregs[REG_R12];
    regs[REG_X86_64_R13] = context.uc_mcontext.gregs[REG_R13];
    regs[REG_X86_64_R14] = context.uc_mcontext.gregs[REG_R14];
    regs[REG_X86_64_R15] = context.uc_mcontext.gregs[REG_R15];
    regs[REG_X86_64_RIP] = context.uc_mcontext.gregs[REG_RIP];

    SetRegsData(regs);
}

void DfxRegsX86_64::SetFromFpMiniRegs(const uintptr_t* regs)
{
    // TODO
}

void DfxRegsX86_64::SetFromQutMiniRegs(const uintptr_t* regs)
{
    // TODO
}

std::string DfxRegsX86_64::PrintRegs() const
{
    char buf[REGS_PRINT_LEN] = {0};
    auto regs = GetRegsData();

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

bool DfxRegsX86_64::StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory)
{
    uint64_t data;
    if (!DfxMemoryCpy::GetInstance().Read(pc, &data, sizeof(data))) {
        return false;
    }
    LOGU("data: %llx", data);

    // __restore_rt:
    // 0x48 0xc7 0xc0 0x0f 0x00 0x00 0x00   mov $0xf,%rax
    // 0x0f 0x05                            syscall
    // 0x0f                                 nopl 0x0($rax)
    if (data != 0x0f0000000fc0c748) {
        return false;
    }

    uint16_t data2;
    if (!DfxMemoryCpy::GetInstance().Read(pc + sizeof(uint64_t), &data2, sizeof(data2))) {
        return false;
    }
    if (data2 != 0x0f05) {
        return false;
    }

    // Read the mcontext data from the stack.
    // sp points to the ucontext data structure, read only the mcontext part.
    ucontext_t ucontext;
    uintptr_t scAddr = regsData_[REG_SP] + 0x28;
    if (!memory->Read(scAddr, &ucontext.uc_mcontext, sizeof(ucontext), false)) {
        return false;
    }
    SetFromUcontext(ucontext);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
