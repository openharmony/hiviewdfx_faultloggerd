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

#ifndef DFX_MEMORY_FILE_H
#define DFX_MEMORY_FILE_H

#include "dfx_memory.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMemoryFile : public DfxMemory {
public:
    static std::shared_ptr<DfxMemoryFile> CreateFileMemory(const std::string& path, uint64_t offset);

    DfxMemoryFile() = default;
    virtual ~DfxMemoryFile() { Clear(); }

    bool Init(const std::string& file, uint64_t offset, uint64_t size = UINT64_MAX);

    size_t Read(uint64_t addr, void* dst, size_t size) override;

    size_t Size() { return size_; }

    void Clear() override;
protected:
    size_t size_ = 0;
    size_t offset_ = 0;
    uint8_t* data_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
