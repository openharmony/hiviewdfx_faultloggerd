/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "dfx_symbols.h"

#include <algorithm>
#include <cstdlib>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_demangle.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxSymbols"
}

bool DfxSymbols::IsFunc(const ElfSymbol symbol)
{
    return ((symbol.shndx != SHN_UNDEF) &&
        (ELF32_ST_TYPE(symbol.info) == STT_FUNC || ELF32_ST_TYPE(symbol.info) == STT_GNU_IFUNC));
}

bool DfxSymbols::ParseSymbols(std::vector<DfxSymbol>& symbols, DfxElf* elf, const std::string& filePath)
{
    if (elf == nullptr) {
        return false;
    }
    std::vector<ElfSymbol> elfSymbols = elf->GetElfSymbols();
    for (auto elfSymbol : elfSymbols) {
        if (IsFunc(elfSymbol)) {
            if (elfSymbol.value == 0) {
                continue;
            }
            DfxSymbol symbol;
            symbol.SetVaddr(elfSymbol.value, elfSymbol.value, elfSymbol.size);
            std::string demangleName = DfxDemangle::Demangle(elfSymbol.nameStr);
            symbol.SetName(elfSymbol.nameStr, demangleName);
            symbol.SetModule(filePath);
            //LOGU("%016" PRIx64 "|%4" PRIu64 "|%s", elfSymbol.value, elfSymbol.size, demangleName.c_str());
            symbols.emplace_back(symbol);
        } else {
            continue;
        }
    }
    return true;
}

bool DfxSymbols::AddSymbolsByPlt(std::vector<DfxSymbol>& symbols, DfxElf* elf, const std::string& filePath)
{
    if (elf == nullptr) {
        return false;
    }
    DfxSymbol symbol;
    ShdrInfo shdr;
    elf->GetSectionInfo(shdr, PLT);
    symbol.SetVaddr(shdr.addr, shdr.addr, shdr.size);
    symbol.SetName(PLT, PLT);
    symbol.SetModule(filePath);
    symbols.emplace_back(symbol);
    return true;
}

bool DfxSymbols::GetFuncNameAndOffset(uint64_t pc, DfxElf* elf,
    std::string* funcName, uint64_t* start, uint64_t* end)
{
    std::vector<DfxSymbol> symbols;
    if (!ParseSymbols(symbols, elf, "")) {
        return false;
    }

    // TODO: use binary search ?
    for (const auto& symbol : symbols) {
        if (symbol.Contain(pc)) {
            *funcName = symbol.demangle_;
            *start = symbol.funcVaddr_;
            *end = symbol.funcVaddr_ + symbol.size_;
            return true;
        }
    }
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS
