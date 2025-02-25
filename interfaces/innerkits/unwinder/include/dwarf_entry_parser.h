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
#ifndef DWARF_ENTRY_PARSER_H
#define DWARF_ENTRY_PARSER_H

#include <cinttypes>
#include <memory>
#include <unordered_map>
#include "dwarf_define.h"
#include "dfx_errors.h"
#include "dfx_memory.h"
#include "dfx_regs.h"
#include "unwind_context.h"
#include "unwind_entry_parser.h"

namespace OHOS {
namespace HiviewDFX {
class DwarfEntryParser : public UnwindEntryParser {
public:
    explicit DwarfEntryParser(const std::shared_ptr<DfxMemory>& memory) : UnwindEntryParser(memory) {}
    ~DwarfEntryParser() override = default;
    bool Step(uintptr_t pc, const UnwindTableInfo& uti, std::shared_ptr<RegLocState> rs) override;
protected:
    bool SearchEntry(uintptr_t pc, const UnwindTableInfo& uti, UnwindEntryInfo& uei) override;
    bool ParseFde(uintptr_t fdeAddr, uintptr_t fdePtr, FrameDescEntry& fdeInfo);
    bool ParseCie(uintptr_t cieAddr, uintptr_t ciePtr, CommonInfoEntry& cieInfo);
private:
    bool LinearSearchEntry(uintptr_t pc, const UnwindTableInfo& uti, UnwindEntryInfo& uei);
    bool GetCieOrFde(uintptr_t& addr, FrameDescEntry& fdeInfo);
    void ParseCieOrFdeHeader(uintptr_t& ptr, FrameDescEntry& fdeInfo, bool& isCieEntry);
    bool FillInFde(uintptr_t ptr, FrameDescEntry& fdeInfo);
    bool FillInCie(uintptr_t ptr, CommonInfoEntry& cieInfo);
    void ParseAugData(uintptr_t& ptr, CommonInfoEntry& cieInfo, const std::vector<char>& augStr);
    uint32_t cie32Value_ = 0;
    uint64_t cie64Value_ = 0;
    std::unordered_map<uintptr_t, FrameDescEntry> fdeEntries_;
    std::unordered_map<uintptr_t, CommonInfoEntry> cieEntries_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
