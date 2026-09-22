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
#include "mixed_stack_handler.h"

#include <string>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_regs_get.h"
#include "dfx_trace_dlsym.h"
#include "dfx_util.h"
#include "elapsed_time.h"
#include "string_printf.h"
#include "unwind_define.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMixedStackHandler"

constexpr int32_t ELAPSED_TIME_LIMIT_MS = 20;
[[maybe_unused]] constexpr uintptr_t SENTINEL_SP = UINTPTR_MAX;
}

void MixedStackHandler::SetBytecodePcAndInterruptPc()
{
    state_.interruptPc = state_.regs->GetPc();
#if defined(__arm__)
    state_.bytecodePc = *state_.regs->GetReg(REG_ARM_R4);
#elif defined(__aarch64__)
    state_.bytecodePc = *state_.regs->GetReg(REG_AARCH64_X20);
#elif defined(__x86_64__)
    state_.bytecodePc = *state_.regs->GetReg(REG_X86_64_R10);
#endif
}

int MixedStackHandler::ArkWriteJitCodeToFile(int fd)
{
#if defined(ENABLE_MIXSTACK)
    return DfxArk::Instance().JitCodeWriteFile(state_.memory.get(),
        &(Unwinder::AccessMem), fd, state_.jitCache.data(), state_.jitCache.size());
#else
    (void)fd;
    return -1;
#endif
}

bool MixedStackHandler::StepArkJsFrame(StepFrame& frame, uint64_t frameIndex)
{
#if defined(ENABLE_MIXSTACK)
    DFX_TRACE_SCOPED_DLSYM("StepArkJsFrame pc: %p", reinterpret_cast<void*>(frame.pc));
    ElapsedTime counter(MakeArkStepLog(frame), ELAPSED_TIME_LIMIT_MS);
    LogArkStepBegin(frame);
    int ret = DoStepArkJsFrame(frame, frameIndex);
    if (ret < 0) {
        DFXLOGE("Failed to step ark frame");
        return false;
    }
    LogArkStepEnd(frame);
    state_.regs->SetPc(StripPac(frame.pc, state_.pacMask));
    state_.regs->SetSp(frame.sp);
    state_.regs->SetFp(frame.fp);
    return true;
#else
    (void)frame;
    (void)frameIndex;
    return false;
#endif
}

std::string MixedStackHandler::MakeArkStepLog(const StepFrame& frame)
{
    return "StepArkJsFrame, ark pc: " + std::to_string(frame.pc) +
        ", fp:" + std::to_string(frame.fp) + ", sp:" + std::to_string(frame.sp) +
        ", isJsFrame:" + std::to_string(frame.isJsFrame);
}

void MixedStackHandler::LogArkStepBegin(const StepFrame& frame)
{
    if (!state_.IsCustomize()) {
        DFXLOGD("+++ark pc: %{private}p, fp: %{private}p, sp: %{private}p, isJsFrame: %{public}d.",
            reinterpret_cast<void*>(frame.pc),
            reinterpret_cast<void*>(frame.fp), reinterpret_cast<void*>(frame.sp), frame.isJsFrame);
    }
}

void MixedStackHandler::LogArkStepEnd(const StepFrame& frame)
{
    if (!state_.IsCustomize()) {
        DFXLOGD("---ark pc: %{private}p, fp: %{private}p, sp: %{private}p, isJsFrame: %{public}d.",
            reinterpret_cast<void*>(frame.pc),
            reinterpret_cast<void*>(frame.fp), reinterpret_cast<void*>(frame.sp), frame.isJsFrame);
    }
}

int MixedStackHandler::DoStepArkJsFrame(StepFrame& frame, uint64_t frameIndex)
{
    if (state_.isJitCrash) {
        return DoStepArkFrameWithJit(frame, frameIndex);
    }
    return DoStepArkFrameNormal(frame, frameIndex);
}

int MixedStackHandler::DoStepArkFrameWithJit(StepFrame& frame, uint64_t frameIndex)
{
    MAYBE_UNUSED uintptr_t methodId = 0;
    ArkUnwindParam arkParam(state_.memory.get(), &(Unwinder::AccessMem),
        &frame.fp, &frame.sp, &frame.pc, &methodId, &frame.isJsFrame,
        &frame.frameType, frameIndex, state_.jitCache);
    return DfxArk::Instance().StepArkFrameWithJit(&arkParam);
}

int MixedStackHandler::DoStepArkFrameNormal(StepFrame& frame, uint64_t frameIndex)
{
    uintptr_t stepSp = state_.interruptPc != frame.pc ? frame.sp : SENTINEL_SP;
    uintptr_t prevSp = stepSp;
    frame.pc = state_.bytecodePc;
    ArkStepParam arkParam(&frame.fp, &stepSp, &frame.pc, &frame.isJsFrame,
        &frame.frameType, frameIndex);
    int ret = DfxArk::Instance().StepArkFrame(state_.memory.get(),
        &(Unwinder::AccessMem), &arkParam);
    frame.sp = stepSp == prevSp ? frame.sp : stepSp;
    return ret;
}

bool MixedStackHandler::UnwindArkFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    bool& stopUnwind)
{
#if defined(ENABLE_MIXSTACK)
    if (ShouldStopByArkFrame(frame, map)) {
        DFXLOGU("Stop by ark frame");
        stopUnwind = true;
        return false;
    }
    if (!state_.enableMixstack || map == nullptr) {
        return true;
    }
    return StepArkFrameByType(frame, map, stopUnwind);
#else
    (void)frame;
    (void)map;
    (void)stopUnwind;
    return true;
#endif
}

bool MixedStackHandler::ShouldStopByArkFrame(const StepFrame& frame,
    const std::shared_ptr<DfxMap>& map)
{
    if (!state_.stopWhenArkFrame || map == nullptr) {
        return false;
    }
    return map->IsArkExecutable() || map->IsStaticArkExecutable(frame.pc);
}

bool MixedStackHandler::StepArkFrameByType(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    bool& stopUnwind)
{
    if (IsStaticArkFrame(frame, map)) {
        return StepStaticArkFrame(frame, map, stopUnwind);
    }
    if (IsDynamicArkFrame(frame, map)) {
        return StepDynamicArkFrame(frame, stopUnwind);
    }
    return true;
}

bool MixedStackHandler::IsStaticArkFrame(const StepFrame& frame,
    const std::shared_ptr<DfxMap>& map)
{
    return map->IsStaticArkExecutable(frame.pc) ||
        frame.frameType == FrameType::STATIC_JS_FRAME;
}

bool MixedStackHandler::IsDynamicArkFrame(const StepFrame& frame,
    const std::shared_ptr<DfxMap>& map)
{
    return map->IsArkExecutable() || frame.frameType == FrameType::JS_FRAME;
}

bool MixedStackHandler::StepStaticArkFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    bool& stopUnwind)
{
    uint64_t frameIndex = 0;
    if (map->IsStaticArkExecutable(frame.pc)) {
        frame.frameType = FrameType::STATIC_JS_FRAME;
    }
    if (!StepArkJsFrame(frame, frameIndex)) {
        DFXLOGE("Failed to step static ark Js frame, pc: %{private}p",
            reinterpret_cast<void*>(frame.pc));
        return false;
    }
    stopUnwind = true;
    return true;
}

bool MixedStackHandler::StepDynamicArkFrame(StepFrame& frame, bool& stopUnwind)
{
    if (!StepArkJsFrame(frame)) {
        DFXLOGE("Failed to step ark Js frame, pc: %{private}p",
            reinterpret_cast<void*>(frame.pc));
        state_.lastErrorData.SetAddrAndCode(frame.pc, UNW_ERROR_STEP_ARK_FRAME);
        return false;
    }
    stopUnwind = true;
    return true;
}

bool MixedStackHandler::StepV8Frame(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
    bool& stopUnwind)
{
    if (map == nullptr) {
        return false;
    }
    if (!NeedStepV8(map)) {
        stopUnwind = false;
        return true;
    }
    return DoStepV8Frame(frame, stopUnwind);
}

bool MixedStackHandler::NeedStepV8(const std::shared_ptr<DfxMap>& map)
{
    bool needStepJsvmStack = enableJsvmstack_ && (map->IsJsvmExecutable());
    bool needStepArkwebStack = map->IsArkWebJsExecutable();
    return needStepJsvmStack || needStepArkwebStack;
}

bool MixedStackHandler::DoStepV8Frame(StepFrame& frame, bool& stopUnwind)
{
    DFX_TRACE_SCOPED_DLSYM("StepV8Frame pc: %p", reinterpret_cast<void*>(frame.pc));
    std::string timeLimitCheck = "StepV8Frame, pc: " + std::to_string(frame.pc) +
        ", fp:" + std::to_string(frame.fp) + ", sp:" + std::to_string(frame.sp) +
        ", frameType: " + std::to_string(static_cast<uint8_t>(frame.frameType));
    ElapsedTime counter(std::move(timeLimitCheck), ELAPSED_TIME_LIMIT_MS);
    JsvmStepParam jsvmParam(&frame.fp, &frame.sp, &frame.pc, &frame.isJsFrame);
    if (DfxJsvm::Instance().StepJsvmFrame(state_.memory.get(),
        &(Unwinder::AccessMem), &jsvmParam) < 0) {
        DFXLOGE("Failed to step jsvm frame");
        return false;
    }
    state_.regs->SetPc(StripPac(frame.pc, state_.pacMask));
    state_.regs->SetSp(frame.sp);
    state_.regs->SetFp(frame.fp);
    stopUnwind = true;
    return true;
}

int MixedStackHandler::UnwindArkFrameByFp(StepFrame& frame, const ArkMapRange& arkMapRange,
    uint64_t& frameIndex)
{
#if defined(ENABLE_MIXSTACK)
    if (!IsArkFrameForFp(frame, arkMapRange)) {
        return 1;
    }
    return DoUnwindArkFrameByFp(frame, arkMapRange, frameIndex);
#else
    (void)frame;
    (void)arkMapRange;
    (void)frameIndex;
    return 1;
#endif
}

bool MixedStackHandler::IsArkFrameForFp(const StepFrame& frame, const ArkMapRange& arkMapRange)
{
    return IsStaticArkInterpreter(frame, arkMapRange) ||
        IsArkInterpreter(frame, arkMapRange) || IsArkFrameType(frame);
}

bool MixedStackHandler::IsStaticArkInterpreter(const StepFrame& frame,
    const ArkMapRange& arkMapRange)
{
    return (frame.pc >= arkMapRange.staticArkMapStart) &&
        (frame.pc < arkMapRange.staticArkMapEnd);
}

bool MixedStackHandler::IsArkInterpreter(const StepFrame& frame, const ArkMapRange& arkMapRange)
{
    return (frame.pc >= arkMapRange.arkMapStart) && (frame.pc < arkMapRange.arkMapEnd);
}

bool MixedStackHandler::IsArkFrameType(const StepFrame& frame)
{
    return (frame.frameType == FrameType::STATIC_JS_FRAME) ||
        (frame.frameType == FrameType::JS_FRAME);
}

int MixedStackHandler::DoUnwindArkFrameByFp(StepFrame& frame, const ArkMapRange& arkMapRange,
    uint64_t& frameIndex)
{
    if (IsStaticArkInterpreter(frame, arkMapRange)) {
        frame.frameType = FrameType::STATIC_JS_FRAME;
        frameIndex = 0;
    }
    if (!StepArkJsFrame(frame, frameIndex)) {
        DFXLOGE("Failed to step ark Js frame, pc: %{private}p",
            reinterpret_cast<void*>(frame.pc));
        state_.lastErrorData.SetAddrAndCode(frame.pc, UNW_ERROR_STEP_ARK_FRAME);
        return -1;
    }
    frameIndex++;
    return 0;
}

} // namespace HiviewDFX
} // namespace OHOS
