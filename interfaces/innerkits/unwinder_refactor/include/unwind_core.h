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
#ifndef UNWIND_CORE_H
#define UNWIND_CORE_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>

#include "dfx_memory.h"
#include "dfx_regs.h"
#include "frame_manager.h"
#include "mixed_stack_handler.h"
#include "stack_utils.h"
#include "unwind_context.h"
#include "unwind_define.h"
#include "unwinder_shared_state.h"

namespace OHOS {
namespace HiviewDFX {

class UnwindCore {
public:
    struct StepCache {
        std::shared_ptr<DfxMap> map {nullptr};
        std::shared_ptr<RegLocState> rs {nullptr};
    };

    explicit UnwindCore(UnwinderSharedState& state, FrameManager& frameMgr,
        MixedStackHandler& mixedStack);
    ~UnwindCore() = default;

    void SetEnableLrFallback(bool enable)
    {
        enableLrFallback_ = enable;
    }

    void SetEnableFpCheckMapExec(bool enable)
    {
        enableFpCheckMapExec_ = enable;
    }

    bool IsFpStep() const
    {
        return isFpStep_;
    }

    void Clear()
    {
        isFpStep_ = false;
        isResetFrames_ = false;
    }

    void SetCacheEnabled(bool enabled)
    {
        cacheEnabled_ = enabled;
    }

    bool IsCacheEnabled() const
    {
        return cacheEnabled_;
    }

    bool FindCache(uintptr_t pc, std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs);

    void UpdateCache(uintptr_t pc, bool hasRegLocState,
        const std::shared_ptr<RegLocState>& rs, const std::shared_ptr<DfxMap>& map);

    void ClearCache()
    {
        cacheMap_.clear();
    }

    bool GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);
    bool GetMainStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);
    bool GetArkStackRange(uintptr_t& arkMapStart, uintptr_t& arkMapEnd);
    bool GetStaticArkStackRange(uintptr_t& arkMapStart, uintptr_t& arkMapEnd);

    bool Unwind(void* ctx, size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindByFp(void* ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk);
    bool Step(uintptr_t& pc, uintptr_t& sp, void* ctx);
    bool FpStep(uintptr_t& fp, uintptr_t& pc, void* ctx);
    bool UnwindFrame(void* ctx, StepFrame& frame, bool& needAdjustPc);

    bool GetLockInfo(int32_t tid, char* buf, size_t sz);

    static bool AccessMem(void* memory, uintptr_t addr, uintptr_t* val);

private:
    bool CheckAndReset(void* ctx);
    void DoPcAdjust(uintptr_t& pc);
    bool StepInner(bool isSigFrame, StepFrame& frame, void* ctx);
    bool Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);
    void UpdateRegsState(StepFrame& frame, void* ctx, bool& unwinderResult,
        std::shared_ptr<RegLocState>& rs);
    void StepToNextFpIfNeed();
    bool CheckFrameValid(const StepFrame& frame, const std::shared_ptr<DfxMap>& map, uintptr_t prevSp);
    bool GetCrashLastFrame(StepFrame& frame);
    bool ParseUnwindTable(uintptr_t pc, std::shared_ptr<RegLocState>& rs);
    bool AddFrameMap(const StepFrame& frame, std::shared_ptr<DfxMap>& map);
    void SetLocalStackCheck(void* ctx, bool check) const;

    bool UnwindFrames(void* ctx, size_t maxFrameNum, size_t skipFrameNum);
    bool ShouldResetFrames(size_t skipFrameNum);
    bool IsMaxFramesReached(size_t maxFrameNum);

    bool UnwindByFpLoop(void* ctx, size_t maxFrameNum, size_t skipFrameNum,
        const ArkMapRange& arkMapRange, bool enableArk);
    void HandleResetPcs(bool& resetFrames, size_t skipFrameNum);
    bool ShouldResetPcs(bool& resetFrames, size_t skipFrameNum);
    bool IsMaxPcsReached(size_t maxFrameNum);
    bool FillFpFrameAndStep(StepFrame& frame, void* ctx, const ArkMapRange& arkMapRange,
        bool enableArk, bool& needAdjustPc, uint64_t frameIndex);
    void AdjustFpPcIfNeeded(StepFrame& frame, bool needAdjustPc);
    int CheckArkFrameByFp(StepFrame& frame, const ArkMapRange& arkMapRange,
        bool enableArk, uint64_t& frameIndex);
    bool IsFpStepTerminated(StepFrame& frame, void* ctx);

    bool DoFpStep(uintptr_t& fp, uintptr_t& pc);
    bool ReadFpAndPc(uintptr_t ptr, uintptr_t& fp, uintptr_t& pc);
    void UpdateFpRegs(uintptr_t fp, uintptr_t pc, uintptr_t prevFp);

    bool IsSignalFrameStep(const StepFrame& frame);
    void AdjustFramePc(StepFrame& frame, bool needAdjustPc);
    bool IsRepeatedFrame(const StepFrame& frame, uintptr_t prevPc, uintptr_t prevSp);
    void LogRepeatedFrame(void* ctx);

    bool ApplyWithLrFallback(std::shared_ptr<RegLocState>& rs);
    bool NeedLrFallback(bool applyResult);
    void LogLrFallback();
    bool IsFirstFrameLrFallback();
    void TryLrFallbackOnStepFail(bool& unwinderResult);
    void LogStepFailLrFallback();
    void TryFpStep(StepFrame& frame, void* ctx, bool& unwinderResult);
    void LogFirstFpStep(uintptr_t pc);

    bool CheckStepParams(void* ctx);
    void ProcessStepFrame(bool isSigFrame, StepFrame& frame, void* ctx,
        std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs,
        bool& hasRegLocState, bool& finished, bool& result);
    void StepInnerNoCache(bool isSigFrame, StepFrame& frame, void* ctx,
        std::shared_ptr<DfxMap>& map, std::shared_ptr<RegLocState>& rs,
        bool& hasRegLocState, bool& finished, bool& result);
    void HandleMixedStackStep(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
        std::shared_ptr<RegLocState>& rs, bool& hasRegLocState, void* ctx,
        bool& finished, bool& result);
    void HandleArkFrameAndParse(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
        std::shared_ptr<RegLocState>& rs, bool& hasRegLocState, void* ctx,
        bool& finished, bool& result);
    void ParseTableOrCheckFp(StepFrame& frame, const std::shared_ptr<DfxMap>& map,
        std::shared_ptr<RegLocState>& rs, bool& hasRegLocState,
        bool& finished, bool& result);
    bool IsFpMapNotExec(const std::shared_ptr<DfxMap>& map);

    bool IsApplyRepeated(std::shared_ptr<DfxRegs> regs, uintptr_t prevPc, uintptr_t prevSp);

    bool NeedThumbAdjust(uintptr_t pc);
    bool CanConfirmArmPc(uintptr_t pc);

    bool IsIllegalSpValue(const StepFrame& frame, const std::shared_ptr<DfxMap>& map,
        uintptr_t prevSp);

    bool IsLockFrame(const DfxFrame& frame);
    bool ReadLockContent(int32_t tid, char* buf, size_t sz);

    UnwinderSharedState& state_;
    FrameManager& frameMgr_;
    MixedStackHandler& mixedStack_;

    bool enableLrFallback_ {true};
    bool enableFpCheckMapExec_ {false};
    bool isFpStep_ {false};
    bool isResetFrames_ {false};

    bool cacheEnabled_ {true};
    std::unordered_map<uintptr_t, StepCache> cacheMap_ {};
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // UNWIND_CORE_H
