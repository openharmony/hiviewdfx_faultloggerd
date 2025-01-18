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
#ifndef UNWIND_ENTRY_PARSER_FACTORY_H
#define UNWIND_ENTRY_PARSER_FACTORY_H

#include <cinttypes>
#include <memory>

#include "dwarf_entry_parser.h"
#include "exidx_entry_parser.h"
namespace OHOS {
namespace HiviewDFX {
class UnwindEntryParserFactory {
public:
    static std::shared_ptr<UnwindEntryParser> CreateUnwindEntryParser(std::shared_ptr<DfxMemory> memory)
    {
        std::shared_ptr<UnwindEntryParser> unwindEntryParser = nullptr;
#if defined(__arm__)
        unwindEntryParser = std::make_shared<ExidxEntryParser>(memory);
#else
        unwindEntryParser = std::make_shared<DwarfEntryParser>(memory);
#endif
        return unwindEntryParser;
    }
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
