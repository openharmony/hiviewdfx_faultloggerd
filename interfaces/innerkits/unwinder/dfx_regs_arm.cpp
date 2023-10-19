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

#if defined(__arm__)
#include "dfx_regs.h"

#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_elf.h"
#include "string_printf.h"

#define ARM_NR_sigreturn 119
#define ARM_NR_rt_sigreturn 173
#define ARM_NR_OABI_SYSCALL_BASE 0x900000

/* ARM EABI sigreturn (the syscall number is loaded into r7) */
#define MOV_R7_SIGRETURN (0xe3a07000UL | ARM_NR_sigreturn)
#define MOV_R7_RT_SIGRETURN (0xe3a07000UL | ARM_NR_rt_sigreturn)

/* ARM OABI sigreturn (using SWI) */
#define ARM_SIGRETURN (0xef000000UL | ARM_NR_sigreturn | ARM_NR_OABI_SYSCALL_BASE)
#define ARM_RT_SIGRETURN (0xef000000UL | ARM_NR_rt_sigreturn | ARM_NR_OABI_SYSCALL_BASE)

/* Thumb sigreturn (two insns, syscall number is loaded into r7) */
#define THUMB_SIGRETURN (0xdf00UL << 16 | 0x2700 | ARM_NR_sigreturn)
#define THUMB_RT_SIGRETURN (0xdf00UL << 16 | 0x2700 | ARM_NR_rt_sigreturn)

/* Thumb2 sigreturn (mov.w r7, $SYS_ify(rt_sigreturn/sigreturn)) */
#define THUMB2_SIGRETURN (((0x0700 | ARM_NR_sigreturn) << 16) | 0xf04f)
#define THUMB2_RT_SIGRETURN (((0x0700 | ARM_NR_rt_sigreturn) << 16) | 0xf04f)

namespace OHOS {
namespace HiviewDFX {
void DfxRegsArm::SetFromUcontext(const ucontext_t& context)
{
    std::vector<uintptr_t> regs;
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r0));   // 0:r0
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r1));   // 1:r1
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r2));   // 2:r2
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r3));   // 3:r3
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r4));   // 4:r4
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r5));   // 5:r5
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r6));   // 6:r6
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r7));   // 7:r7
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r8));   // 8:r8
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r9));   // 9:r9
    regs.push_back(uintptr_t(context.uc_mcontext.arm_r10)); // 10:r10
    regs.push_back(uintptr_t(context.uc_mcontext.arm_fp));  // 11:fp
    regs.push_back(uintptr_t(context.uc_mcontext.arm_ip));  // 12:ip
    regs.push_back(uintptr_t(context.uc_mcontext.arm_sp));  // 13:sp
    regs.push_back(uintptr_t(context.uc_mcontext.arm_lr));  // 14:lr
    regs.push_back(uintptr_t(context.uc_mcontext.arm_pc));  // 15:pc

    SetRegsData(regs);
}

void DfxRegsArm::SetFromFpMiniRegs(const uintptr_t* regs)
{
    regsData_[REG_ARM_R7] = regs[0];
    regsData_[REG_ARM_R11] = regs[1];
    regsData_[REG_SP] = regs[2]; // 2 : sp offset
    regsData_[REG_PC] = regs[3]; // 3 : pc offset
}

void DfxRegsArm::SetFromQutMiniRegs(const uintptr_t* regs)
{
    regsData_[REG_ARM_R4] = regs[0]; // 0 : r4 offset
    regsData_[REG_ARM_R7] = regs[1]; // 1 : r7 offset
    regsData_[REG_ARM_R10] = regs[2]; // 2 : r10 offset
    regsData_[REG_ARM_R11] = regs[3]; // 3 : r11 offset
    regsData_[REG_SP] = regs[4];  // 4 : sp offset
    regsData_[REG_PC] = regs[5];  // 5 : pc offset
    regsData_[REG_LR] = regs[6];  // 6 : lr offset
}

std::string DfxRegsArm::PrintRegs() const
{
    char buf[REGS_PRINT_LEN] = {0};
    auto regs = GetRegsData();

    BufferPrintf(buf, sizeof(buf), "r0:%08x r1:%08x r2:%08x r3:%08x\n", \
        regs[REG_ARM_R0], regs[REG_ARM_R1], regs[REG_ARM_R2], regs[REG_ARM_R3]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r4:%08x r5:%08x r6:%08x r7:%08x\n", \
        regs[REG_ARM_R4], regs[REG_ARM_R5], regs[REG_ARM_R6], regs[REG_ARM_R7]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "r8:%08x r9:%08x r10:%08x\n", \
        regs[REG_ARM_R8], regs[REG_ARM_R9], regs[REG_ARM_R10]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "fp:%08x ip:%08x sp:%08x lr:%08x pc:%08x\n", \
        regs[REG_ARM_R11], regs[REG_ARM_R12], regs[REG_ARM_R13], regs[REG_ARM_R14], regs[REG_ARM_R15]);

    std::string regString = StringPrintf("Registers:\n%s", buf);
    return regString;
}

bool DfxRegsArm::StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory)
{
    // The least bit denotes thumb/arm mode. Do not read there.
    pc = pc & ~0x1;

    uint32_t data;
    if (!memory->Read(pc, &data, sizeof(data))) {
        return false;
    }
    LOGU("data: %lx", data);

    uintptr_t scAddr = 0;
    if (data == MOV_R7_SIGRETURN || data == ARM_SIGRETURN
        || data == THUMB_SIGRETURN || data == THUMB2_SIGRETURN) {
        uintptr_t spAddr = regsData_[REG_SP];
        // non-RT sigreturn call.
        // __restore:
        //
        // Form 1 (arm):
        // 0x77 0x70              mov r7, #0x77
        // 0xa0 0xe3              svc 0x00000000
        //
        // Form 2 (arm):
        // 0x77 0x00 0x90 0xef    svc 0x00900077
        //
        // Form 3 (thumb):
        // 0x77 0x27              movs r7, #77
        // 0x00 0xdf              svc 0
        if (!memory->Read(spAddr, &data, sizeof(data), false)) {
            return false;
        }
        if (data == 0x5ac3c35a) {
            // SP + uc_mcontext offset + r0 offset.
            scAddr = spAddr + 0x14 + 0xc;
        } else {
            // SP + r0 offset
            scAddr = spAddr + 0xc;
        }
    } else if (data == MOV_R7_RT_SIGRETURN || data == ARM_RT_SIGRETURN
        || data == THUMB_RT_SIGRETURN || data == THUMB2_RT_SIGRETURN) {
        uintptr_t spAddr = regsData_[REG_SP];
        // RT sigreturn call.
        // __restore_rt:
        //
        // Form 1 (arm):
        // 0xad 0x70      mov r7, #0xad
        // 0xa0 0xe3      svc 0x00000000
        //
        // Form 2 (arm):
        // 0xad 0x00 0x90 0xef    svc 0x009000ad
        //
        // Form 3 (thumb):
        // 0xad 0x27              movs r7, #ad
        // 0x00 0xdf              svc 0
        if (!memory->Read(spAddr, &data, sizeof(data), false)) {
            return false;
        }
        if (data == spAddr + 8) { // 8 : eight bytes offset
            // SP + 8 + sizeof(siginfo_t) + uc_mcontext_offset + r0 offset
            scAddr = spAddr + 8 + sizeof(siginfo_t) + 0x14 + 0xc; // 8 : eight bytes offset
        } else {
            // SP + sizeof(siginfo_t) + uc_mcontext_offset + r0 offset
            scAddr = spAddr + sizeof(siginfo_t) + 0x14 + 0xc;
        }
    }
    if (scAddr == 0) {
        return false;
    }
    if (!memory->Read(scAddr, regsData_.data(), sizeof(uint32_t) * REG_LAST, false)) {
        return false;
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
