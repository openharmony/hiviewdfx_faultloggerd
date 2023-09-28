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
#include <unordered_map>
#include "dwarf_define.h"
#include "dfx_errors.h"
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DwarfSection {
public:
    DwarfSection(std::shared_ptr<DfxMemory> memory) : memory_(memory)
    {
        lastErrorData_.code = UNW_ERROR_NONE;
        lastErrorData_.addr = 0;
    }
    virtual ~DwarfSection() = default;

    bool SearchEntry(struct UnwindEntryInfo& pi, struct UnwindTableInfo uti, uintptr_t pc);
    bool Step(uintptr_t fdeAddr, std::shared_ptr<DfxRegs> regs, std::shared_ptr<RegLocState> rs);

    const uint16_t& GetLastErrorCode() const { return lastErrorData_.code; }
    const uint64_t& GetLastErrorAddr() const { return lastErrorData_.addr; }

protected:
    bool ParseFde(uintptr_t addr, FrameDescEntry &fde);
    bool FillInFdeHeader(uintptr_t& ptr, FrameDescEntry &fdeInfo);
    bool FillInFde(uintptr_t& ptr, FrameDescEntry &fdeInfo);
    bool ParseCie(uintptr_t cieAddr, CommonInfoEntry &cieInfo);
    bool FillInCieHeader(uintptr_t& ptr, CommonInfoEntry &cieInfo);
    bool FillInCie(uintptr_t& ptr, CommonInfoEntry &cieInfo);

protected:
    std::shared_ptr<DfxMemory> memory_;
    UnwindErrorData lastErrorData_;
    uint32_t cie32Value_ = 0;
    uint64_t cie64Value_ = 0;
private:
    std::unordered_map<uintptr_t, FrameDescEntry> fdeEntries_;
    std::unordered_map<uintptr_t, CommonInfoEntry> cieEntries_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
