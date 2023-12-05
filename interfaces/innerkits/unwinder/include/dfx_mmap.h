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
#if !is_mingw
#include <sys/mman.h>
#else
#include "dfx_nonlinux_define.h"
#endif
#include "dfx_memory.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMmap : public DfxMemory {
public:
    DfxMmap() = default;
    virtual ~DfxMmap() { Clear(); }

    bool Init(const int fd, const size_t size, const off_t offset = 0);
    bool Init(uint8_t *decompressedData, size_t size);
    void Clear();

    inline void* Get()
    {
        if (mmap_ != MAP_FAILED) {
            return mmap_;
        }
        return nullptr;
    }
    inline size_t Size() { return size_; }
    inline void SetNeedUnmap(bool need) { needUnmap_ = need; }

    size_t Read(uintptr_t& addr, void* val, size_t size, bool incre = false) override;

private:
    void *mmap_ = MAP_FAILED;
    size_t size_ = 0;
    bool needUnmap_ = true;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
