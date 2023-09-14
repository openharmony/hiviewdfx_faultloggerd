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
#include <unique_fd.h>
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

bool DfxMmap::Init(const std::string &file)
{
    Clear();

    OHOS::UniqueFd fd = OHOS::UniqueFd(OHOS_TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY)));
    if (fd < 0) {
        return false;
    }

    size_ = GetFileSize(fd);
    mmap_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (mmap_ == MAP_FAILED) {
        size_ = 0;
    }
    DFXLOG_DEBUG("mmap size %u", size_);
    return true;
}

void DfxMmap::Clear()
{
    if (mmap_ != MAP_FAILED) {
        munmap(mmap_, size_);
        mmap_ = MAP_FAILED;
    }
}

void* DfxMmap::Get()
{
    if (mmap_ != MAP_FAILED) {
        return mmap_;
    }
    return nullptr;
}

size_t DfxMmap::Read(uint64_t* pos, void *buf, size_t size)
{
    if (mmap_ == MAP_FAILED) {
        return 0;
    }

    size_t addr = static_cast<size_t>(*pos);
    if (addr >= size_) {
        return 0;
    }

    size_t left = size_ - addr;
    const uint8_t* actualBase = static_cast<const uint8_t*>(mmap_) + addr;
    size_t actualLen = std::min(left, size);
    memcpy_s(buf, actualLen, actualBase, actualLen);
    return actualLen;
}

bool DfxMmap::ReadString(uint64_t* pos, std::string* str, size_t maxSize)
{
    char buf[NAME_LEN];
    size_t size = 0;
    uint64_t curPos;
    for (size_t offset = 0; offset < maxSize; offset += size) {
        size_t read = std::min(sizeof(buf), maxSize - offset);
        curPos = *pos + offset;
        size = Read(&curPos, buf, read);
        if (size == 0) {
            return false;
        }
        size_t length = strnlen(buf, size);
        if (length < size) {
            if (offset == 0) {
                str->assign(buf, length);
                return true;
            } else {
                str->assign(offset + length, '\0');
                Read(pos, (void *)str->data(), str->size());
                return true;
            }
        }
    }
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS
