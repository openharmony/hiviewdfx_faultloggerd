/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#include "dfx_regs_qut.h"

#include <elf.h>
#include <securec.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxRegs"
}
std::vector<uint16_t> DfxRegsQut::qutRegs_ = {};

std::shared_ptr<DfxRegs> DfxRegs::Create()
{
#if defined(__arm__)
    auto dfxregs = std::make_shared<DfxRegsArm>();
#elif defined(__aarch64__)
    auto dfxregs = std::make_shared<DfxRegsArm64>();
#elif defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
    auto dfxregs = std::make_shared<DfxRegsRiscv64>();
#elif defined(__x86_64__)
    auto dfxregs = std::make_shared<DfxRegsX86_64>();
#else
#error "Unsupported architecture"
#endif
    return dfxregs;
}

std::shared_ptr<DfxRegs> DfxRegs::CreateFromUcontext(const ucontext_t& context)
{
    auto dfxregs = DfxRegs::Create();
    dfxregs->SetFromUcontext(context);
    return dfxregs;
}

std::shared_ptr<DfxRegs> DfxRegs::CreateFromRegs(const UnwindMode mode, const uintptr_t* regs,
                                                 size_t size)
{
    auto dfxregs = DfxRegs::Create();
    if ((mode == UnwindMode::FRAMEPOINTER_UNWIND && size < FP_MINI_REGS_SIZE) ||
        (mode == UnwindMode::MINIMAL_UNWIND && size < QUT_MINI_REGS_SIZE)) {
        LOGE("The number of long groups is too short");
        return dfxregs;
    }
    if (mode == UnwindMode::DWARF_UNWIND) {
        dfxregs->SetRegsData(regs, REG_LAST);
    } else if (mode == UnwindMode::FRAMEPOINTER_UNWIND) {
        dfxregs->SetFromFpMiniRegs(regs, FP_MINI_REGS_SIZE);
    } else if (mode == UnwindMode::MINIMAL_UNWIND) {
        dfxregs->SetFromQutMiniRegs(regs, QUT_MINI_REGS_SIZE);
    }
    return dfxregs;
}

std::shared_ptr<DfxRegs> DfxRegs::CreateRemoteRegs(pid_t pid)
{
    if (pid <= 0) {
        return nullptr;
    }
    auto dfxregs = DfxRegs::Create();
    gregset_t regs;
    struct iovec iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    // must be attach first
    if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov) == -1) {
        LOGE("Failed to ptrace pid(%d), errno=%d", pid, errno);
        return nullptr;
    }
#if defined(__x86_64__)
    dfxregs->regsData_[REG_X86_64_RAX] = regs[RAX];
    dfxregs->regsData_[REG_X86_64_RDX] = regs[RDX];
    dfxregs->regsData_[REG_X86_64_RCX] = regs[RCX];
    dfxregs->regsData_[REG_X86_64_RBX] = regs[RBX];
    dfxregs->regsData_[REG_X86_64_RSI] = regs[RSI];
    dfxregs->regsData_[REG_X86_64_RDI] = regs[RDI];
    dfxregs->regsData_[REG_X86_64_RBP] = regs[RBP];
    dfxregs->regsData_[REG_X86_64_RSP] = regs[RSP];
    dfxregs->regsData_[REG_X86_64_R8] = regs[R8];
    dfxregs->regsData_[REG_X86_64_R9] = regs[R9];
    dfxregs->regsData_[REG_X86_64_R10] = regs[R10];
    dfxregs->regsData_[REG_X86_64_R11] = regs[R11];
    dfxregs->regsData_[REG_X86_64_R12] = regs[R12];
    dfxregs->regsData_[REG_X86_64_R13] = regs[R13];
    dfxregs->regsData_[REG_X86_64_R14] = regs[R14];
    dfxregs->regsData_[REG_X86_64_R15] = regs[R15];
    dfxregs->regsData_[REG_X86_64_RIP] = regs[RIP];
#else
    if (memcpy_s(dfxregs->regsData_.data(), REG_LAST * sizeof(uintptr_t), &regs, REG_LAST * sizeof(uintptr_t)) != 0) {
        LOGE("Failed to memcpy regs data, errno=%d", errno);
        return nullptr;
    }
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
}

void DfxRegs::SetRegsData(const uintptr_t* regs, const size_t size)
{
    size_t cpySize = (size > RegsSize()) ? RegsSize() : size;
    if (memcpy_s(RawData(), cpySize * sizeof(uintptr_t), regs, cpySize * sizeof(uintptr_t)) != 0) {
        LOGE("Failed to set regs data, errno=%d", errno);
    }
}

uintptr_t* DfxRegs::GetReg(size_t idx)
{
    if (idx >= REG_LAST) {
        return nullptr;
    }
    return &(regsData_[idx]);
}

void DfxRegs::SetReg(const int idx, const uintptr_t* val)
{
    if (idx >= REG_LAST) {
        return;
    }
    regsData_[idx] = *val;
}

void DfxRegs::GetSpecialRegs(uintptr_t& fp, uintptr_t& lr, uintptr_t& sp, uintptr_t& pc) const
{
#if defined(__arm__) || defined(__aarch64__) || defined(__riscv)
    fp = regsData_[REG_FP];
    lr = regsData_[REG_LR];
#endif
    sp = regsData_[REG_SP];
    pc = regsData_[REG_PC];
}

void DfxRegs::SetSpecialRegs(uintptr_t fp, uintptr_t lr, uintptr_t sp, uintptr_t pc)
{
#if defined(__arm__) || defined(__aarch64__) || defined(__riscv)
    regsData_[REG_FP] = fp;
    regsData_[REG_LR] = lr;
#endif
    regsData_[REG_SP] = sp;
    regsData_[REG_PC] = pc;
}

uintptr_t DfxRegs::GetSp() const
{
    return regsData_[REG_SP];
}

void DfxRegs::SetSp(uintptr_t sp)
{
    regsData_[REG_SP] = sp;
}

uintptr_t DfxRegs::GetPc() const
{
    return regsData_[REG_PC];
}

void DfxRegs::SetPc(uintptr_t pc)
{
    regsData_[REG_PC] = pc;
}

uintptr_t DfxRegs::GetFp() const
{
#if defined(__arm__) || defined(__aarch64__)
    return regsData_[REG_FP];
#else
    return 0;
#endif
}

void DfxRegs::SetFp(uintptr_t fp)
{
#if defined(__arm__) || defined(__aarch64__)  || defined(__riscv)
    regsData_[REG_FP] = fp;
#endif
}

std::string DfxRegs::GetSpecialRegsName(uintptr_t val) const
{
    uintptr_t fp = 0, lr = 0, sp = 0, pc = 0;
    GetSpecialRegs(fp, lr, sp, pc);
    if (val == pc) {
        return "pc";
    } else if (val == lr) {
        return "lr";
    } else if (val == sp) {
        return "sp";
    } else if (val == fp) {
        return "fp";
    }
    return "";
}

std::string DfxRegs::PrintSpecialRegs() const
{
    uintptr_t fp = 0, lr = 0, sp = 0, pc = 0;
    GetSpecialRegs(fp, lr, sp, pc);
    std::string regsStr;
#ifdef __LP64__
    regsStr = StringPrintf("fp:%016lx sp:%016lx lr:%016lx pc:%016lx\n", fp, sp, lr, pc);
#else
    regsStr = StringPrintf("fp:%08x sp:%08x lr:%08x pc:%08x\n", fp, sp, lr, pc);
#endif
    return regsStr;
}
} // namespace HiviewDFX
} // namespace OHOS
