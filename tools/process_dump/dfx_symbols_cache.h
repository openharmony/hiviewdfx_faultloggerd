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

#ifndef DFX_SYMBOLS_CACHE_H
#define DFX_SYMBOLS_CACHE_H

#include <cstdint>
#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct DfxSymbol {
    uint64_t start;
    uint64_t end;
    std::string funcName;
} DfxSymbol;
#ifdef __cplusplus
};
#endif
struct unw_addr_space;
namespace OHOS {
namespace HiviewDFX {
class DfxSymbolsCache final {
public:
    DfxSymbolsCache();
    ~DfxSymbolsCache() {};
    bool GetNameAndOffsetByPc(struct unw_addr_space *as, uint64_t pc, std::string& name, uint64_t& offset);

private:
    bool GetNameAndOffsetByPc(uint64_t pc, std::string& name, uint64_t& offset);
    std::vector<DfxSymbol> cachedSymbols_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_SYMBOL_CACHE_H
