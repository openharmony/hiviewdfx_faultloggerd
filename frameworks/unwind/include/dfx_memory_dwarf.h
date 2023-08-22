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
#ifndef DFX_DWARF_MEMORY_H
#define DFX_DWARF_MEMORY_H

#include <atomic>
#include <cstdint>
#include <string>
#include "dfx_memory.h"

namespace OHOS {
namespace HiviewDFX {
class DfxDwarfMemory {
public:
    DfxDwarfMemory(DfxMemory* memory) : memory_(memory) {}
    virtual ~DfxDwarfMemory() = default;

    uintptr_t ReadUintptr(uintptr_t* addr);
    uint64_t ReadUleb128(uintptr_t* addr);
    int64_t ReadSleb128(uintptr_t* addr);

    template <typename AddressType>
    size_t GetEncodedSize(uint8_t encoding);

    template <typename AddressType>
    bool ReadEncodedValue(uintptr_t* addr, uintptr_t* val, uint8_t encoding);

    void ClearOffset(uint64_t& offset) { offset = UINT64_MAX; }
    int64_t pcOffset_ = INT64_MAX;
    uint64_t dataOffset_ = UINT64_MAX;
    uint64_t funcOffset_ = UINT64_MAX;
    uint64_t textOffset_ = UINT64_MAX;
protected:
    DfxMemory* memory_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
