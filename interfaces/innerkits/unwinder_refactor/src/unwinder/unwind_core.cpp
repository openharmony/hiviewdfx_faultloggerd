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
#include "unwind_core.h"

#include <algorithm>
#include <string>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_instructions.h"
#include "dfx_log.h"
#include "dfx_param.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#include "string_printf.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxUnwindCore"

#if defined(__x86_64__)
constexpr size_t X86_FP_SP_OFFSET = 16;
#endif
constexpr uintptr_t PC_ADJUST_DEFAULT = 0x4;
constexpr uintptr_t PC_ADJUST_ARM_THUMB = 0x2;
[[maybe_unused]] constexpr uintptr_t PC_ADJUST_X86 = 0x1;
constexpr uintptr_t PC_ADJUST_MIN = 0x4;
constexpr uintptr_t PC_ADJUST_ARM_CHECK = 0x5;
constexpr uint32_t ARM_THUMB_MASK = 0xe000f000;
constexpr int32_t MIN_FRAMES_FOR_MAP_ERROR = 2;
[[maybe_unused]] constexpr int32_t LOCK_INFO_SP_OFFSET = 64;
}

UnwindCore::UnwindCore(UnwinderSharedState& state, FrameManager& frameMgr,
    MixedStackHandler& mixedStack)
    : state_(state), frameMgr_(frameMgr), mixedStack_(mixedStack)
{}

bool UnwindCore::FindCache(uintptr_t pc, std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs)
{
    if (!cacheEnabled_ || state_.unwindType == UNWIND_TYPE_CUSTOMIZE) {
        return false;
    }
    auto iter = cacheMap_.find(pc);
    if (iter == cacheMap_.end()) {
        return false;
    }
    rs = iter->second.rs;
    map = iter->second.map;
    if (!state_.IsCustomize()) {
        DFXLOGU("Find rs cache, pc: %{public}p", reinterpret_cast<void*>(pc));
    }
    return true;
}

void UnwindCore::UpdateCache(uintptr_t pc, bool hasRegLocState,
    const std::shared_ptr<RegLocState>& rs, const std::shared_ptr<DfxMap>& map)
{
    if (!hasRegLocState || !cacheEnabled_) {
        return;
    }
    StepCache cache;
    cache.map = map;
    cache.rs = rs;
    cacheMap_.emplace(pc, cache);
}

bool UnwindCore::GetMainStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    if (state_.maps != nullptr) {
        if (!state_.maps->GetStackRange(stackBottom, stackTop)) {
            return false;
        }
    } else {
        if (!StackUtils::Instance().GetMainStackRange(stackBottom, stackTop)) {
            return false;
        }
    }
    return true;
}

bool UnwindCore::GetArkStackRange(uintptr_t& arkMapStart, uintptr_t& arkMapEnd)
{
    if (state_.maps != nullptr) {
        if (!state_.maps->GetArkStackRange(arkMapStart, arkMapEnd)) {
            return false;
        }
    } else {
        if (!StackUtils::Instance().GetArkStackRange(arkMapStart, arkMapEnd)) {
            return false;
        }
    }
    return true;
}

bool UnwindCore::GetStaticArkStackRange(uintptr_t& arkMapStart, uintptr_t& arkMapEnd)
{
    if (state_.maps == nullptr) {
        return false;
    }
    return state_.maps->GetStaticArkRange(arkMapStart, arkMapEnd);
}

bool UnwindCore::GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
{
    if (gettid() == getpid()) {
        return GetMainStackRange(stackBottom, stackTop);
    }
    return StackUtils::GetSelfStackRange(stackBottom, stackTop);
}

bool UnwindCore::CheckAndReset(void* ctx)
{
    if ((ctx == nullptr) || (state_.memory == nullptr)) {
        return false;
    }
    state_.memory->SetCtx(ctx);
    return true;
}

void UnwindCore::SetLocalStackCheck(void* ctx, bool check) const
{
    if (state_.IsLocal() && (ctx != nullptr)) {
        UnwindContext* uctx = reinterpret_cast<UnwindContext*>(ctx);
        uctx->stackCheck = check;
    }
}

bool UnwindCore::Unwind(void* ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    if ((state_.regs == nullptr) || (!CheckAndReset(ctx))) {
        DFXLOGE("[%{public}d]: params is nullptr?", __LINE__);
        state_.lastErrorData.SetCode(UNW_ERROR_INVALID_CONTEXT);
        return false;
    }
    SetLocalStackCheck(ctx, false);
    frameMgr_.Clear();
    Clear();
    return UnwindFrames(ctx, maxFrameNum, skipFrameNum);
}

bool UnwindCore::UnwindFrames(void* ctx, size_t maxFrameNum, size_t skipFrameNum)
{
    bool needAdjustPc = false;
    StepFrame frame;
    while (true) {
        if (ShouldResetFrames(skipFrameNum)) {
            isResetFrames_ = true;
            DFXLOGU("frames size: %{public}zu, will be reset frames", frameMgr_.GetFrameCount());
        }
        if (IsMaxFramesReached(maxFrameNum)) {
            break;
        }
        if (!UnwindFrame(ctx, frame, needAdjustPc)) {
            break;
        }
    }
    DFXLOGU("Last error code: %{public}d, addr: %{public}p",
        static_cast<int>(state_.lastErrorData.GetCode()),
        reinterpret_cast<void*>(state_.lastErrorData.GetAddr()));
    return (frameMgr_.GetFrameCount() > 0);
}

bool UnwindCore::ShouldResetFrames(size_t skipFrameNum)
{
    return !isResetFrames_ && (skipFrameNum != 0) && (frameMgr_.GetFrameCount() >= skipFrameNum);
}

bool UnwindCore::IsMaxFramesReached(size_t maxFrameNum)
{
    if (frameMgr_.GetFrameCount() >= maxFrameNum) {
        DFXLOGW("frames size: %{public}zu", frameMgr_.GetFrameCount());
        state_.lastErrorData.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
        return true;
    }
    return false;
}

bool UnwindCore::UnwindByFp(void* ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
{
    if (state_.regs == nullptr) {
        DFXLOGE("[%{public}d]: params is nullptr?", __LINE__);
        return false;
    }
    const_cast<std::vector<uintptr_t>&>(frameMgr_.GetPcs()).clear();
    ArkMapRange arkMapRange;
    if (enableArk) {
        GetArkStackRange(arkMapRange.arkMapStart, arkMapRange.arkMapEnd);
        GetStaticArkStackRange(arkMapRange.staticArkMapStart, arkMapRange.staticArkMapEnd);
    }
    return UnwindByFpLoop(ctx, maxFrameNum, skipFrameNum, arkMapRange, enableArk);
}

bool UnwindCore::UnwindByFpLoop(void* ctx, size_t maxFrameNum, size_t skipFrameNum,
    const ArkMapRange& arkMapRange, bool enableArk)
{
    bool needAdjustPc = false;
    bool resetFrames = false;
    StepFrame frame;
    uint64_t frameIndex = 0;
    (void)frameIndex;
    while (true) {
        HandleResetPcs(resetFrames, skipFrameNum);
        if (IsMaxPcsReached(maxFrameNum)) {
            break;
        }
        if (!FillFpFrameAndStep(frame, ctx, arkMapRange, enableArk, needAdjustPc, frameIndex)) {
            break;
        }
    }
    return (frameMgr_.GetPcs().size() > 0);
}

void UnwindCore::HandleResetPcs(bool& resetFrames, size_t skipFrameNum)
{
    if (ShouldResetPcs(resetFrames, skipFrameNum)) {
        DFXLOGU("pcs size: %{public}zu, will be reset pcs", frameMgr_.GetPcs().size());
        resetFrames = true;
    }
}

bool UnwindCore::ShouldResetPcs(bool& resetFrames, size_t skipFrameNum)
{
    return !resetFrames && (skipFrameNum != 0) && (frameMgr_.GetPcs().size() == skipFrameNum);
}

bool UnwindCore::IsMaxPcsReached(size_t maxFrameNum)
{
    if (frameMgr_.GetPcs().size() >= maxFrameNum) {
        state_.lastErrorData.SetCode(UNW_ERROR_MAX_FRAMES_EXCEEDED);
        return true;
    }
    return false;
}

bool UnwindCore::FillFpFrameAndStep(StepFrame& frame, void* ctx, const ArkMapRange& arkMapRange,
    bool enableArk, bool& needAdjustPc, uint64_t frameIndex)
{
    frame.pc = state_.regs->GetPc();
    frame.sp = state_.regs->GetSp();
    frame.fp = state_.regs->GetFp();
    AdjustFpPcIfNeeded(frame, needAdjustPc);
    needAdjustPc = true;
    const_cast<std::vector<uintptr_t>&>(frameMgr_.GetPcs()).emplace_back(frame.pc);
    int arkRet = CheckArkFrameByFp(frame, arkMapRange, enableArk, frameIndex);
    if (arkRet == 0) {
        return true;
    }
    if (arkRet < 0) {
        return false;
    }
    return !IsFpStepTerminated(frame, ctx);
}

void UnwindCore::AdjustFpPcIfNeeded(StepFrame& frame, bool needAdjustPc)
{
    if (!frame.isJsFrame && needAdjustPc) {
        DoPcAdjust(frame.pc);
    }
}

int UnwindCore::CheckArkFrameByFp(StepFrame& frame, const ArkMapRange& arkMapRange,
    bool enableArk, uint64_t& frameIndex)
{
#if defined(ENABLE_MIXSTACK)
    if (!enableArk) {
        return 1;
    }
    return mixedStack_.UnwindArkFrameByFp(frame, arkMapRange, frameIndex);
#else
    (void)frame;
    (void)arkMapRange;
    (void)enableArk;
    (void)frameIndex;
    return 1;
#endif
}

bool UnwindCore::IsFpStepTerminated(StepFrame& frame, void* ctx)
{
    return !FpStep(frame.fp, frame.pc, ctx) || (frame.pc == 0);
}

bool UnwindCore::FpStep(uintptr_t& fp, uintptr_t& pc, void* ctx)
{
#if defined(__aarch64__) || defined(__x86_64__)
    DFXLOGU("+fp: %{public}lx, pc: %{public}lx", (uint64_t)fp, (uint64_t)pc);
    if ((state_.regs == nullptr) || (state_.memory == nullptr)) {
        DFXLOGE("[%{public}d]: params is nullptr", __LINE__);
        return false;
    }
    if (ctx != nullptr) {
        state_.memory->SetCtx(ctx);
    }
    return DoFpStep(fp, pc);
#else
    (void)fp;
    (void)pc;
    (void)ctx;
    return false;
#endif
}

bool UnwindCore::DoFpStep(uintptr_t& fp, uintptr_t& pc)
{
    uintptr_t prevFp = fp;
    uintptr_t ptr = fp;
    if (!ReadFpAndPc(ptr, fp, pc)) {
        return false;
    }
    if (fp != 0 && fp <= prevFp) {
        DFXLOGU("Illegal or same fp value");
        state_.lastErrorData.SetAddrAndCode(pc, UNW_ERROR_ILLEGAL_VALUE);
        return false;
    }
    UpdateFpRegs(fp, pc, prevFp);
    DFXLOGU("-fp: %{public}lx, pc: %{public}lx", (uint64_t)fp, (uint64_t)pc);
    return true;
}

bool UnwindCore::ReadFpAndPc(uintptr_t ptr, uintptr_t& fp, uintptr_t& pc)
{
    return ptr != 0 && state_.memory->Read<uintptr_t>(ptr, &fp, true) &&
        state_.memory->Read<uintptr_t>(ptr, &pc, false);
}

void UnwindCore::UpdateFpRegs(uintptr_t fp, uintptr_t pc, uintptr_t prevFp)
{
#ifdef __x86_64__
    uintptr_t newSp = fp + X86_FP_SP_OFFSET;
    state_.regs->SetReg(REG_FP, &fp);
    state_.regs->SetReg(REG_SP, &newSp);
    state_.regs->SetPc(pc);
#else
    state_.regs->SetReg(REG_FP, &fp);
    state_.regs->SetReg(REG_SP, &prevFp);
    state_.regs->SetPc(StripPac(pc, state_.pacMask));
#endif
}

bool UnwindCore::Step(uintptr_t& pc, uintptr_t& sp, void* ctx)
{
    DFX_TRACE_SCOPED_DLSYM("Step pc:%p", reinterpret_cast<void*>(pc));
    if ((state_.regs == nullptr) || (!CheckAndReset(ctx))) {
        DFXLOGE("[%{public}d]: params is nullptr?", __LINE__);
        return false;
    }
    StepFrame frame;
    frame.pc = pc;
    frame.sp = sp;
    frame.fp = state_.regs->GetFp();
    bool ret = false;
    if (state_.regs->StepIfSignalFrame(frame.pc, state_.memory)) {
        DFXLOGW("Step signal frame, pc: %{private}p", reinterpret_cast<void*>(frame.pc));
        ret = StepInner(true, frame, ctx);
    } else {
        ret = StepInner(false, frame, ctx);
    }
    pc = frame.pc;
    sp = frame.sp;
    return ret;
}

bool UnwindCore::UnwindFrame(void* ctx, StepFrame& frame, bool& needAdjustPc)
{
    frame.pc = state_.regs->GetPc();
    frame.sp = state_.regs->GetSp();
    frame.fp = state_.regs->GetFp();
    if (IsSignalFrameStep(frame)) {
        DFXLOGW("Step signal frame, pc: %{private}p", reinterpret_cast<void*>(frame.pc));
        StepInner(true, frame, ctx);
        return true;
    }
    AdjustFramePc(frame, needAdjustPc);
    needAdjustPc = true;
    uintptr_t prevPc = frame.pc;
    uintptr_t prevSp = frame.sp;
    if (!StepInner(false, frame, ctx)) {
        return false;
    }
    if (IsRepeatedFrame(frame, prevPc, prevSp)) {
        LogRepeatedFrame(ctx);
        state_.lastErrorData.SetAddrAndCode(frame.pc, UNW_ERROR_REPEATED_FRAME);
        return false;
    }
    return true;
}

bool UnwindCore::IsSignalFrameStep(const StepFrame& frame)
{
    return !state_.IsLocal() && state_.regs->StepIfSignalFrame(
        static_cast<uintptr_t>(frame.pc), state_.memory);
}

void UnwindCore::AdjustFramePc(StepFrame& frame, bool needAdjustPc)
{
    if (!frame.isJsFrame && needAdjustPc) {
        DoPcAdjust(frame.pc);
    }
}

bool UnwindCore::IsRepeatedFrame(const StepFrame& frame, uintptr_t prevPc, uintptr_t prevSp)
{
    return frame.pc == prevPc && frame.sp == prevSp && frameMgr_.GetFrameCount() > 1;
}

void UnwindCore::LogRepeatedFrame(void* ctx)
{
    if (state_.pid >= 0) {
        MAYBE_UNUSED UnwindContext* uctx = reinterpret_cast<UnwindContext*>(ctx);
        DFXLOGU("pc and sp is same, tid: %{public}d", uctx->pid);
    } else {
        DFXLOGU("pc and sp is same");
    }
}

bool UnwindCore::ParseUnwindTable(uintptr_t pc, std::shared_ptr<RegLocState>& rs)
{
    UnwindTableInfo uti;
    int utiRet = state_.memory->FindUnwindTable(pc, uti);
    if (utiRet != UNW_ERROR_NONE) {
        state_.lastErrorData.SetAddrAndCode(pc, utiRet);
        DFXLOGU("Failed to find unwind table ret: %{public}d", utiRet);
        return false;
    }
    rs = std::make_shared<RegLocState>();
    if (!state_.unwindEntryParser->Step(pc, uti, rs)) {
        state_.lastErrorData.SetAddrAndCode(state_.unwindEntryParser->GetLastErrorAddr(),
            state_.unwindEntryParser->GetLastErrorCode());
        DFXLOGU("Step unwind entry failed");
        return false;
    }
    return true;
}

void UnwindCore::StepToNextFpIfNeed()
{
#if defined(__aarch64__) || defined(__x86_64__)
    if (state_.memory == nullptr || state_.regs == nullptr) {
        return;
    }
    uintptr_t fpStepPc = 0;
    uintptr_t pcPtr = state_.regs->GetFp() + sizeof(uintptr_t);
    state_.memory->Read<uintptr_t>(pcPtr, &fpStepPc, false);
    if (state_.regs->GetPc() == fpStepPc) {
        auto fp = state_.regs->GetFp();
        uintptr_t nextFp = 0;
        state_.memory->Read<uintptr_t>(fp, &nextFp, false);
        state_.regs->SetFp(nextFp);
    }
#endif
}

void UnwindCore::UpdateRegsState(StepFrame& frame, void* ctx, bool& unwinderResult,
    std::shared_ptr<RegLocState>& rs)
{
    SetLocalStackCheck(ctx, true);
    if (unwinderResult) {
        unwinderResult = ApplyWithLrFallback(rs);
    } else {
        TryLrFallbackOnStepFail(unwinderResult);
    }
    state_.regs->SetPc(StripPac(state_.regs->GetPc(), state_.pacMask));
    TryFpStep(frame, ctx, unwinderResult);
    frame.pc = state_.regs->GetPc();
    frame.sp = state_.regs->GetSp();
    frame.fp = state_.regs->GetFp();
}

bool UnwindCore::ApplyWithLrFallback(std::shared_ptr<RegLocState>& rs)
{
#if defined(__arm__) || defined(__aarch64__)
    auto lr = *(state_.regs->GetReg(REG_LR));
#endif
    bool ret = Apply(state_.regs, rs);
#if defined(__arm__) || defined(__aarch64__)
    if (NeedLrFallback(ret)) {
        state_.regs->SetPc(lr);
        ret = true;
        LogLrFallback();
    }
#else
    (void)rs;
#endif
    return ret;
}

bool UnwindCore::NeedLrFallback(bool applyResult)
{
    return !applyResult && enableLrFallback_ && (frameMgr_.GetFrameCount() == 1);
}

void UnwindCore::LogLrFallback()
{
    if (!state_.IsCustomize()) {
        DFXLOGW("Failed to apply first frame, lr fallback");
    }
}

bool UnwindCore::IsFirstFrameLrFallback()
{
    return enableLrFallback_ && (frameMgr_.GetFrameCount() == 1) && !isResetFrames_;
}

void UnwindCore::TryLrFallbackOnStepFail(bool& unwinderResult)
{
    if (IsFirstFrameLrFallback() &&
        state_.regs->SetPcFromReturnAddress(state_.memory)) {
        unwinderResult = true;
        StepToNextFpIfNeed();
        LogStepFailLrFallback();
    }
}

void UnwindCore::LogStepFailLrFallback()
{
    if (!state_.IsCustomize()) {
        DFXLOGW("Failed to step first frame, lr fallback");
    }
}

void UnwindCore::TryFpStep(StepFrame& frame, void* ctx, bool& unwinderResult)
{
#if defined(__aarch64__) || defined(__x86_64__)
    if (!unwinderResult) {
        unwinderResult = FpStep(frame.fp, frame.pc, ctx);
        if (unwinderResult && !isFpStep_) {
            LogFirstFpStep(frame.pc);
            isFpStep_ = true;
        }
    }
#else
    (void)frame;
    (void)ctx;
    (void)unwinderResult;
#endif
}

void UnwindCore::LogFirstFpStep(uintptr_t pc)
{
    if (!state_.IsCustomize()) {
        DFXLOGI("First enter fp step, pc: %{private}p", reinterpret_cast<void*>(pc));
    }
}

bool UnwindCore::CheckFrameValid(const StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    uintptr_t prevSp)
{
    DFXLOGU("-pc: %{public}p, sp: %{public}p, fp: %{public}p, prevSp: %{public}p",
        reinterpret_cast<void*>(frame.pc), reinterpret_cast<void*>(frame.sp),
        reinterpret_cast<void*>(frame.fp), reinterpret_cast<void*>(prevSp));
    if (IsIllegalSpValue(frame, map, prevSp)) {
        DFXLOGU("Illegal sp value");
        state_.lastErrorData.SetAddrAndCode(frame.pc, UNW_ERROR_ILLEGAL_VALUE);
        return false;
    }
    if (frame.pc == 0) {
        return false;
    }
    return true;
}

bool UnwindCore::IsIllegalSpValue(const StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    uintptr_t prevSp)
{
    return !isFpStep_ && (map != nullptr) && (!map->IsVdsoMap()) && (frame.sp < prevSp);
}

bool UnwindCore::GetCrashLastFrame(StepFrame& frame)
{
    if (state_.isCrash && !isFpStep_ && frameMgr_.GetFrameCount() > 0) {
        const auto& frames = frameMgr_.GetFrames();
        const auto& lastFrame = frames.back();
        frame.pc = lastFrame.pc;
        frame.sp = lastFrame.sp;
        frame.fp = lastFrame.fp;
        frame.isJsFrame = lastFrame.isJsFrame;
        DFXLOGW("Dwarf unwind failed to find map, try fp unwind again");
        return true;
    }
    return false;
}

bool UnwindCore::AddFrameMap(const StepFrame& frame, std::shared_ptr<DfxMap>& map)
{
    int mapRet = state_.memory->GetMapByPc(frame.pc, map);
    if (mapRet != UNW_ERROR_NONE) {
        if (frame.isJsFrame) {
            DFXLOGW("Failed to get js frame map, frames size: %{public}zu", frameMgr_.GetFrameCount());
            mapRet = UNW_ERROR_UNKNOWN_ARK_MAP;
        }
        if (frameMgr_.GetFrameCount() > MIN_FRAMES_FOR_MAP_ERROR) {
            DFXLOGU("Failed to get map, frames size: %{public}zu", frameMgr_.GetFrameCount());
            state_.lastErrorData.SetAddrAndCode(frame.pc, mapRet);
            return false;
        }
    }
    frameMgr_.AddFrame(frame, map);
    return true;
}

bool UnwindCore::StepInner(bool isSigFrame, StepFrame& frame, void* ctx)
{
    if (!CheckStepParams(ctx)) {
        DFXLOGE("params is nullptr");
        return false;
    }
    SetLocalStackCheck(ctx, false);
    DFXLOGU("+pc: %{public}p, sp: %{public}p, fp: %{public}p",
        reinterpret_cast<void*>(frame.pc), reinterpret_cast<void*>(frame.sp),
        reinterpret_cast<void*>(frame.fp));
    uintptr_t prevSp = frame.sp;
    bool hasRegLocState = false;
    std::shared_ptr<RegLocState> rs = nullptr;
    std::shared_ptr<DfxMap> map = nullptr;
    bool finished = false;
    bool result = false;
    ProcessStepFrame(isSigFrame, frame, ctx, map, rs, hasRegLocState, finished, result);
    if (finished) {
        return result;
    }
    UpdateRegsState(frame, ctx, hasRegLocState, rs);
    return CheckFrameValid(frame, map, prevSp) ? hasRegLocState : false;
}

bool UnwindCore::CheckStepParams(void* ctx)
{
    return (state_.regs != nullptr) && CheckAndReset(ctx);
}

void UnwindCore::ProcessStepFrame(bool isSigFrame, StepFrame& frame, void* ctx,
    std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs,
    bool& hasRegLocState, bool& finished, bool& result)
{
    if (FindCache(frame.pc, map, rs)) {
        hasRegLocState = true;
        frameMgr_.AddFrame(frame, map);
        return;
    }
    StepInnerNoCache(isSigFrame, frame, ctx, map, rs, hasRegLocState, finished, result);
}

void UnwindCore::StepInnerNoCache(bool isSigFrame, StepFrame& frame, void* ctx,
    std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs,
    bool& hasRegLocState, bool& finished, bool& result)
{
    if (!AddFrameMap(frame, map)) {
        finished = !GetCrashLastFrame(frame);
        result = false;
        return;
    }
    if (isSigFrame) {
        finished = true;
        result = true;
        return;
    }
    HandleMixedStackStep(frame, map, rs, hasRegLocState, ctx, finished, result);
}

void UnwindCore::HandleMixedStackStep(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    std::shared_ptr<RegLocState>& rs, bool& hasRegLocState, void* ctx,
    bool& finished, bool& result)
{
    bool stopUnwind = false;
    bool res = mixedStack_.StepV8Frame(frame, map, stopUnwind);
    if (stopUnwind) {
        finished = true;
        result = res;
        return;
    }
    if (!res) {
        UpdateRegsState(frame, ctx, hasRegLocState, rs);
        finished = true;
        result = false;
        return;
    }
    HandleArkFrameAndParse(frame, map, rs, hasRegLocState, ctx, finished, result);
}

void UnwindCore::HandleArkFrameAndParse(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    std::shared_ptr<RegLocState>& rs, bool& hasRegLocState, void* ctx,
    bool& finished, bool& result)
{
    bool stopUnwind = false;
    bool processFrameResult = mixedStack_.UnwindArkFrame(frame, map, stopUnwind);
    if (stopUnwind) {
        finished = true;
        result = processFrameResult;
        return;
    }
    if (!processFrameResult) {
        UpdateRegsState(frame, ctx, hasRegLocState, rs);
        finished = true;
        result = false;
        return;
    }
    ParseTableOrCheckFp(frame, map, rs, hasRegLocState, finished, result);
}

void UnwindCore::ParseTableOrCheckFp(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    std::shared_ptr<RegLocState>& rs, bool& hasRegLocState,
    bool& finished, bool& result)
{
    if (isFpStep_) {
        if (IsFpMapNotExec(map)) {
            DFXLOGE("Fp step check map is not exec");
            finished = true;
            result = false;
            return;
        }
    } else {
        hasRegLocState = ParseUnwindTable(frame.pc, rs);
        UpdateCache(frame.pc, hasRegLocState, rs, map);
    }
}

bool UnwindCore::IsFpMapNotExec(const std::shared_ptr<DfxMap>& map)
{
    return enableFpCheckMapExec_ && (map != nullptr) && (!map->IsMapExec());
}

bool UnwindCore::Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs)
{
    if (rs == nullptr || regs == nullptr) {
        return false;
    }
    uintptr_t prevPc = regs->GetPc();
    uintptr_t prevSp = regs->GetSp();
    uint16_t errCode = 0;
    bool ret = DfxInstructions::Apply(state_.memory, *(regs.get()), *(rs.get()), errCode);
    uintptr_t tmp = 0;
    uintptr_t sp = regs->GetSp();
    if (ret && (!state_.memory->Read<uintptr_t>(sp, &tmp, false))) {
        errCode = UNW_ERROR_UNREADABLE_SP;
        ret = false;
    }
    if (IsApplyRepeated(regs, prevPc, prevSp)) {
        errCode = UNW_ERROR_REPEATED_FRAME;
        ret = false;
    }
    if (!ret) {
        state_.lastErrorData.SetCode(errCode);
        DFXLOGU("Failed to apply reg state, errCode: %{public}d", static_cast<int>(errCode));
    }
    return ret;
}

bool UnwindCore::IsApplyRepeated(std::shared_ptr<DfxRegs> regs, uintptr_t prevPc, uintptr_t prevSp)
{
    return StripPac(regs->GetPc(), state_.pacMask) == prevPc && regs->GetSp() == prevSp;
}

void UnwindCore::DoPcAdjust(uintptr_t& pc)
{
    if (pc <= PC_ADJUST_MIN) {
        return;
    }
    uintptr_t sz = PC_ADJUST_DEFAULT;
#if defined(__arm__)
    if (NeedThumbAdjust(pc)) {
        sz = PC_ADJUST_ARM_THUMB;
    }
#elif defined(__x86_64__)
    sz = PC_ADJUST_X86;
#endif
    pc -= sz;
}

bool UnwindCore::NeedThumbAdjust(uintptr_t pc)
{
    return (pc & 0x1) && (state_.memory != nullptr) && !CanConfirmArmPc(pc);
}

bool UnwindCore::CanConfirmArmPc(uintptr_t pc)
{
    if (pc < PC_ADJUST_ARM_CHECK) {
        return false;
    }
    uintptr_t val = 0;
    if (!state_.memory->ReadMem(pc - PC_ADJUST_ARM_CHECK, &val)) {
        return false;
    }
    return (val & ARM_THUMB_MASK) == ARM_THUMB_MASK;
}

bool UnwindCore::GetLockInfo(int32_t tid, char* buf, size_t sz)
{
#ifdef __aarch64__
    const auto& frames = frameMgr_.GetFrames();
    if (frames.empty()) {
        return false;
    }
    if (!IsLockFrame(frames[0])) {
        return false;
    }
    return ReadLockContent(tid, buf, sz);
#else
    (void)tid;
    (void)buf;
    (void)sz;
    return false;
#endif
}

bool UnwindCore::IsLockFrame(const DfxFrame& frame)
{
    return frame.funcName.find("__timedwait_cp") != std::string::npos;
}

bool UnwindCore::ReadLockContent(int32_t tid, char* buf, size_t sz)
{
    uintptr_t lockPtrAddr = state_.firstFrameSp + LOCK_INFO_SP_OFFSET;
    uintptr_t lockAddr = 0;
    if (DfxMemory::ReadProcMemByPid(tid, lockPtrAddr, &lockAddr, sizeof(uintptr_t)) !=
        sizeof(uintptr_t)) {
        DFXLOGW("Failed to find lock addr.");
        return false;
    }
    size_t rsize = DfxMemory::ReadProcMemByPid(tid, lockAddr, buf, sz);
    if (rsize != sz) {
        DFXLOGW("Failed to fetch lock content, read size:%{public}zu expected size:%{public}zu",
            rsize, sz);
        return false;
    }
    return true;
}

bool UnwindCore::AccessMem(void* memory, uintptr_t addr, uintptr_t* val)
{
    return reinterpret_cast<DfxMemory*>(memory)->ReadMem(addr, val);
}

} // namespace HiviewDFX
} // namespace OHOS
