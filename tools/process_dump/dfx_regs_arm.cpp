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

/* This files contains process dump arm reg module. */

#include "dfx_regs.h"

#include <cstdio>
#include <cstdlib>

#include "dfx_define.h"
#include "dfx_log.h"

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
    REGISTER_FIFTEEN
};

DfxRegsArm::DfxRegsArm(const ucontext_t& context)
{
    DfxLogDebug("Enter %s.", __func__);
    std::vector<uintptr_t> regs {};

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

    SetRegs(regs);
    DfxLogDebug("fp:%08x ip:%08x sp:%08x lr:%08x pc:%08x \n", regs[REGISTER_ELEVEN], regs[REGISTER_TWELVE],
        regs[REGISTER_THIRTEEN], regs[REGISTER_FOURTEEN], regs[REGISTER_FIFTEEN]);
    DfxLogDebug("Exit %s.", __func__);
}

void DfxRegsArm::PrintRegs(int32_t fd) const
{
    DfxLogDebug("Enter %s.", __func__);
    if (fd < 0) {
        return;
    }
    WriteLog(fd, "Registers:\n");
    std::vector<uintptr_t> regs = GetRegsData();
    WriteLog(fd, "r0:%08x r1:%08x r2:%08x r3:%08x\n", regs[REGISTER_ZERO], regs[REGISTER_ONE], regs[REGISTER_TWO],
        regs[REGISTER_THREE]);
    WriteLog(fd, "r4:%08x r5:%08x r6:%08x r7:%08x\n", regs[REGISTER_FOUR], regs[REGISTER_FIVE], regs[REGISTER_SIX],
        regs[REGISTER_SEVEN]);
    WriteLog(fd, "r8:%08x r9:%08x r10:%08x\n", regs[REGISTER_EIGHT], regs[REGISTER_NINE], regs[REGISTER_TEN]);
    WriteLog(fd, "fp:%08x ip:%08x sp:%08x lr:%08x pc:%08x \n", regs[REGISTER_ELEVEN], regs[REGISTER_TWELVE],
        regs[REGISTER_THIRTEEN], regs[REGISTER_FOURTEEN], regs[REGISTER_FIFTEEN]);
    DfxLogDebug("Exit %s.", __func__);
}
} // namespace HiviewDFX
} // namespace OHOS
