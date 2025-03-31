/*
 * Copyright 2024 Institute of Software, Chinese Academy of Sciences.
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

#if defined(__loongarch_lp64)
#include "dfx_regs.h"

#include <securec.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
void DfxRegsLoongArch64::SetFromUcontext(const ucontext_t &context)
{
    if (regsData_.size() < REG_LAST) {
        return;
    }
    for (uint16_t index = 0; index <= REG_LOONGARCH64_R31; index++) {
        regsData_[index] = static_cast<uintptr_t>(context.uc_mcontext.__gregs[index]);
    }
    regsData_[REG_LOONGARCH64_PC] = (static_cast<uintptr_t>(context.uc_mcontext.__pc)); // 32:pc
}

void DfxRegsLoongArch64::SetFromFpMiniRegs(const uintptr_t* regs, const size_t size)
{
    if (size < FP_MINI_REGS_SIZE) {
        return;
    }
    regsData_[REG_FP] = regs[0]; // 0 : fp offset
    regsData_[REG_LR] = regs[1]; // 1 : lr offset
    regsData_[REG_SP] = regs[2]; // 2 : sp offset
    regsData_[REG_PC] = regs[3]; // 3 : pc offset
}

void DfxRegsLoongArch64::SetFromQutMiniRegs(const uintptr_t* regs, const size_t size)
{
    if (size < QUT_MINI_REGS_SIZE) {
        return;
    }

    regsData_[REG_FP] = regs[3]; // 3 : fp offset
    regsData_[REG_SP] = regs[4];  // 4 : sp offset
    regsData_[REG_PC] = regs[5];  // 5 : pc offset
    regsData_[REG_LR] = regs[6];  // 6 : lr offset
}

bool DfxRegsLoongArch64::SetPcFromReturnAddress(MAYBE_UNUSED std::shared_ptr<DfxMemory> memory)
{
    uintptr_t lr = regsData_[REG_LR];
    if (regsData_[REG_PC] == lr) {
        return false;
    }
    regsData_[REG_PC] = lr;
    return true;
}

std::string DfxRegsLoongArch64::PrintRegs() const
{
    char buf[REGS_PRINT_LEN] = {0};
    std::vector<uintptr_t> regs = GetRegsData();

    BufferPrintf(buf, sizeof(buf), "r0:%016lx r1:%016lx r2:%016lx r3:%016lx\n", \
        regs[REG_LOONGARCH64_R0], regs[REG_LOONGARCH64_R1], regs[REG_LOONGARCH64_R2], regs[REG_LOONGARCH64_R3]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r4:%016lx r5:%016lx r6:%016lx r7:%016lx\n", \
        regs[REG_LOONGARCH64_R4], regs[REG_LOONGARCH64_R5], regs[REG_LOONGARCH64_R6], regs[REG_LOONGARCH64_R7]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r8:%016lx r9:%016lx r10:%016lx r11:%016lx\n", \
        regs[REG_LOONGARCH64_R8], regs[REG_LOONGARCH64_R9], regs[REG_LOONGARCH64_R10], regs[REG_LOONGARCH64_R11]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r12:%016lx r13:%016lx r14:%016lx r15:%016lx\n", \
        regs[REG_LOONGARCH64_R12], regs[REG_LOONGARCH64_R13], regs[REG_LOONGARCH64_R14], regs[REG_LOONGARCH64_R15]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r16:%016lx r17:%016lx r18:%016lx r19:%016lx\n", \
        regs[REG_LOONGARCH64_R16], regs[REG_LOONGARCH64_R17], regs[REG_LOONGARCH64_R18], regs[REG_LOONGARCH64_R19]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r20:%016lx r21:%016lx x22:%016lx r23:%016lx\n", \
        regs[REG_LOONGARCH64_R20], regs[REG_LOONGARCH64_R21], regs[REG_LOONGARCH64_R22], regs[REG_LOONGARCH64_R23]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r24:%016lx r25:%016lx r26:%016lx r27:%016lx\n", \
        regs[REG_LOONGARCH64_R24], regs[REG_LOONGARCH64_R25], regs[REG_LOONGARCH64_R26], regs[REG_LOONGARCH64_R27]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r28:%016lx r29:%016lx r30:%016lx r31:%016lx\n", \
        regs[REG_LOONGARCH64_R28], regs[REG_LOONGARCH64_R29], regs[REG_LOONGARCH64_R30], regs[REG_LOONGARCH64_R31]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "lr:%016lx sp:%016lx pc:%016lx\n", \
        regs[REG_LR], regs[REG_SP], regs[REG_PC]);

    std::string regString = StringPrintf("Registers:\n%s", buf);
    return regString;
}

bool DfxRegsLoongArch64::StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory)
{
    uint64_t data;
    if (!memory->Read<uint64_t>(pc, &data, false)) {
        return false;
    }
    DFXLOGU("data: %llx", data);

    // Look for the kernel sigreturn function.
    // arch/loongarch/vdso/sigreturn.S:
    // 0000000000000000 <__vdso_rt_sigreturn>:
    // 0: 03822c0b        ori     $r11,$r0,0x8b
    // 4: 002b0000        syscall 0x0

    if (data != 0x002b000003822c0bULL) {
        return false;
    }

    // SP + sizeof(siginfo_t) + mcontext_gregs offset.
    uintptr_t scAddr = regsData_[REG_SP] + sizeof(siginfo_t) + 0xb8;
    DFXLOGU("scAddr: %llx", scAddr);
    memory->Read(scAddr, regsData_.data(), sizeof(uint64_t) * REG_LAST, false);
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
