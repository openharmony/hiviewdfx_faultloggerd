/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include <cstdint>
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
    if (regsData_.size() < REG_LAST) {
        return;
    }
    regsData_[REG_X86_64_RAX] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RAX]);
    regsData_[REG_X86_64_RDX] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RDX]);
    regsData_[REG_X86_64_RCX] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RCX]);
    regsData_[REG_X86_64_RBX] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RBX]);
    regsData_[REG_X86_64_RSI] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RSI]);
    regsData_[REG_X86_64_RDI] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RDI]);
    regsData_[REG_X86_64_RBP] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RBP]);
    regsData_[REG_X86_64_RSP] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RSP]);
    regsData_[REG_X86_64_R8] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R8]);
    regsData_[REG_X86_64_R9] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R9]);
    regsData_[REG_X86_64_R10] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R10]);
    regsData_[REG_X86_64_R11] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R11]);
    regsData_[REG_X86_64_R12] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R12]);
    regsData_[REG_X86_64_R13] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R13]);
    regsData_[REG_X86_64_R14] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R14]);
    regsData_[REG_X86_64_R15] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_R15]);
    regsData_[REG_X86_64_RIP] = static_cast<uintptr_t>(context.uc_mcontext.gregs[REG_RIP]);
}

void DfxRegsX86_64::SetFromFpMiniRegs(const uintptr_t* regs, const size_t size)
{
}

void DfxRegsX86_64::SetFromQutMiniRegs(const uintptr_t* regs, const size_t size)
{
}

bool DfxRegsX86_64::SetPcFromReturnAddress(std::shared_ptr<DfxMemory> memory)
{
    uintptr_t newPc;
    if (!memory->Read<uintptr_t>(regsData_[REG_SP], &newPc, false) ||
        newPc == regsData_[REG_PC]) {
        return false;
    }
    regsData_[REG_PC] = newPc;
    return true;
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
    if (!memory->Read<uint64_t>(pc, &data, false)) {
        return false;
    }
    DFXLOGU("[%{public}d]: data: %{public}" PRIu64 "", __LINE__, data);

    // __restore_rt:
    // 0x48 0xc7 0xc0 0x0f 0x00 0x00 0x00   mov $0xf,%rax
    // 0x0f 0x05                            syscall
    // 0x0f                                 nopl 0x0($rax)
    if (data != 0x0f0000000fc0c748) {
        return false;
    }

    uint16_t data2;
    pc += sizeof(uint64_t);
    if (!memory->Read<uint16_t>(pc, &data2, false) || (data2 != 0x0f05)) {
        DFXLOGU("data2: %{public}x", data2);
        return false;
    }

    // Read the mcontext data from the stack.
    // sp points to the ucontext data structure, read only the mcontext part.
    ucontext_t ucontext;
    uintptr_t scAddr = regsData_[REG_SP] + 0x28;
    DFXLOGU("[%{public}d]: scAddr: %{public}" PRIu64 "", __LINE__, scAddr);
    memory->Read(scAddr, &ucontext.uc_mcontext, sizeof(ucontext.uc_mcontext), false);
    SetFromUcontext(ucontext);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
