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
#include <vector>
#include "dfx_accessors.h"
#include "dfx_frame.h"
#include "dfx_memory.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class Unwinder {
public:
    // for local
    Unwinder() : pid_(UWNIND_TYPE_LOCAL)
    {
        acc_ = new DfxAccessorsLocal();
        memory_ = new DfxMemory(acc_);
        Init();
    };
    // for remote
    Unwinder(int pid) : pid_(pid)
    {
        acc_ = new DfxAccessorsRemote();
        memory_ = new DfxMemory(acc_);
        Init();
    };
    // for customized
    Unwinder(std::shared_ptr<UnwindAccessors> accessors) : pid_(UWNIND_TYPE_CUSTOMIZE)
    {
        acc_ = new DfxAccessorsCustomize(accessors);
        memory_ = new DfxMemory(acc_);
        Init();
    };
    ~Unwinder() { Destroy(); }

    inline UnwindMode GetUnwindMode() { return mode_; }
    inline void SetUnwindMode(UnwindMode mode) { mode_ = mode; }

    inline void SetTargetPid(int pid) { pid_ = pid; }
    inline int32_t GetTargetPid() { return pid_; }

    inline void SetRegs(std::shared_ptr<DfxRegs> regs) { regs_ = regs; }
    inline const std::shared_ptr<DfxRegs>& GetRegs() { return regs_; }

    const std::vector<uintptr_t>& GetPcs() { return pcs_; }
    const std::vector<DfxFrame>& GetFrames() { return frames_; }
    const uint16_t& GetLastErrorCode() const { return lastErrorData_.code; }
    const uint64_t& GetLastErrorAddr() const { return lastErrorData_.addr; }

    bool Unwind(void *ctx, size_t maxFrameNum = 64, size_t skipFrameNum = 0);
    bool Step(uintptr_t pc, uintptr_t sp, void *ctx);

    bool UnwindLocal(size_t maxFrameNum, size_t skipFrameNum);
    bool UnwindRemote(size_t maxFrameNum, size_t skipFrameNum);

private:
    void Init();
    void Destroy();
    bool IsValidFrame(uintptr_t addr, uintptr_t stackTop, uintptr_t stackBottom);

private:
    int32_t pid_;
    UnwindMode mode_ = DWARF_UNWIND;
    uintptr_t stackBottom_;
    uintptr_t stackTop_;
    DfxAccessors* acc_;
    DfxMemory* memory_;
    std::shared_ptr<DfxRegs> regs_;
    std::shared_ptr<DfxMaps> maps_;
    std::shared_ptr<DfxElf> elf_;
    std::vector<uintptr_t> pcs_;
    std::vector<DfxFrame> frames_;
    UnwindErrorData lastErrorData_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
