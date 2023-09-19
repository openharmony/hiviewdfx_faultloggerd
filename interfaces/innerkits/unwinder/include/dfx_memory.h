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
    DfxMemory() = default;
    explicit DfxMemory(std::shared_ptr<DfxAccessors> acc) : acc_(acc) {}
    virtual ~DfxMemory() = default;

    void SetCtx(void* ctx) { ctx_ = ctx; }
    void SetAlign(bool alignAddr, int alignBytes)
    {
        alignAddr_ = alignAddr;
        alignBytes_ = alignBytes;
    }

    bool ReadReg(int regIdx, uintptr_t *val);
    bool ReadMem(uintptr_t addr, uintptr_t *val);

    virtual size_t Read(uintptr_t& addr, void* val, size_t size, bool incre = false);
    virtual bool ReadU8(uintptr_t& addr, uint8_t *val, bool incre = false);
    virtual bool ReadU16(uintptr_t& addr, uint16_t *val, bool incre = false);
    virtual bool ReadU32(uintptr_t& addr, uint32_t *val, bool incre = false);
    virtual bool ReadU64(uintptr_t& addr, uint64_t *val, bool incre = false);
    virtual bool ReadUptr(uintptr_t& addr, uintptr_t *val, bool incre = false);
    virtual bool ReadString(uintptr_t& addr, std::string* str, size_t maxSize, bool incre = false);

    template <typename T>
    T Read(uintptr_t& addr, bool incre = true)
    {
        T val = 0;
        Read(addr, (void *)&val, sizeof(T), incre);
        return val;
    }

    virtual bool ReadPrel31(uintptr_t& addr, uintptr_t *val);

    virtual uint64_t ReadUleb128(uintptr_t& addr);
    virtual int64_t ReadSleb128(uintptr_t& addr);
    virtual void SetDataOffset(uintptr_t offset) { dataOffset_ = offset; }
    virtual void SetFuncOffset(uintptr_t offset) { funcOffset_ = offset; }
    virtual size_t GetEncodedSize(uint8_t encoding);
    virtual uintptr_t ReadEncodedValue(uintptr_t& addr, uint8_t encoding);

private:
    std::shared_ptr<DfxAccessors> acc_;
    void* ctx_ = nullptr;
    bool alignAddr_ = false;
    int alignBytes_ = 0;
    uintptr_t dataOffset_ = 0;
    uintptr_t funcOffset_ = 0;
};

class DfxMemoryCpy : public DfxMemory {
public:
    DfxMemoryCpy() = default;
    virtual ~DfxMemoryCpy() = default;

    size_t Read(uintptr_t& addr, void* val, size_t size, bool incre = false) override;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
