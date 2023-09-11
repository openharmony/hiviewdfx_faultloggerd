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
#ifndef DFX_DWARF_SECTION_H
#define DFX_DWARF_SECTION_H

#include <cinttypes>
#include <memory>
#include "dwarf_define.h"
#include "dfx_errors.h"
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DwarfSection {
public:
    DwarfSection(std::shared_ptr<DfxMemory> memory) : memory_(memory) {};
    virtual ~DwarfSection() = default;

    bool Step(uintptr_t fdeAddr, std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);

    const uint16_t& GetLastErrorCode() const { return lastErrorData_.code; }
    const uint64_t& GetLastErrorAddr() const { return lastErrorData_.addr; }

    void SetDataOffset(uintptr_t dataOffset) { dataOffset_ = dataOffset; }

protected:
    bool ParseFde(uintptr_t addr, FrameDescEntry &fde, CommonInfoEntry &cie);
    bool FillInFde(uintptr_t& addr, FrameDescEntry &fdeInfo, CommonInfoEntry &cieInfo);
    bool ParseCie(uintptr_t cieAddr, CommonInfoEntry &cieInfo);
    bool FillInCieHeader(uintptr_t& addr, CommonInfoEntry &cieInfo);
    bool FillInCie(uintptr_t& addr, CommonInfoEntry &cieInfo);

private:
    std::shared_ptr<DfxMemory> memory_;
    UnwindErrorData lastErrorData_;
    uint32_t cieIdValue_ = 0;
    uintptr_t dataOffset_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
