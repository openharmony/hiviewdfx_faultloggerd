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

#ifndef DFX_DWARF_INSTRUCTIONS_H
#define DFX_DWARF_INSTRUCTIONS_H

#include <cinttypes>
#include <memory>
#include <stack>
#include "dwarf_define.h"
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DwarfCfaInstructions {
public:
    DwarfCfaInstructions(std::shared_ptr<DfxMemory> memory) : memory_(memory) {};
    virtual ~DwarfCfaInstructions() = default;

    bool Parse(uintptr_t pc, FrameDescEntry fde, RegLocState& rsState);

private:
    bool Iterate(uintptr_t pc, FrameDescEntry fde, uintptr_t instStart, uintptr_t instEnd, RegLocState &rsState);

private:
    std::shared_ptr<DfxMemory> memory_;
    std::stack<RegLocState> saveRsStates_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
