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

#if defined(__aarch64__)
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
void DfxRegsArm64::SetFromUcontext(const ucontext_t &context)
{
    std::vector<uintptr_t> regs(REG_LAST);
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X0]));   // 0:x0
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X1]));   // 1:x1
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X2]));   // 2:x2
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X3]));   // 3:x3
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X4]));   // 4:x4
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X5]));   // 5:x5
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X6]));   // 6:x6
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X7]));   // 7:x7
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X8]));   // 8:x8
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X9]));   // 9:x9
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X10])); // 10:x10
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X11])); // 11:x11
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X12])); // 12:x12
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X13])); // 13:x13
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X14])); // 14:x14
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X15])); // 15:x15
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X16])); // 16:x16
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X17])); // 17:x17
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X18])); // 18:x18
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X19])); // 19:x19
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X20])); // 20:x20
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X21])); // 21:x21
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X22])); // 22:x22
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X23])); // 23:x23
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X24])); // 24:x24
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X25])); // 25:x25
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X26])); // 26:x26
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X27])); // 27:x27
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X28])); // 28:x28
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X29])); // 29:x29
    regs.push_back(uintptr_t(context.uc_mcontext.regs[REG_AARCH64_X30])); // 30:lr
    regs.push_back(uintptr_t(context.uc_mcontext.sp));       // 31:sp
    regs.push_back(uintptr_t(context.uc_mcontext.pc));       // 32:pc

    SetRegsData(regs);
}

void DfxRegsArm64::SetFromFpMiniRegs(const uintptr_t* regs)
{
    regsData_[REG_FP] = regs[0]; // 0 : fp offset
    regsData_[REG_LR] = regs[1]; // 1 : lr offset
    regsData_[REG_SP] = regs[2]; // 2 : sp offset
    regsData_[REG_PC] = regs[3]; // 3 : pc offset
}

void DfxRegsArm64::SetFromQutMiniRegs(const uintptr_t* regs)
{
    regsData_[REG_AARCH64_X28] = regs[2]; // 2 : REG_AARCH64_X28 offset
    regsData_[REG_FP] = regs[3]; // 3 : fp offset
    regsData_[REG_SP] = regs[4];  // 4 : sp offset
    regsData_[REG_PC] = regs[5];  // 5 : pc offset
}

std::string DfxRegsArm64::PrintRegs() const
{
    char buf[REGS_PRINT_LEN] = {0};
    auto regs = GetRegsData();

    BufferPrintf(buf, sizeof(buf), "x0:%016lx x1:%016lx x2:%016lx x3:%016lx\n", \
        regs[REG_AARCH64_X0], regs[REG_AARCH64_X1], regs[REG_AARCH64_X2], regs[REG_AARCH64_X3]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "x4:%016lx x5:%016lx x6:%016lx x7:%016lx\n", \
        regs[REG_AARCH64_X4], regs[REG_AARCH64_X5], regs[REG_AARCH64_X6], regs[REG_AARCH64_X7]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "x8:%016lx x9:%016lx x10:%016lx x11:%016lx\n", \
        regs[REG_AARCH64_X8], regs[REG_AARCH64_X9], regs[REG_AARCH64_X10], regs[REG_AARCH64_X11]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "x12:%016lx x13:%016lx x14:%016lx x15:%016lx\n", \
        regs[REG_AARCH64_X12], regs[REG_AARCH64_X13], regs[REG_AARCH64_X14], regs[REG_AARCH64_X15]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "x16:%016lx x17:%016lx x18:%016lx x19:%016lx\n", \
        regs[REG_AARCH64_X16], regs[REG_AARCH64_X17], regs[REG_AARCH64_X18], regs[REG_AARCH64_X19]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "x20:%016lx x21:%016lx x22:%016lx x23:%016lx\n", \
        regs[REG_AARCH64_X20], regs[REG_AARCH64_X21], regs[REG_AARCH64_X22], regs[REG_AARCH64_X23]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "x24:%016lx x25:%016lx x26:%016lx x27:%016lx\n", \
        regs[REG_AARCH64_X24], regs[REG_AARCH64_X25], regs[REG_AARCH64_X26], regs[REG_AARCH64_X27]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "x28:%016lx x29:%016lx\n", \
        regs[REG_AARCH64_X28], regs[REG_AARCH64_X29]);

    BufferPrintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "lr:%016lx sp:%016lx pc:%016lx\n", \
        regs[REG_AARCH64_X30], regs[REG_SP], regs[REG_PC]);

    std::string regString = StringPrintf("Registers:\n%s", buf);
    return regString;
}

bool DfxRegsArm64::StepIfSignalHandler(uint64_t relPc, DfxElf* elf, DfxMemory* memory)
{
    if (elf == nullptr || !elf->IsValid() || (relPc < static_cast<uint64_t>(elf->GetLoadBias()))) {
        return false;
    }
    uint64_t elfOffset = relPc - elf->GetLoadBias();
    uint64_t data;
    if (!elf->Read(elfOffset, &data, sizeof(data))) {
        return false;
    }

    // Look for the kernel sigreturn function.
    // __kernel_rt_sigreturn:
    // 0xd2801168     mov x8, #0x8b
    // 0xd4000001     svc #0x0
    if (data != 0xd4000001d2801168ULL) {
        return false;
    }

    // SP + sizeof(siginfo_t) + uc_mcontext offset + X0 offset.
    uint64_t offset = regsData_[REG_SP] + 0x80 + 0xb0 + 0x08;
    if (!memory->Read(offset, regsData_.data(), sizeof(uint64_t) * REG_LAST, false)) {
        return false;
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
#endif
