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

#ifndef DFX_SYMBOLS_H
#define DFX_SYMBOLS_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "dfx_memory.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct SymbolInfo {
    uint64_t start;
    uint64_t end;
    uint32_t name;
    uint16_t ndx;
    uint8_t type;
    std::string funcName;
} SymbolInfo;
#ifdef __cplusplus
};
#endif
struct unw_addr_space;

namespace OHOS {
namespace HiviewDFX {
class DfxSymbols final {
public:
    DfxSymbols();
    ~DfxSymbols() = default;
    bool GetNameAndOffsetByPc(struct unw_addr_space *as, uint64_t pc, std::string& name, uint64_t& offset);
    bool GetNameAndOffsetByPc(std::shared_ptr<DfxMemory> memory, uint64_t pc, std::string& name, uint64_t& offset);

private:
    bool GetNameAndOffsetByPc(uint64_t pc, std::string& name, uint64_t& offset);
    bool Demangle(const char* buf, const int len, std::string& funcName);

private:
    std::vector<SymbolInfo> symbols_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif  // DFX_SYMBOL_CACHE_H
