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

#include "dfx_mmap.h"

#include <fcntl.h>
#include <securec.h>

#include "dfx_define.h"
#include "dfx_log.h"

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
#if is_ohos
    Clear();

    if (fd < 0) {
        return false;
    }
    mmap_ = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, offset);
    if (mmap_ == MAP_FAILED) {
        LOGE("Faild to mmap, errno(%d)", errno);
        size_ = 0;
        return false;
    }
    size_ = size;
    DFXLOG_DEBUG("mmap size %u", size_);
    return true;
#else
    return false;
#endif
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
#if is_ohos
    if (mmap_ != MAP_FAILED) {
        munmap(mmap_, size_);
        mmap_ = MAP_FAILED;
    }
#endif
}

size_t DfxMmap::Read(uintptr_t& addr, void* val, size_t size, bool incre)
{
    if ((mmap_ == MAP_FAILED) || (val == nullptr)) {
        return 0;
    }

    size_t ptr = static_cast<size_t>(addr);
    if (ptr >= size_) {
        LOGE("pos: %d, size: %d", ptr, size_);
        return 0;
    }

    size_t left = size_ - ptr;
    const uint8_t* actualBase = static_cast<const uint8_t*>(mmap_) + ptr;
    size_t actualLen = std::min(left, size);
    if (memcpy_s(val, actualLen, actualBase, actualLen) != 0) {
        LOGE("fail to memcpy");
        return 0;
    }
    if (incre) {
        addr += actualLen;
    }
    return actualLen;
}
} // namespace HiviewDFX
} // namespace OHOS
