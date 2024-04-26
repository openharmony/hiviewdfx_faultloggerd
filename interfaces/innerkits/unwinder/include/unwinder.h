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
#ifndef UNWINDER_H
#define UNWINDER_H

#include <memory>
#include <unordered_map>
#include <vector>
#include "dfx_accessors.h"
#include "dfx_define.h"
#include "dfx_errors.h"
#include "dfx_frame.h"
#include "dfx_memory.h"
#include "dfx_maps.h"
#include "dfx_param.h"
#include "dfx_regs.h"
#if defined(__arm__)
#include "arm_exidx.h"
#endif
#include "dwarf_section.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class Unwinder {
public:
    // for local
    Unwinder(bool needMaps = true);
    // for remote
    Unwinder(int pid, bool crash = true);
    Unwinder(int pid, int nspid, bool crash);
    // for customized
    Unwinder(std::shared_ptr<UnwindAccessors> accessors, bool local = false);
    ~Unwinder() { Destroy(); }

    inline void SetTargetPid(int pid) { pid_ = pid; }
    inline int32_t GetTargetPid() { return pid_; }
    inline void SetPacMask(uintptr_t mask) { pacMask_ = mask; }

    inline void EnableUnwindCache(bool enableCache) { enableCache_ = enableCache; }
    inline void EnableLrFallback(bool enableLrFallback) { enableLrFallback_ = enableLrFallback; }
    inline void EnableFpFallback(bool enableFpFallback) { enableFpFallback_ = enableFpFallback; }
    inline void EnableFpCheckMapExec(bool enableFpCheckMapExec) { enableFpCheckMapExec_ = enableFpCheckMapExec; }
    inline void EnableFillFrames(bool enableFillFrames) { enableFillFrames_ = enableFillFrames; }
    inline void IgnoreMixstack(bool ignoreMixstack) { ignoreMixstack_ = ignoreMixstack; }
    inline void StopWhenArkFrame(bool stopWhenArkFrame) { stopWhenArkFrame_ = stopWhenArkFrame; }

    inline void SetRegs(const std::shared_ptr<DfxRegs> regs) { regs_ = regs; }
    inline const std::shared_ptr<DfxRegs>& GetRegs() { return regs_; }
    inline void SetMaps(const std::shared_ptr<DfxMaps> maps) { maps_ = maps; }
    inline const std::shared_ptr<DfxMaps>& GetMaps() { return maps_; }

    inline const uint16_t& GetLastErrorCode() { return lastErrorData_.GetCode(); }
    inline const uint64_t& GetLastErrorAddr() { return lastErrorData_.GetAddr(); }

    bool GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);

    bool UnwindLocalWithContext(const ucontext_t& context, \
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindLocalWithTid(const pid_t tid, \
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindLocal(bool withRegs = false, bool fpUnwind = false, \
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindRemote(pid_t tid = 0, bool withRegs = false, \
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool Unwind(void *ctx, \
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);
    bool UnwindByFp(void *ctx, \
        size_t maxFrameNum = DEFAULT_MAX_FRAME_NUM, size_t skipFrameNum = 0);

    bool Step(uintptr_t& pc, uintptr_t& sp, void *ctx);
    bool FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx);

    void AddFrame(DfxFrame& frame);
    std::vector<DfxFrame>& GetFrames();
    inline const std::vector<uintptr_t>& GetPcs() { return pcs_; }
    void FillFrames(std::vector<DfxFrame>& frames);
    void FillFrame(DfxFrame& frame);
    void FillJsFrame(DfxFrame& frame);
    bool GetFrameByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps, DfxFrame& frame);
    void GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs);

    static bool GetSymbolByPc(uintptr_t pc, std::shared_ptr<DfxMaps> maps,
        std::string& funcName, uint64_t& funcOffset);
    static void GetLocalFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs);
    static std::string GetFramesStr(const std::vector<DfxFrame>& frames);
    static void FillLocalFrames(std::vector<DfxFrame>& frames);

    static bool AccessMem(void* memory, uintptr_t addr, uintptr_t *val)
    {
        return reinterpret_cast<DfxMemory*>(memory)->ReadMem(addr, val);
    }

private:
    struct StepFrame {
        uintptr_t pc = 0;
        uintptr_t methodid = 0;
        uintptr_t sp = 0;
        uintptr_t fp = 0;
        bool isJsFrame {false};
    };
    void Init();
    void Clear();
    void Destroy();
    void InitParam()
    {
#if defined(ENABLE_MIXSTACK)
        enableMixstack_ = DfxParam::EnableMixstack();
#endif
    }
    bool GetMainStackRangeInner(uintptr_t& stackBottom, uintptr_t& stackTop);
    bool CheckAndReset(void* ctx);
    void DoPcAdjust(uintptr_t& pc);
    void AddFrame(const StepFrame& frame, std::shared_ptr<DfxMap> map);
    bool StepInner(const bool isSigFrame, StepFrame& frame, void *ctx);
    bool Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);
#if defined(ENABLE_MIXSTACK)
    bool StepArkJsFrame(StepFrame& frame);
#endif
    static uintptr_t StripPac(uintptr_t inAddr, uintptr_t pacMask);
    static int DlPhdrCallback(struct dl_phdr_info *info, size_t size, void *data);
    inline void SetLocalStackCheck(void* ctx, bool check)
    {
        if ((pid_ == UNWIND_TYPE_LOCAL) && (ctx != nullptr)) {
            UnwindContext* uctx = reinterpret_cast<UnwindContext *>(ctx);
            uctx->stackCheck = check;
        }
    }

private:
#if defined(__aarch64__)
    MAYBE_UNUSED const uintptr_t pacMaskDefault_ = static_cast<uintptr_t>(0xFFFFFF8000000000);
#endif
    bool enableCache_ = true;
    bool enableFillFrames_ = true;
    bool enableLrFallback_ = true;
    bool enableFpFallback_ = true;
    bool enableFpCheckMapExec_ = false;
    bool isFpStep_ = false;
    MAYBE_UNUSED bool enableMixstack_ = true;
    MAYBE_UNUSED bool ignoreMixstack_ = false;
    MAYBE_UNUSED bool stopWhenArkFrame_ = false;

    int32_t pid_ = 0;
    uintptr_t pacMask_ = 0;
    std::shared_ptr<DfxAccessors> acc_ = nullptr;
    std::shared_ptr<DfxMemory> memory_ = nullptr;
    std::unordered_map<uintptr_t, std::shared_ptr<RegLocState>> rsCache_ {};
    std::shared_ptr<DfxRegs> regs_ = nullptr;
    std::shared_ptr<DfxMaps> maps_ = nullptr;
    std::vector<uintptr_t> pcs_ {};
    std::vector<DfxFrame> frames_ {};
    UnwindErrorData lastErrorData_ {};
#if defined(__arm__)
    std::shared_ptr<ArmExidx> armExidx_ = nullptr;
#endif
    std::shared_ptr<DwarfSection> dwarfSection_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
