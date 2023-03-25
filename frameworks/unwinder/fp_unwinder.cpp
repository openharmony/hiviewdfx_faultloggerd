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
#ifdef __aarch64__
    uintptr_t fp = context.uc_mcontext.regs[29]; // 29 : fp location
    uintptr_t pc = context.uc_mcontext.pc;

    size_t index = 0;
    do {
        if (index < skipFrameNum) {
            index++;
            continue;
        }

        NativeFrame frame;
        frame.index = index - skipFrameNum;
        frame.pc = index == 0 ? pc : pc - 4; // 4 : aarch64 instruction size
        frame.fp = fp;
        frames_.emplace_back(frame);
        index++;
    } while (Step(fp, pc) && (index < BACK_STACK_MAX_STEPS));
#endif
    return (frames_.size() > 0);
}

bool FpUnwinder::UnwindWithRegs(size_t skipFrameNum)
{
    uintptr_t regs[FP_MINI_REGS_SIZE] = {0};
    std::shared_ptr<DfxRegs> dfxreg;
#if defined(__arm__)
    dfxreg = std::make_shared<DfxRegsArm>();
#elif defined(__aarch64__)
    dfxreg = std::make_shared<DfxRegsArm64>();
#elif defined(__x86_64__)
    return false;
#endif
    dfxreg->GetFramePointerMiniRegs(regs);
    uintptr_t fp = regs[0]; // x29
    uintptr_t pc = regs[3]; // x32

    // skip get mini regs function
    skipFrameNum += 1;

    size_t index = 0;
    do {
        if (index < skipFrameNum) {
            index++;
            continue;
        }

        NativeFrame frame;
        frame.index = index - skipFrameNum;
        frame.pc = index == 0 ? pc : pc - 4; // 4 : aarch64 instruction size
        frame.fp = fp;
        frames_.emplace_back(frame);
        index++;
    } while (Step(fp, pc) && ((index - skipFrameNum) < BACK_STACK_MAX_STEPS));
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
