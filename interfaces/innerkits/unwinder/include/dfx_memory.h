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
#ifndef DFX_MEMORY_H
#define DFX_MEMORY_H

#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>
#include "dfx_accessors.h"
#include "unwind_context.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMemory {
public:
    DfxMemory(std::shared_ptr<DfxAccessors> acc) : acc_(acc) {}
    virtual ~DfxMemory() = default;

    void SetCtx(void* ctx) { ctx_ = ctx; }
    void SetAlign(bool alignAddr, int alignBytes)
    {
        alignAddr_ = alignAddr;
        alignBytes_ = alignBytes;
    }

    bool ReadReg(int regIdx, uintptr_t *val);
    bool ReadMem(uintptr_t addr, uintptr_t *val);

    size_t Read(uintptr_t& addr, void* val, size_t size, bool incre = false);
    bool ReadU8(uintptr_t& addr, uint8_t *val, bool incre = false);
    bool ReadU16(uintptr_t& addr, uint16_t *val, bool incre = false);
    bool ReadU32(uintptr_t& addr, uint32_t *val, bool incre = false);
    bool ReadU64(uintptr_t& addr, uint64_t *val, bool incre = false);
    bool ReadUptr(uintptr_t& addr, uintptr_t *val, bool incre = false);

    template <typename T>
    T Read(uintptr_t& addr, bool incre = true)
    {
        T val = 0;
        Read(addr, (void *)&val, sizeof(T), incre);
        return val;
    }

    bool ReadPrel31(uintptr_t& addr, uintptr_t *val);

    uint64_t ReadUleb128(uintptr_t& addr);
    int64_t ReadSleb128(uintptr_t& addr);
    size_t GetEncodedSize(uint8_t encoding);
    uintptr_t ReadEncodedValue(uintptr_t& addr, uint8_t encoding,
        uintptr_t dataRel = 0, uintptr_t funcRel = 0);

private:
    std::shared_ptr<DfxAccessors> acc_;
    void* ctx_;
    bool alignAddr_ = false;
    int alignBytes_ = 0;

};
} // namespace HiviewDFX
} // namespace OHOS
#endif
