/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef UNWIND_ENTRY_PARSER_H
#define UNWIND_ENTRY_PARSER_H

#include <cinttypes>
#include <memory>
#include <securec.h>

#include "dfx_memory.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class UnwindEntryParser {
public:
    explicit UnwindEntryParser(const std::shared_ptr<DfxMemory>& memory) : memory_(memory) {}
    virtual ~UnwindEntryParser() = default;
    virtual bool Step(uintptr_t pc, const UnwindTableInfo& uti, std::shared_ptr<RegLocState> rs) = 0;
    uint16_t GetLastErrorCode() { return lastErrorData_.GetCode(); }
    uint64_t GetLastErrorAddr() { return lastErrorData_.GetAddr(); }
protected:
    virtual bool SearchEntry(uintptr_t pc, const UnwindTableInfo& uti, UnwindEntryInfo& uei) = 0;
    UnwindErrorData lastErrorData_ {};
    std::shared_ptr<DfxMemory> memory_ = nullptr;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
