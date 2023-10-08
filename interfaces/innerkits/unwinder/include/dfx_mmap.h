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
#ifndef DFX_MMAP_H
#define DFX_MMAP_H

#include <atomic>
#include <cstdint>
#include <string>
#include <sys/mman.h>
#include "dfx_memory.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMmap : public DfxMemory {
public:
    DfxMmap() = default;
    virtual ~DfxMmap() { Clear(); }

    bool Init(const std::string &file);
    void Clear();

    inline void* Get()
    {
        if (mmap_ != MAP_FAILED) {
            return mmap_;
        }
        return nullptr;
    }
    inline size_t Size() { return size_; }

    size_t Read(uintptr_t& addr, void* val, size_t size, bool incre = false) override;

private:
    void *mmap_ = MAP_FAILED;
    size_t size_ = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif