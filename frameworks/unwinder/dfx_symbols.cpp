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
#include <cstring>
#include <cxxabi.h>
#include <string>
#include <vector>
#include "dfx_define.h"
#include "dfx_log.h"
#include "libunwind_i-ohos.h"
#ifdef RUSTC_DEMANGLE
#include "rustc_demangle.h"
#endif
#include "dfx_elf.h"

namespace OHOS {
namespace HiviewDFX {

DfxSymbols::DfxSymbols()
{
    symbols_.clear();
}

bool DfxSymbols::GetNameAndOffsetByPc(struct unw_addr_space *as,
    uint64_t pc, std::string& name, uint64_t& offset)
{
    if (GetNameAndOffsetByPc(pc, name, offset)) {
        return true;
    }

    char buf[LOG_BUF_LEN] = {0};
    SymbolInfo symbol;
    if (unw_get_symbol_info_by_pc(as, pc, LOG_BUF_LEN, buf, &symbol.start, &symbol.end) != 0) {
        return false;
    }

    Demangle(buf, strlen(buf), symbol.funcName);

    offset = pc - symbol.start;
    name = symbol.funcName;
    symbols_.push_back(symbol);
    std::sort(symbols_.begin(), symbols_.end(), [](const SymbolInfo& a, const SymbolInfo& b) {
        return a.start < b.start;
    });
    return true;
}

bool DfxSymbols::GetNameAndOffsetByPc(std::shared_ptr<DfxMemory> memory,
    uint64_t pc, std::string& name, uint64_t& offset)
{
    if (GetNameAndOffsetByPc(pc, name, offset)) {
        return true;
    }

    std::string funcName;
    SymbolInfo symbol;
    auto elf = std::make_shared<DfxElf>(memory);
    if (!elf->GetFuncNameAndOffset(pc, &funcName, &symbol.start, &symbol.end)) {
        return false;
    }

    Demangle(funcName.c_str(), funcName.length(), symbol.funcName);

    offset = pc - symbol.start;
    name = symbol.funcName;
    symbols_.push_back(symbol);
    std::sort(symbols_.begin(), symbols_.end(), [](const SymbolInfo& a, const SymbolInfo& b) {
        return a.start < b.start;
    });
    return true;
}

bool DfxSymbols::GetNameAndOffsetByPc(uint64_t pc, std::string& name, uint64_t& offset)
{
    size_t begin = 0;
    size_t end = symbols_.size();
    while (begin < end) {
        size_t mid = begin + (end - begin) / 2;
        const SymbolInfo& symbol = symbols_[mid];
        if (pc < symbol.start) {
            end = mid;
        } else if (pc <= symbol.end) {
            offset = pc - symbol.start;
            name = symbol.funcName;
            return true;
        } else {
            begin = mid + 1;
        }
    }
    return false;
}

bool DfxSymbols::Demangle(const char* buf, const int len, std::string& funcName)
{
    if ((buf == nullptr) || (len >= LOG_BUF_LEN - 1)) {
        return false;
    }

    int status = 0;
    auto name = abi::__cxa_demangle(buf, nullptr, nullptr, &status);
#ifdef RUSTC_DEMANGLE
    if (name == nullptr) {
        DFXLOG_DEBUG("Fail to __cxa_demangle(%s), will rustc_demangle.", buf);
        name = rustc_demangle(buf);
    }
#endif
    if (name != nullptr) {
        funcName = std::string(name);
        std::free(name);
    } else {
        funcName = std::string(buf, len);
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
