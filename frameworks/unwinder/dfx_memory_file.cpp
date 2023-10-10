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

#include "dfx_memory_file.h"

#include <algorithm>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "unique_fd.h"

namespace OHOS {
namespace HiviewDFX {
std::shared_ptr<DfxMemoryFile> DfxMemoryFile::CreateFileMemory(const std::string& path, uint64_t offset)
{
    auto memory = std::make_shared<DfxMemoryFile>();
    if (memory->Init(path, offset)) {
        return memory;
    }
    return nullptr;
}

void DfxMemoryFile::Clear()
{
    if (data_ != nullptr) {
        munmap(&data_[-offset_], size_ + offset_);
        data_ = nullptr;
    }
}

bool DfxMemoryFile::Init(const std::string& file, uint64_t offset, uint64_t size)
{
    Clear(); // Clear out any previous data if it exists.

    OHOS::UniqueFd ufd = OHOS::UniqueFd(OHOS_TEMP_FAILURE_RETRY(open(file.c_str(), O_RDONLY | O_CLOEXEC)));
    int fd = ufd.Get();
    if (fd == -1) {
        DFXLOG_ERROR("%s : Failed to get fd.", __func__);
        return false;
    }

    size_t fileSize = static_cast<size_t>(GetFileSize(fd));
    if ((fileSize == 0) || (offset >= static_cast<uint64_t>(fileSize))) {
        DFXLOG_ERROR("%s : invalid fileSize(%lu)", __func__, fileSize);
        return false;
    }

    offset_ = offset & static_cast<uint64_t>ALIGN_BYTES(getpagesize());
    uint64_t alignedOffset = offset & ALIGN_MASK(getpagesize());
    if (alignedOffset > static_cast<uint64_t>(fileSize) ||
        offset > static_cast<uint64_t>(fileSize)) {
        DFXLOG_ERROR(
            "%s : invalid alignedOffset(%lu) or offset(%lu) : file size(%lu)",
            __func__, alignedOffset, offset, fileSize);
        return false;
    }

    size_ = fileSize - alignedOffset;
    uint64_t maxSize;
    if (!__builtin_add_overflow(size, offset_, &maxSize) && maxSize < size_) {
        size_ = maxSize;
    }
    // (tomys): Some symbol seek & fetch libraries like fake_dlfcn search libs base address
    // by matching privilege 'r--p' and 'r-xp' in proc maps. So we mapped our target library
    // by 'rw-p' privilege flags to avoid disturbing these libraries.
    void* map = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, alignedOffset);
    if (map == MAP_FAILED) {
        DFXLOG_ERROR("%s : Failed to mmap", __func__);
        return false;
    }

    data_ = &reinterpret_cast<uint8_t*>(map)[offset_];
    size_ -= offset_;
    return true;
}

size_t DfxMemoryFile::Read(uint64_t addr, void* dst, size_t size)
{
    if (addr >= size_) {
        return 0;
    }
    return MemoryCopy(dst, data_, size, addr, size_);
}
} // namespace HiviewDFX
} // namespace OHOS
