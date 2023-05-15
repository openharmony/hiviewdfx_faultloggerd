/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fp_unwinder.h"

#include <pthread.h>
#include <libunwind.h>
#include <libunwind_i-ohos.h>
#include "dfx_define.h"
#include "dfx_regs.h"
#include "dfx_regs_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxFpUnwinder"
}

FpUnwinder::FpUnwinder()
{
    frames_.clear();
    pthread_attr_t tattr;
    void *base = nullptr;
    size_t size = 0;
    pthread_getattr_np(pthread_self(), &tattr);
    pthread_attr_getstack(&tattr, &base, &size);
    stackBottom_ = reinterpret_cast<uintptr_t>(base);
    stackTop_ = reinterpret_cast<uintptr_t>(base) + size;
}

FpUnwinder::~FpUnwinder()
{
    frames_.clear();
}

const std::vector<NativeFrame>& FpUnwinder::GetFrames() const
{
    return frames_;
}

int FpUnwinder::DlIteratePhdrCallback(struct dl_phdr_info *info, size_t size, void *data)
{
    auto frame = static_cast<NativeFrame*>(data);
    const Elf_W(Phdr) *phdr = info->dlpi_phdr;
    for (int n = info->dlpi_phnum; --n >= 0; phdr++) {
        if (phdr->p_type == PT_LOAD) {
            Elf_W(Addr) vaddr = phdr->p_vaddr + info->dlpi_addr;
            if (frame->pc >= vaddr && frame->pc < vaddr + phdr->p_memsz) {
                frame->relativePc = frame->pc - info->dlpi_addr;
                frame->binaryName = std::string(info->dlpi_name);
                return 1; // let dl_iterate_phdr break
            }
        }
    }
    return 0;
}

bool FpUnwinder::UnwindWithContext(unw_context_t& context, size_t skipFrameNum)
{
    std::shared_ptr<DfxRegs> dfxregs = DfxRegs::Create();
#if defined(__arm__)
    dfxregs->SetFP(context.regs[REG_ARM_R11]);
    dfxregs->SetPC(context.regs[REG_ARM_R15]);
#elif defined(__aarch64__)
    dfxregs->SetFP(context.uc_mcontext.regs[REG_AARCH64_X29]);
    dfxregs->SetPC(context.uc_mcontext.pc);
#else
#pragma message("Unsupported architecture")
#endif
    uintptr_t fp = dfxregs->GetFP();
    uintptr_t pc = dfxregs->GetPC();

    size_t index = 0;
    size_t curIndex = 0;
    do {
        if (index < skipFrameNum) {
            index++;
            continue;
        }
        curIndex = index - skipFrameNum;

        NativeFrame frame;
        frame.index = curIndex;
        frame.pc = index == 0 ? pc : pc - 4; // 4 : aarch64 instruction size
        frame.fp = fp;
        frames_.emplace_back(frame);
        index++;
    } while (Step(fp, pc) && (curIndex < BACK_STACK_MAX_STEPS));
    return (frames_.size() > 0);
}

bool FpUnwinder::Unwind(size_t skipFrameNum)
{
    std::shared_ptr<DfxRegs> dfxregs = DfxRegs::Create(FP_UNWIND);
    uintptr_t fp = dfxregs->GetFP();
    uintptr_t pc = dfxregs->GetPC();

    skipFrameNum += 2; // 2 : skip GetFramePointerMiniRegs and Create function

    size_t index = 0;
    size_t curIndex = 0;
    do {
        if (index < skipFrameNum) {
            index++;
            continue;
        }
        curIndex = index - skipFrameNum;

        NativeFrame frame;
        frame.index = curIndex;
        frame.pc = index == 0 ? pc : pc - 4; // 4 : aarch64 instruction size
        frame.fp = fp;
        frames_.emplace_back(frame);
        index++;
    } while (Step(fp, pc) && (curIndex < BACK_STACK_MAX_STEPS));
    return (frames_.size() > 0);
}

void FpUnwinder::UpdateFrameInfo()
{
    if (frames_.empty()) {
        return;
    }

    auto it = frames_.begin();
    while (it != frames_.end()) {
        if (dl_iterate_phdr(FpUnwinder::DlIteratePhdrCallback, &(*it)) != 1) {
            // clean up frames after first invalid frame
            frames_.erase(it, frames_.end());
            break;
        }
        it++;
    }
}

bool FpUnwinder::Step(uintptr_t& fp, uintptr_t& pc)
{
    uintptr_t prevFp = fp;
    if ((prevFp > stackBottom_) && (prevFp + sizeof(uintptr_t) < stackTop_)) {
        fp = *reinterpret_cast<uintptr_t*>(prevFp);
        pc = *reinterpret_cast<uintptr_t*>(prevFp + sizeof(uintptr_t));
        return true;
    }
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS
