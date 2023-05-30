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

/* This files contains process dump arm reg module. */

#include "dfx_regs.h"

#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_regs_define.h"
#include "unwinder_define.h"

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
    if (mode == UnwinderMode::FRAMEPOINTER_UNWIND) {
        uintptr_t regs[FP_MINI_REGS_SIZE] = {0};
        dfxregs->GetFramePointerMiniRegs(regs);
        dfxregs->fp_ = regs[0]; // x29 or r11
        dfxregs->pc_ = regs[3]; // x32 or r15
    } else if (mode == UnwinderMode::QUICKEN_UNWIND) {
        uintptr_t regs[QUT_MINI_REGS_SIZE] = {0};
        dfxregs->GetQuickenMiniRegs(regs);
        dfxregs->fp_ = regs[3]; // x29 or r11
        dfxregs->sp_ = regs[4]; // x31 or r13
        dfxregs->pc_ = regs[5]; // x32 or r15
        dfxregs->lr_ = regs[6]; // x30 or r14
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

int DfxRegs::PrintFormat(char *buf, int size, const char *format, ...) const
{
    int ret = -1;
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, size, size - 1, format, args);
    va_end(args);
    return ret;
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
    char buf[REGS_PRINT_LEN_SPECIAL] = {0};
#ifdef __LP64__
    PrintFormat(buf, sizeof(buf), "fp:%016lx sp:%016lx lr:%016lx pc:%016lx\n", fp_, sp_, lr_, pc_);
#else
    PrintFormat(buf, sizeof(buf), "fp:%08x sp:%08x lr:%08x pc:%08x\n", fp_, sp_, lr_, pc_);
#endif
    std::string regString = std::string(buf);
    return regString;
}
} // namespace HiviewDFX
} // namespace OHOS
