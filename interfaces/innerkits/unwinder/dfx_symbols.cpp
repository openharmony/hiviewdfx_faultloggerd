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

bool DfxSymbols::ParseSymbols(std::vector<DfxSymbol>& symbols, std::shared_ptr<DfxElf> elf, const std::string& filePath)
{
    if (elf == nullptr) {
        return false;
    }
    auto elfSymbols = elf->GetFuncSymbols(true);
    for (auto elfSymbol : elfSymbols) {
        symbols.emplace_back(elfSymbol.value, elfSymbol.size,
            elfSymbol.nameStr, Demangle(elfSymbol.nameStr), filePath);
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

bool DfxSymbols::GetFuncNameAndOffsetByPc(uint64_t relPc, std::shared_ptr<DfxElf> elf,
    std::string& funcName, uint64_t& funcOffset)
{
    if (BinarySearch(relPc, funcName, funcOffset)) {
        return true;
    }

    std::string name;
    uint64_t start, size;
#if defined(__arm__)
    relPc = relPc | 1;
#endif
    if (elf->GetFuncInfo(relPc, name, start, size)) {
        funcName = Demangle(name);
        funcOffset = relPc - start;
#if defined(__arm__)
        funcOffset &= ~1;
#endif
        LOGU("Symbol funcName: %s, funcOffset: %llx", funcName.c_str(), (uint64_t)funcOffset);
        symbols_.emplace_back(start, size, name, funcName, "");
        return true;
    }
    return false;
}

bool DfxSymbols::BinarySearch(uint64_t addr, std::string& funcName, uint64_t& funcOffset)
{
    if (symbols_.empty()) {
        return false;
    }
    size_t begin = 0;
    size_t end = symbols_.size();
    while (begin < end) {
        size_t mid = begin + (end - begin) / 2;
        const auto& iter = symbols_[mid];
        if (addr < iter.funcVaddr_) {
            end = mid;
        } else if (addr < (iter.funcVaddr_ + iter.size_)) {
            funcName = iter.demangle_;
            funcOffset = addr - iter.funcVaddr_;
            return true;
        } else {
            begin = mid + 1;
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
