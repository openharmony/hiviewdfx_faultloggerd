/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "dfx_regs.h"

#include <cstdio>
#include <cstdlib>
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_printf.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
std::shared_ptr<DfxRegs> DfxRegs::Create(int mode)
{
    std::shared_ptr<DfxRegs> dfxregs;
#if defined(__arm__)
    dfxregs = std::make_shared<DfxRegsArm>();
#elif defined(__aarch64__)
    dfxregs = std::make_shared<DfxRegsArm64>();
#elif defined(__x86_64__)
    dfxregs = std::make_shared<DfxRegsX86_64>();
#else
#error "Unsupported architecture"
#endif
    if (mode == UnwindMode::FRAMEPOINTER_UNWIND) {
        uintptr_t regs[FP_MINI_REGS_SIZE] = {0};
        dfxregs->GetFramePointerMiniRegs(regs);
        dfxregs->fp_ = regs[0]; // 0 : index of x29 or r11 register
        dfxregs->pc_ = regs[3]; // 3 : index of x32 or r15 register
    } else if (mode == UnwindMode::QUICKEN_UNWIND) {
        uintptr_t regs[QUT_MINI_REGS_SIZE] = {0};
        dfxregs->GetQuickenMiniRegs(regs);
        dfxregs->fp_ = regs[3]; // 3 : index of x29 or r11 register
        dfxregs->sp_ = regs[4]; // 4 : index of x31 or r13 register
        dfxregs->pc_ = regs[5]; // 5 : index of x32 or r15 register
        dfxregs->lr_ = regs[6]; // 6 : index of x30 or r14 register
    }
    return dfxregs;
}

std::shared_ptr<DfxRegs> DfxRegs::CreateFromContext(const ucontext_t &context)
{
    std::shared_ptr<DfxRegs> dfxregs;
#if defined(__arm__)
    dfxregs = std::make_shared<DfxRegsArm>(context);
#elif defined(__aarch64__)
    dfxregs = std::make_shared<DfxRegsArm64>(context);
#elif defined(__x86_64__)
    dfxregs = std::make_shared<DfxRegsX86_64>(context);
#else
#error "Unsupported architecture"
#endif
    return dfxregs;
}

std::vector<uintptr_t> DfxRegs::GetRegsData() const
{
    return regsData_;
}

void DfxRegs::SetRegsData(const std::vector<uintptr_t>& regs)
{
    regsData_ = regs;
#if defined(__arm__)
    fp_ = regs[REG_ARM_R11];
    sp_ = regs[REG_ARM_R13];
    lr_ = regs[REG_ARM_R14];
    pc_ = regs[REG_ARM_R15];
#elif defined(__aarch64__)
    fp_ = regs[REG_AARCH64_FP];
    sp_ = regs[REG_AARCH64_SP];
    lr_ = regs[REG_AARCH64_X30];
    pc_ = regs[REG_AARCH64_PC];
#elif defined(__x86_64__)
    sp_ = regs[REG_X86_64_SP];
    pc_ = regs[REG_X86_64_PC];
#else
#error "Unsupported architecture"
#endif
    DFXLOG_INFO("%s", PrintSpecialRegs().c_str());
}

std::string DfxRegs::GetSpecialRegisterName(uintptr_t val) const
{
    if (val == pc_) {
        return "pc";
    } else if (val == lr_) {
        return "lr";
    } else if (val == sp_) {
        return "sp";
    } else if (val == fp_) {
        return "fp";
    }
    return "";
}

std::string DfxRegs::PrintSpecialRegs() const
{
    std::string regsStr;
#ifdef __LP64__
    regsStr = StringPrintf("fp:%016lx sp:%016lx lr:%016lx pc:%016lx\n", fp_, sp_, lr_, pc_);
#else
    regsStr = StringPrintf("fp:%08x sp:%08x lr:%08x pc:%08x\n", fp_, sp_, lr_, pc_);
#endif
    return regsStr;
}
} // namespace HiviewDFX
} // namespace OHOS
