/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "coredump_buffer_writer.h"

#include <algorithm>

#include "dfx_log.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
    constexpr size_t MAX_COPY_CAP = 1 * 1024 * 1024;
}

CoredumpBufferWriter::CoredumpBufferWriter(char* base, size_t capacity)
{
    base_ = base;
    current_ = base;
    capacity_ = capacity;
}

bool CoredumpBufferWriter::Write(const void* data, size_t size)
{
    if (base_ == nullptr) {
        DFXLOGE("buffer writer not initialized");
        return false;
    }

    size_t used = static_cast<size_t>(current_ - base_);
    if (size > capacity_ - used) {
        DFXLOGE("buffer overflow, used=%{public}zu, write=%{public}zu, cap=%{public}zu", used, size, capacity_);
        return false;
    }

    auto cap = std::min(MAX_COPY_CAP, capacity_ - used);
    if (memcpy_s(current_, cap, data, size) != EOK) {
        DFXLOGE("memcpy_s failed in buffer writer");
        return false;
    }

    current_ += size;
    return true;
}

char* CoredumpBufferWriter::GetCurrent() const
{
    return current_;
}

char* CoredumpBufferWriter::GetBase() const
{
    return base_;
}

size_t CoredumpBufferWriter::GetCapacity() const
{
    return capacity_;
}

bool CoredumpBufferWriter::Advance(size_t size)
{
    if (current_ + size > base_ + capacity_) {
        DFXLOGE("invalid advance");
        return false;
    }
    current_ += size;
    return true;
}

void CoredumpBufferWriter::Reset()
{
    current_ = base_;
}

bool CoredumpBufferWriter::WriteAt(const void* data, size_t size, size_t offset)
{
    if (base_ == nullptr) {
        DFXLOGE("write at called before init");
        return false;
    }
    size_t result;
    if (__builtin_add_overflow(offset, size, &result)) {
        return false;
    }
    if (result > capacity_) {
        DFXLOGE("write at overflow offset=%{public}zu, write=%{public}zu, cap=%{public}zu", offset, size, capacity_);
        return false;
    }
    char* dest = base_ + offset;
    auto cap = std::min(MAX_COPY_CAP, capacity_ - offset);
    if (memcpy_s(dest, cap, data, size) != EOK) {
        DFXLOGE("memcpy_s failed in WriteAt");
        return false;
    }
    return true;
}

uint64_t CoredumpBufferWriter::GetOffset() const
{
    return static_cast<uint64_t>(current_ - base_);
}

bool CoredumpBufferWriter::SetCurrent(char* current)
{
    if (current < base_ || current > base_ + capacity_) {
        return false;
    }
    current_ = current;
    return true;
}
}
}
