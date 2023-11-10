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
#ifndef UNWINDER_H
#define UNWINDER_H

#include <memory>
#include <unordered_map>
#include <vector>
#include "dfx_accessors.h"
#include "dfx_errors.h"
#include "dfx_frame.h"
#include "dfx_memory.h"
#include "dfx_maps.h"
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
    Unwinder() : pid_(UNWIND_TYPE_LOCAL)
    {
        acc_ = std::make_shared<DfxAccessorsLocal>();
        Init();
    };
    // for remote
    Unwinder(int pid) : pid_(pid)
    {
        acc_ = std::make_shared<DfxAccessorsRemote>();
        Init();
    };
    // for customized
    Unwinder(std::shared_ptr<UnwindAccessors> accessors) : pid_(UNWIND_TYPE_CUSTOMIZE)
    {
        acc_ = std::make_shared<DfxAccessorsCustomize>(accessors);
#if defined(__aarch64__)
        pacMask_ = pacMaskDefault_;
#endif
        Init();
    };
    ~Unwinder() { Clear(); }

    inline UnwindMode GetUnwindMode() { return mode_; }
    inline void SetUnwindMode(UnwindMode mode) { mode_ = mode; }

    inline void SetTargetPid(int pid) { pid_ = pid; }
    inline int32_t GetTargetPid() { return pid_; }
    inline void SetPacMask(uintptr_t mask) { pacMask_ = mask; }

    inline void SetRegs(std::shared_ptr<DfxRegs> regs) { regs_ = regs; }
    inline const std::shared_ptr<DfxRegs>& GetRegs() { return regs_; }
    inline const std::shared_ptr<DfxMaps>& GetMaps() { return maps_; }

    const uint16_t& GetLastErrorCode() const { return lastErrorData_.code; }
    const uint64_t& GetLastErrorAddr() const { return lastErrorData_.addr; }

    bool GetStackRange(uintptr_t& stackBottom, uintptr_t& stackTop);
    bool GetMap(uintptr_t pc, void *ctx, std::shared_ptr<DfxMap>& map);

    bool UnwindLocal(size_t maxFrameNum = 64, size_t skipFrameNum = 0);
    bool UnwindRemote(size_t maxFrameNum = 64, size_t skipFrameNum = 0);
    bool Unwind(void *ctx, size_t maxFrameNum = 64, size_t skipFrameNum = 0);
    bool Step(uintptr_t& pc, uintptr_t& sp, void *ctx);
    bool FpStep(uintptr_t& fp, uintptr_t& pc, void *ctx);

    const std::vector<uintptr_t>& GetPcs() { return pcs_; }
    void FillFrames(std::vector<DfxFrame>& frames);
    const std::vector<DfxFrame>& GetFrames();
    static void GetFramesByPcs(std::vector<DfxFrame>& frames, std::vector<uintptr_t> pcs,
        std::shared_ptr<DfxMaps> maps);
    static void FillFrame(DfxFrame& frame);
    static std::string GetFramesStr(const std::vector<DfxFrame>& frames);

private:
    void Init();
    void Clear();
    void DoPcAdjust(uintptr_t& pc);
    bool Apply(std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);
    static uintptr_t StripPac(uintptr_t inAddr, uintptr_t pacMask);

private:
#if defined(__aarch64__)
    MAYBE_UNUSED const uintptr_t pacMaskDefault_ = static_cast<uintptr_t>(0xFFFFFF8000000000);
#endif
    int32_t pid_ = 0;
    uintptr_t pacMask_ = 0;
    UnwindMode mode_ = DWARF_UNWIND;
    std::shared_ptr<DfxAccessors> acc_;
    std::shared_ptr<DfxMemory> memory_;
    std::unordered_map<uintptr_t, std::shared_ptr<RegLocState>> rsCache_;
    std::shared_ptr<DfxRegs> regs_ = nullptr;
    std::shared_ptr<DfxMaps> maps_ = nullptr;
    std::vector<uintptr_t> pcs_;
    std::vector<DfxFrame> frames_;
    UnwindErrorData lastErrorData_;
#if defined(__arm__)
    std::shared_ptr<ArmExidx> armExidx_;
#endif
    std::shared_ptr<DwarfSection> dwarfSection_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
