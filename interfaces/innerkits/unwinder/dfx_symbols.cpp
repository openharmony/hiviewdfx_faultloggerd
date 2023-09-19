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
#include <cxxabi.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_util.h"
#ifdef RUSTC_DEMANGLE
#include "rustc_demangle.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxSymbols"

const std::string LINKER_PREFIX = "__dl_";
const std::string LINKER_PREFIX_NAME = "[linker]";
}

bool DfxSymbols::IsFunc(const ElfSymbol symbol)
{
    return ((symbol.shndx != SHN_UNDEF) &&
        (ELF32_ST_TYPE(symbol.info) == STT_FUNC || ELF32_ST_TYPE(symbol.info) == STT_GNU_IFUNC));
}

bool DfxSymbols::ParseSymbols(std::vector<DfxSymbol>& symbols, std::shared_ptr<DfxElf> elf, const std::string& filePath)
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
            symbols.emplace_back(elfSymbol.value, elfSymbol.size, elfSymbol.nameStr,
                Demangle(elfSymbol.nameStr), filePath);
        } else {
            continue;
        }
    }
    return true;
}

bool DfxSymbols::AddSymbolsByPlt(std::vector<DfxSymbol>& symbols, std::shared_ptr<DfxElf> elf,
                                 const std::string& filePath)
{
    if (elf == nullptr) {
        return false;
    }
    ShdrInfo shdr;
    elf->GetSectionInfo(shdr, PLT);
    symbols.emplace_back(shdr.addr, shdr.size, PLT, filePath);
    return true;
}

bool DfxSymbols::GetFuncNameAndOffset(uint64_t relPc, std::shared_ptr<DfxElf> elf,
    std::string& funcName, uint64_t& funcOffset)
{
    uint64_t start = 0;
    uint64_t end = 0;
    bool ret = GetFuncNameAndOffset(relPc, elf, funcName, start, end);
    funcOffset = relPc - start;
    return ret;
}

bool DfxSymbols::GetFuncNameAndOffset(uint64_t relPc, std::shared_ptr<DfxElf> elf,
    std::string& funcName, uint64_t& start, uint64_t& end)
{
    std::vector<DfxSymbol> symbols;
    if (!ParseSymbols(symbols, elf, "")) {
        return false;
    }

    // TODO: use binary search ?
    for (const auto& symbol : symbols) {
        if (symbol.Contain(relPc)) {
            funcName = symbol.demangle_;
            start = symbol.funcVaddr_;
            end = symbol.funcVaddr_ + symbol.size_;
            return true;
        }
    }
    return false;
}

std::string DfxSymbols::Demangle(const std::string buf)
{
    if (buf.empty()) {
        return "";
    }

    std::string funcName;
    const char *bufStr = buf.c_str();
    bool isLinkerName = false;
    if (StartsWith(buf, LINKER_PREFIX)) {
        bufStr += LINKER_PREFIX.size();
        isLinkerName = true;
        funcName += LINKER_PREFIX_NAME;
    }

    int status = 0;
    auto name = abi::__cxa_demangle(bufStr, nullptr, nullptr, &status);
#ifdef RUSTC_DEMANGLE
    if (name == nullptr) {
        DFXLOG_DEBUG("Fail to __cxa_demangle(%s), will rustc_demangle.", bufStr);
        name = rustc_demangle(bufStr);
    }
#endif
    std::string demangleName;
    if (name != nullptr) {
        demangleName = std::string(name);
        std::free(name);
    } else {
        demangleName = std::string(bufStr);
    }
    funcName += demangleName;
    return funcName;
}
} // namespace HiviewDFX
} // namespace OHOS
