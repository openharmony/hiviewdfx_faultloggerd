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
#ifndef UNWINDER_BASE_H
#define UNWINDER_BASE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "dfx_frame.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "frame_manager.h"
#include "mixed_stack_handler.h"
#include "unwind_context.h"
#include "unwind_core.h"
#include "unwind_define.h"
#include "unwinder_shared_state.h"

namespace OHOS {
namespace HiviewDFX {

class UnwinderBase {
public:
    virtual ~UnwinderBase();

    UnwinderBase(const UnwinderBase&) = delete;
    UnwinderBase& operator=(const UnwinderBase&) = delete;
    UnwinderBase(UnwinderBase&&) = delete;
    UnwinderBase& operator=(UnwinderBase&&) = delete;

    void InitCommon(std::shared_ptr<UnwindAccessors> accessors = nullptr);
    void EnableUnwindCache(bool enableCache)
    {
        unwindCore_.SetCacheEnabled(enableCache);
    }

    void EnableFpCheckMapExec(bool enable)
    {
        enableFpCheckMapExec_ = enable;
        unwindCore_.SetEnableFpCheckMapExec(enable);
    }

    void EnableFillFrames(bool enable)
    {
        frameMgr_.SetEnableFillFrames(enable);
    }

    void EnableParseNativeSymbol(bool enable)
    {
        frameMgr_.SetEnableParseNativeSymbol(enable);
    }

    void EnableJsvmstack(bool enable)
    {
        mixedStack_.SetEnableJsvmstack(enable);
    }

    void IgnoreMixstack(bool ignore)
    {
        mixedStack_.SetIgnoreMixstack(ignore);
    }

    void SetRegs(std::shared_ptr<DfxRegs> regs)
    {
        if (regs == nullptr) {
            return;
        }
        state_->regs = regs;
        state_->firstFrameSp = regs->GetSp();
    }

    void SetMaps(std::shared_ptr<DfxMaps> maps)
    {
        if (maps == nullptr) {
            return;
        }
        state_->maps = maps;
    }

    const std::shared_ptr<DfxRegs>& GetRegs() const
    {
        return state_->regs;
    }

    const std::shared_ptr<DfxMaps>& GetMaps() const
    {
        return state_->maps;
    }

    uint16_t GetLastErrorCode() const
    {
        return state_->lastErrorData.GetCode();
    }

    uint64_t GetLastErrorAddr() const
    {
        return state_->lastErrorData.GetAddr();
    }

    bool GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop)
    {
        return unwindCore_.GetStackRange(stackBottom, stackTop);
    }

    bool Unwind(void* ctx, size_t maxFrameNum, size_t skipFrameNum)
    {
        return unwindCore_.Unwind(ctx, maxFrameNum, skipFrameNum);
    }

    bool UnwindByFp(void* ctx, size_t maxFrameNum, size_t skipFrameNum, bool enableArk)
    {
        return unwindCore_.UnwindByFp(ctx, maxFrameNum, skipFrameNum, enableArk);
    }

    bool Step(uintptr_t& pc, uintptr_t& sp, void* ctx)
    {
        return unwindCore_.Step(pc, sp, ctx);
    }

    bool FpStep(uintptr_t& fp, uintptr_t& pc, void* ctx)
    {
        return unwindCore_.FpStep(fp, pc, ctx);
    }

    void AddFrame(DfxFrame& frame)
    {
        frameMgr_.AddFrame(frame);
    }

    const std::vector<DfxFrame>& GetFrames()
    {
        return frameMgr_.GetFrames();
    }

    const std::vector<uintptr_t>& GetPcs() const
    {
        return frameMgr_.GetPcs();
    }

    void FillFrames(std::vector<DfxFrame>& frames)
    {
        frameMgr_.FillFrames(frames);
    }

    void FillFrame(DfxFrame& frame, bool needSymParse)
    {
        frameMgr_.FillFrame(frame, needSymParse);
    }

    void ParseFrameSymbol(DfxFrame& frame)
    {
        frameMgr_.ParseFrameSymbol(frame);
    }

    void FillJsFrame(DfxFrame& frame)
    {
        frameMgr_.FillJsFrame(frame);
    }

    bool GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame)
    {
        return frameMgr_.GetFrameByPc(pc, maps, frame);
    }

    void GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
    {
        frameMgr_.GetFramesByPcs(frames, std::move(pcs));
    }

    virtual void GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs)
    {}

    virtual void FillLocalFrames(std::vector<DfxFrame>& frames)
    {}

    bool GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps,
        std::string& funcName, uint64_t& funcOffset)
    {
        return frameMgr_.GetSymbolByPc(pc, maps, funcName, funcOffset);
    }

    void SetIsJitCrashFlag(bool isCrash)
    {
        mixedStack_.SetIsJitCrashFlag(isCrash);
    }

    int ArkWriteJitCodeToFile(int fd)
    {
        return mixedStack_.ArkWriteJitCodeToFile(fd);
    }

    const std::vector<uintptr_t>& GetJitCache() const
    {
        return state_->jitCache;
    }

    bool GetLockInfo(int32_t tid, char* buf, size_t sz)
    {
        return unwindCore_.GetLockInfo(tid, buf, sz);
    }

    void SetFrames(std::vector<DfxFrame>& frames)
    {
        frameMgr_.SetFrames(frames);
    }

    virtual bool DoUnwindLocalWithContext(const ucontext_t&, size_t, size_t)
    {
        return false;
    }

    virtual bool DoUnwindLocalWithTid(const pid_t, size_t, size_t)
    {
        return false;
    }

    virtual bool DoUnwindLocalByOtherTid(const pid_t, bool, size_t, size_t)
    {
        return false;
    }

    virtual bool DoUnwindLocal(bool, bool, size_t, size_t, bool)
    {
        return false;
    }

    virtual bool DoUnwindRemote(pid_t, bool, size_t, size_t)
    {
        return false;
    }

protected:
    explicit UnwinderBase(std::shared_ptr<UnwinderSharedState> state)
        : state_(state),
          frameMgr_(*state_),
          mixedStack_(*state_),
          unwindCore_(*state_, frameMgr_, mixedStack_)
    {}

    virtual void OnDestroy()
    {}

    std::shared_ptr<UnwinderSharedState> state_;
    FrameManager frameMgr_;
    MixedStackHandler mixedStack_;
    UnwindCore unwindCore_;
    bool enableFpCheckMapExec_ {false};

#if defined(__aarch64__)
    static constexpr uintptr_t pacMaskDefault_ = static_cast<uintptr_t>(0xFFFFFF8000000000);
#endif
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // UNWINDER_BASE_H
