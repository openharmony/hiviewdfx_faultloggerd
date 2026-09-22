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
#ifndef UNWINDER_SHARED_STATE_H
#define UNWINDER_SHARED_STATE_H

#include <memory>
#include <string>
#include <vector>

#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_maps.h"
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "unwind_context.h"
#include "unwind_define.h"
#include "unwind_entry_parser.h"
#include "unwind_entry_parser_factory.h"

namespace OHOS {
namespace HiviewDFX {

struct UnwinderSharedState {
    int32_t pid {0};
    UnwindType unwindType {UNWIND_TYPE_REMOTE};
    uintptr_t pacMask {0};
    uintptr_t firstFrameSp {0};
    bool isCrash {false};

    std::shared_ptr<DfxMemory> memory {nullptr};
    std::shared_ptr<DfxRegs> regs {nullptr};
    std::shared_ptr<DfxMaps> maps {nullptr};
    std::shared_ptr<UnwindEntryParser> unwindEntryParser {nullptr};
    UnwindErrorData lastErrorData {};
    UnwindContext context {};
    uintptr_t arkwebJsExtractorptr {0};
    bool isArkCreateLocal {false};
    uintptr_t interruptPc {0};
    uintptr_t bytecodePc {0};
    std::vector<uintptr_t> jitCache {};
    MAYBE_UNUSED bool enableMixstack {true};
    MAYBE_UNUSED bool ignoreMixstack {false};
    MAYBE_UNUSED bool stopWhenArkFrame {false};
    bool isJitCrash {false};

    bool IsLocal() const
    {
        return (unwindType == UNWIND_TYPE_LOCAL || unwindType == UNWIND_TYPE_CUSTOMIZE_LOCAL);
    }

    bool IsCustomize() const
    {
        return (unwindType == UNWIND_TYPE_CUSTOMIZE || unwindType == UNWIND_TYPE_CUSTOMIZE_LOCAL);
    }

    pid_t GetRealPid() const
    {
        return (pid <= 0) ? getpid() : pid;
    }
};

} // namespace HiviewDFX
} // namespace OHOS
#endif // UNWINDER_SHARED_STATE_H
