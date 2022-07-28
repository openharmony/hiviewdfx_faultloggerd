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

/* This files contains process dump header. */

#include "dfx_symbols_cache.h"
#include <cxxabi.h>            // for __cxa_demangle
#include <stdlib.h>            // for size_t, free, malloc
#include "algorithm"           // for sort
#include "cstdlib"             // for free
#include "dfx_define.h"        // for LOG_BUF_LEN
#include "libunwind_i-ohos.h"  // for unw_get_symbol_info_by_pc
#include "string"              // for basic_string
#include "string.h"            // for strlen
#include "vector"              // for vector

#ifdef __cplusplus
extern "C" {
#endif
static std::vector<DfxSymbol> g_elfSymbols;

bool SymbolComparator(DfxSymbol s1, DfxSymbol s2)
{
    return (s1.start < s2.start);
};
#ifdef __cplusplus
};
#endif
namespace OHOS {
namespace HiviewDFX {
bool DfxSymbolsCache::GetNameAndOffsetByPc(struct unw_addr_space *as,
    uint64_t pc, std::string& name, uint64_t& offset)
{
    if (GetNameAndOffsetByPc(pc, name, offset)) {
        return true;
    }

    char buf[LOG_BUF_LEN] { 0 };
    DfxSymbol symbol;
    if (unw_get_symbol_info_by_pc(as, pc, LOG_BUF_LEN, buf, &symbol.start, &symbol.end) != 0) {
        return false;
    }

    if (strlen(buf) >= LOG_BUF_LEN - 1) {
        return false;
    }

    int status = 0;
    std::size_t demangleBufferSize = LOG_BUF_LEN;
    auto demangleBuffer = static_cast<char*>(std::malloc(demangleBufferSize));
    auto funcName = abi::__cxa_demangle(buf, demangleBuffer, &demangleBufferSize, &status);
    if (funcName != nullptr) {
        symbol.funcName = std::string(funcName);
    } else {
        symbol.funcName = std::string(buf, strlen(buf));
    }
    std::free(demangleBuffer);
    offset = pc - symbol.start;
    name = symbol.funcName;
    cachedSymbols_.push_back(symbol);
    std::sort(cachedSymbols_.begin(), cachedSymbols_.end(), SymbolComparator);
    return true;
}

bool DfxSymbolsCache::GetNameAndOffsetByPc(uint64_t pc, std::string& name, uint64_t& offset)
{
    size_t begin = 0;
    size_t end = cachedSymbols_.size();
    while (begin < end) {
        size_t mid = begin + (end - begin) / 2;
        DfxSymbol& symbol = cachedSymbols_[mid];
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
} // namespace HiviewDFX
} // namespace OHOS
