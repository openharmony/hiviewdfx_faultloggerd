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

#ifndef DFX_INSTRUCTIONS_H
#define DFX_INSTRUCTIONS_H

#include <cstdint>
#include <string>
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DfxInstructions {
public:
    DfxInstructions() = default;
    virtual ~DfxInstructions() = default;

    static bool Apply(std::shared_ptr<DfxMemory> memory, DfxRegs& regs, RegLocState& rsState);

private:
    static uintptr_t Flush(DfxRegs& dfxRegs, std::shared_ptr<DfxMemory> memory, uintptr_t cfa, RegLoc loc);
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
