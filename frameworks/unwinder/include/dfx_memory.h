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
#include <memory>
#include <string>
#include <cinttypes>
#include <unistd.h>

namespace OHOS {
namespace HiviewDFX {
#define ALIGN(val, align) (((val) + (align) - 1) & ~((align) - 1))
const uint64_t pagesizeMask = static_cast<uint64_t>(getpagesize() - 1UL);
const uint64_t pagesizeAlignMask = ~((static_cast<uint64_t>(getpagesize())) - 1UL);

class DfxMemory {
public:
    DfxMemory() = default;
    virtual ~DfxMemory() = default;

    static size_t MemoryCopy(void *dst, uint8_t *data, size_t copySize, size_t offset, size_t dataSize);

    virtual bool IsLocal() const { return false; }

    bool ReadFully(uint64_t addr, void* dst, size_t size);
    virtual size_t Read(uint64_t addr, void* dst, size_t size) = 0;
    virtual bool ReadString(uint64_t addr, std::string* dst, size_t maxRead);
    inline bool Read32(uint64_t addr, uint32_t* dst);
    inline bool Read64(uint64_t addr, uint64_t* dst);

    virtual void Clear() {}
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
