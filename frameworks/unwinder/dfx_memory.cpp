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

#include "dfx_memory.h"
#include <algorithm>
#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
size_t DfxMemory::MemoryCopy(void *dst, uint8_t *data, size_t copySize, size_t offset, size_t dataSize)
{
    if (offset >= dataSize) {
        return 0;
    }

    size_t bytesLeft = dataSize - static_cast<size_t>(offset);
    const unsigned char *actualBase = static_cast<const unsigned char *>(data) + offset;
    size_t actualLen = std::min(bytesLeft, copySize);
    if (memcpy_s(dst, actualLen, actualBase, actualLen) != 0) {
        DFXLOG_ERROR("%s :: memcpy error\n", __func__);
    }
    return actualLen;
}

bool DfxMemory::ReadFully(uint64_t addr, void* dst, size_t size)
{
    size_t rc = Read(addr, dst, size);
    return rc == size;
}

bool DfxMemory::ReadString(uint64_t addr, std::string* dst, size_t maxRead)
{
    if (dst == nullptr) {
        return false;
    }
    char buffer[LOG_BUF_LEN] = {0};
    size_t size = 0;
    for (size_t nRead = 0; nRead < maxRead; nRead += size) {
        size_t read = std::min(sizeof(buffer), maxRead - nRead);
        size = Read(addr + nRead, buffer, read);
        if (size == 0) {
            return false;
        }
        size_t length = strnlen(buffer, size);
        if (length < size) {
            if (nRead == 0) {
                dst->assign(buffer, length);
                return true;
            } else {
                dst->assign(nRead + length, '\0');
                return ReadFully(addr, (void*)(dst->data()), dst->size());
            }
        }
    }
    return false;
}

inline bool DfxMemory::Read32(uint64_t addr, uint32_t* dst)
{
    return ReadFully(addr, dst, sizeof(uint32_t));
}

inline bool DfxMemory::Read64(uint64_t addr, uint64_t* dst)
{
    return ReadFully(addr, dst, sizeof(uint64_t));
}

} // namespace HiviewDFX
} // namespace OHOS
