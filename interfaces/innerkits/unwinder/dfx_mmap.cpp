/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "dfx_mmap.h"

#include <fcntl.h>
#include <securec.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMmap"
}

bool DfxMmap::Init(const int fd, const size_t size, const off_t offset)
{
    Clear();

    if (fd < 0) {
        return false;
    }
    mmap_ = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, offset);
    if (mmap_ == MAP_FAILED) {
        DFXLOGE("Failed to mmap, errno(%{public}d)", errno);
        size_ = 0;
        return false;
    }
    size_ = size;
    DFXLOGD("mmap size %{public}zu", size_);
    return true;
}

bool DfxMmap::Init(uint8_t *decompressedData, size_t size)
{
    if (!decompressedData || size == 0) {
        return false;
    }

    // this pointer was managed by shared_ptr will not occur double free issue.
    mmap_ = decompressedData;
    size_ = size;
    return true;
}

void DfxMmap::Clear()
{
    if ((mmap_ != MAP_FAILED) && (needUnmap_)) {
        munmap(mmap_, size_);
        mmap_ = MAP_FAILED;
    }
}

size_t DfxMmap::Read(uintptr_t& addr, void* val, size_t size, bool incre)
{
    if ((mmap_ == MAP_FAILED) || (val == nullptr)) {
        return 0;
    }

    size_t ptr = static_cast<size_t>(addr);
    if (ptr >= size_) {
        DFXLOGU("pos: %{public}zu, size: %{public}zu", ptr, size_);
        return 0;
    }

    size_t left = size_ - ptr;
    const uint8_t* actualBase = static_cast<const uint8_t*>(mmap_) + ptr;
    size_t actualLen = std::min(left, size);
    if (memcpy_s(val, actualLen, actualBase, actualLen) != 0) {
        DFXLOGE("Failed to memcpy_s");
        return 0;
    }
    if (incre) {
        addr += actualLen;
    }
    return actualLen;
}
} // namespace HiviewDFX
} // namespace OHOS
