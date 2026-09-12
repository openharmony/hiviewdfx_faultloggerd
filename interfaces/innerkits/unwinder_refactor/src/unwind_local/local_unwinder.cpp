/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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
#include "local_unwinder.h"

#include <algorithm>
#include <cstdint>
#include <link.h>
#include <unistd.h>

#include "dfx_ark.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_regs_get.h"
#include "dfx_util.h"
#include "thread_context.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {

void LocalUnwinder::GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
{
    frames.clear();
    for (size_t i = 0; i < pcs.size(); ++i) {
        DfxFrame frame;
        frame.index = i;
        frame.pc = static_cast<uint64_t>(pcs[i]);
        frames.emplace_back(frame);
    }
    FillLocalFrames(frames);
}

void LocalUnwinder::FillLocalFrames(std::vector<DfxFrame>& frames)
{
    if (frames.empty()) {
        return;
    }
    auto it = frames.begin();
    while (it != frames.end()) {
        if (dl_iterate_phdr(DlPhdrCallback, &(*it)) != 1) {
            frames.erase(it, frames.end());
            break;
        }
        it++;
    }
}

int LocalUnwinder::DlPhdrCallback(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    auto frame = reinterpret_cast<DfxFrame*>(data);
    const ElfW(Phdr)* phdr = info->dlpi_phdr;
    frame->pc = StripPac(frame->pc, 0);
    for (int n = info->dlpi_phnum; --n >= 0; phdr++) {
        if (IsPcInLoadSegment(frame->pc, info, phdr)) {
            frame->relPc = frame->pc - info->dlpi_addr;
            frame->mapName = std::string(info->dlpi_name);
            return 1;
        }
    }
    return 0;
}

bool LocalUnwinder::IsPcInLoadSegment(uintptr_t pc, struct dl_phdr_info *info,
    const ElfW(Phdr)* phdr)
{
    if (phdr->p_type != PT_LOAD) {
        return false;
    }
    ElfW(Addr) vaddr = phdr->p_vaddr + info->dlpi_addr;
    return pc >= vaddr && pc < vaddr + phdr->p_memsz;
}

bool LocalUnwinder::FillJsFrame(DfxFrame& frame, JsFunction* jsFunction)
{
    if (DfxArk::Instance().ArkCreateLocal() < 0) {
        DFXLOGW("Failed to ark create local.");
        return false;
    }
    if (DfxArk::Instance().ParseArkFrameInfoLocal(static_cast<uintptr_t>(frame.pc),
        static_cast<uintptr_t>(frame.map->begin), static_cast<uintptr_t>(frame.map->offset),
        jsFunction) < 0) {
        DFXLOGW("Failed to parse ark frame info local, pc: %{private}p", reinterpret_cast<void*>(frame.pc));
        return false;
    }
    frame.isJsFrame = true;
    return true;
}

LocalUnwinder::LocalUnwinder(bool needMaps)
    : UnwinderBase(std::make_shared<UnwinderSharedState>())
{
    state_->unwindType = UNWIND_TYPE_LOCAL;
    if (needMaps) {
        state_->maps = DfxMaps::Create();
    }
    InitCommon();
}

bool LocalUnwinder::DoUnwindLocalWithContext(const ucontext_t& context, size_t maxFrameNum,
    size_t skipFrameNum)
{
    if (state_->regs == nullptr) {
        state_->regs = DfxRegs::CreateFromUcontext(context);
    } else {
        state_->regs->SetFromUcontext(context);
    }
    return DoUnwindLocal(true, false, maxFrameNum, skipFrameNum, false);
}

bool LocalUnwinder::DoUnwindLocalWithTid(const pid_t tid, size_t maxFrameNum, size_t skipFrameNum)
{
#if defined(__aarch64__) || defined(__loongarch_lp64) || defined(__x86_64__)
    if (IsInvalidTid(tid)) {
        state_->lastErrorData.SetCode(UNW_ERROR_NOT_SUPPORT);
        return false;
    }
    return FillPcsByTid(tid, maxFrameNum, skipFrameNum);
#else
    (void)tid;
    (void)maxFrameNum;
    (void)skipFrameNum;
    return false;
#endif
}

bool LocalUnwinder::IsInvalidTid(pid_t tid)
{
    return tid < 0 || tid == gettid();
}

bool LocalUnwinder::FillPcsByTid(pid_t tid, size_t maxFrameNum, size_t skipFrameNum)
{
    auto threadContext = LocalThreadContext::GetInstance().CollectThreadContext(tid);
    if (threadContext == nullptr || threadContext->frameSz == 0) {
        return false;
    }
    auto& pcs = frameMgr_.GetPcs();
    const_cast<std::vector<uintptr_t>&>(pcs).clear();
    size_t frameSize = std::min(threadContext->frameSz.load(), skipFrameNum + maxFrameNum);
    for (size_t i = skipFrameNum; i < frameSize; i++) {
        const_cast<std::vector<uintptr_t>&>(pcs).emplace_back(threadContext->pcs[i]);
    }
    state_->firstFrameSp = threadContext->firstFrameSp;
    return true;
}

bool LocalUnwinder::DoUnwindLocalByOtherTid(const pid_t tid, bool fast, size_t maxFrameNum,
    size_t skipFrameNum)
{
    if (IsInvalidTid(tid)) {
        state_->lastErrorData.SetCode(UNW_ERROR_NOT_SUPPORT);
        return false;
    }
    auto& instance = LocalThreadContextMix::GetInstance();
    if (tid == getpid()) {
        uintptr_t stackBottom = 1;
        uintptr_t stackTop = static_cast<uintptr_t>(-1);
        unwindCore_.GetMainStackRange(stackBottom, stackTop);
        instance.SetStackRang(stackTop, stackBottom);
    }
    if (!instance.CollectThreadContext(tid)) {
        instance.ReleaseCollectThreadContext();
        return false;
    }
    bool ret = DoUnwindOtherTid(instance, fast, maxFrameNum, skipFrameNum);
    instance.ReleaseCollectThreadContext();
    return ret;
}

bool LocalUnwinder::DoUnwindOtherTid(LocalThreadContextMix& instance, bool fast,
    size_t maxFrameNum, size_t skipFrameNum)
{
    std::shared_ptr<DfxRegs> regs = DfxRegs::Create();
    instance.SetRegister(regs);
    state_->regs = regs;
    state_->firstFrameSp = regs->GetSp();
    frameMgr_.SetEnableFillFrames(true);
    mixedStack_.SetBytecodePcAndInterruptPc();
    if (fast) {
        state_->maps = instance.GetMaps();
        return unwindCore_.UnwindByFp(&instance, maxFrameNum, 0, true);
    }
    return unwindCore_.Unwind(&instance, maxFrameNum, skipFrameNum);
}

bool __attribute__((optnone)) LocalUnwinder::DoUnwindLocal(bool withRegs, bool fpUnwind,
    size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
{
    uintptr_t stackBottom = 1;
    uintptr_t stackTop = static_cast<uintptr_t>(-1);
    if (!unwindCore_.GetStackRange(stackBottom, stackTop)) {
        return false;
    }
    if (!PrepareLocalRegs(withRegs, fpUnwind)) {
        return false;
    }
    FillLocalContext(stackBottom, stackTop);
    return UnwindLocalByMode(fpUnwind, maxFrameNum, skipFrameNum, enableArk);
}

bool LocalUnwinder::PrepareLocalRegs(bool& withRegs, bool fpUnwind)
{
    if (withRegs) {
        return true;
    }
#if defined(__aarch64__) || defined(__x86_64__)
    if (fpUnwind) {
        uintptr_t miniRegs[FP_MINI_REGS_SIZE] = {0};
        GetFramePointerMiniRegs(miniRegs, sizeof(miniRegs) / sizeof(miniRegs[0]));
        state_->regs = DfxRegs::CreateFromRegs(UnwindMode::FRAMEPOINTER_UNWIND, miniRegs,
            sizeof(miniRegs) / sizeof(miniRegs[0]));
        withRegs = true;
        return true;
    }
#endif
    state_->regs = DfxRegs::Create();
    auto regsData = state_->regs->RawData();
    if (regsData == nullptr) {
        return false;
    }
    GetLocalRegs(regsData);
    return true;
}

void LocalUnwinder::FillLocalContext(uintptr_t stackBottom, uintptr_t stackTop)
{
    mixedStack_.SetBytecodePcAndInterruptPc();
    state_->context.pid = UNWIND_TYPE_LOCAL;
    state_->context.regs = state_->regs;
    state_->context.maps = state_->maps;
    state_->context.stackCheck = true;
    state_->context.stackBottom = stackBottom;
    state_->context.stackTop = stackTop;
}

bool LocalUnwinder::UnwindLocalByMode(bool fpUnwind, size_t maxFrameNum, size_t skipFrameNum,
    bool enableArk)
{
#if defined(__aarch64__) || defined(__x86_64__)
    if (fpUnwind) {
        return unwindCore_.UnwindByFp(&state_->context, maxFrameNum, skipFrameNum, enableArk);
    }
#else
    (void)fpUnwind;
    (void)maxFrameNum;
    (void)skipFrameNum;
    (void)enableArk;
#endif
    return unwindCore_.Unwind(&state_->context, maxFrameNum, skipFrameNum);
}

void LocalUnwinder::OnDestroy()
{
#if defined(__aarch64__) || defined(__loongarch_lp64) || defined(__x86_64__)
    if (state_->unwindType == UNWIND_TYPE_LOCAL) {
        LocalThreadContext::GetInstance().CleanUp();
    }
#endif
}

} // namespace HiviewDFX
} // namespace OHOS
