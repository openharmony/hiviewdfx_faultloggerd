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
#ifndef DFX_DWARF_PARSER_H
#define DFX_DWARF_PARSER_H

#include <cinttypes>
#include <memory>

#include "dwarf_define.h"
#include "dfx_memory.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DwarfParser {
public:
    DwarfParser(std::shared_ptr<DfxMemory> memory) : memory_(memory) {};
    virtual ~DwarfParser() = default;

    bool ParseCIE(uintptr_t cieAddr, CommonInfoEntry &cieInfo, uintptr_t sectionStart);
    bool ParseFDE(uintptr_t addr, FrameDescEntry &fde, CommonInfoEntry &cie, uintptr_t sectionStart);

private:
    std::shared_ptr<DfxMemory> memory_;
};
} // nameapace HiviewDFX
} // nameapace OHOS
#endif
