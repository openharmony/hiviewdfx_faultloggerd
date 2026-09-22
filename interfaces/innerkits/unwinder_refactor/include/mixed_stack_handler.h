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
#ifndef MIXED_STACK_HANDLER_H
#define MIXED_STACK_HANDLER_H

#include <cstdint>
#include <string>

#include "dfx_ark.h"
#include "dfx_jsvm.h"
#include "dfx_map.h"
#include "frame_manager.h"
#include "unwinder_shared_state.h"

namespace OHOS {
namespace HiviewDFX {

struct ArkMapRange {
    uintptr_t arkMapStart {0};
    uintptr_t arkMapEnd {0};
    uintptr_t staticArkMapStart {0};
    uintptr_t staticArkMapEnd {0};
};

class MixedStackHandler {
public:
    explicit MixedStackHandler(UnwinderSharedState& state) : state_(state)
    {}

    ~MixedStackHandler() = default;

    void SetEnableJsvmstack(bool enable)
    {
        enableJsvmstack_ = enable;
    }

    void SetEnableMixstack(bool enable)
    {
        state_.enableMixstack = enable;
    }

    void SetIgnoreMixstack(bool ignore)
    {
        state_.ignoreMixstack = ignore;
    }

    void SetStopWhenArkFrame(bool stop)
    {
        state_.stopWhenArkFrame = stop;
    }

    void SetIsJitCrashFlag(bool isCrash)
    {
        state_.isJitCrash = isCrash;
    }

    void SetBytecodePcAndInterruptPc();

    int ArkWriteJitCodeToFile(int fd);

    bool StepArkJsFrame(StepFrame& frame, uint64_t frameIndex = 0);
    bool UnwindArkFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map, bool& stopUnwind);
    bool StepV8Frame(StepFrame& frame, const std::shared_ptr<DfxMap>& map, bool& stopUnwind);

    int UnwindArkFrameByFp(StepFrame& frame, const ArkMapRange& arkMapRange, uint64_t& frameIndex);

    void SetInterruptPc(uintptr_t pc)
    {
        state_.interruptPc = pc;
    }

    uintptr_t GetInterruptPc() const
    {
        return state_.interruptPc;
    }

    uintptr_t GetBytecodePc() const
    {
        return state_.bytecodePc;
    }

private:
    bool ShouldStopByArkFrame(const StepFrame& frame, const std::shared_ptr<DfxMap>& map);
    bool StepArkFrameByType(StepFrame& frame, const std::shared_ptr<DfxMap>& map, bool& stopUnwind);
    bool IsStaticArkFrame(const StepFrame& frame, const std::shared_ptr<DfxMap>& map);
    bool IsDynamicArkFrame(const StepFrame& frame, const std::shared_ptr<DfxMap>& map);
    bool StepStaticArkFrame(StepFrame& frame, const std::shared_ptr<DfxMap>& map, bool& stopUnwind);
    bool StepDynamicArkFrame(StepFrame& frame, bool& stopUnwind);
    bool IsArkFrameForFp(const StepFrame& frame, const ArkMapRange& arkMapRange);
    bool IsStaticArkInterpreter(const StepFrame& frame, const ArkMapRange& arkMapRange);
    bool IsArkInterpreter(const StepFrame& frame, const ArkMapRange& arkMapRange);
    bool IsArkFrameType(const StepFrame& frame);
    int DoUnwindArkFrameByFp(StepFrame& frame, const ArkMapRange& arkMapRange, uint64_t& frameIndex);
    std::string MakeArkStepLog(const StepFrame& frame);
    void LogArkStepBegin(const StepFrame& frame);
    void LogArkStepEnd(const StepFrame& frame);
    int DoStepArkJsFrame(StepFrame& frame, uint64_t frameIndex);
    int DoStepArkFrameWithJit(StepFrame& frame, uint64_t frameIndex);
    int DoStepArkFrameNormal(StepFrame& frame, uint64_t frameIndex);
    bool NeedStepV8(const std::shared_ptr<DfxMap>& map);
    bool DoStepV8Frame(StepFrame& frame, bool& stopUnwind);

    UnwinderSharedState& state_;
    bool enableJsvmstack_ {false};
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // MIXED_STACK_HANDLER_H
