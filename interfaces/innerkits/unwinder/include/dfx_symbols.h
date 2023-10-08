/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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
#include <map>
#include <vector>
#include "dfx_elf.h"
#include "dfx_symbol.h"

namespace OHOS {
namespace HiviewDFX {
class DfxSymbols final {
public:
    DfxSymbols() { symbols_.clear(); }
    ~DfxSymbols() = default;

    static bool ParseSymbols(std::vector<DfxSymbol>& symbols,
        std::shared_ptr<DfxElf> elf, const std::string& filePath);
    static bool AddSymbolsByPlt(std::vector<DfxSymbol>& symbols,
        std::shared_ptr<DfxElf> elf, const std::string& filePath);

    bool GetFuncNameAndOffsetByPc(uint64_t relPc, std::shared_ptr<DfxElf> elf,
        std::string& funcName, uint64_t& funcOffset);

    static std::string Demangle(const std::string buf);

private:
    bool BinarySearch(uint64_t addr, std::string& funcName, uint64_t& funcOffset);

private:
    std::vector<DfxSymbol> symbols_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif