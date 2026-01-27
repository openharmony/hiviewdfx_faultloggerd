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

#include <fcntl.h>
#include <unistd.h>

#include <sys/syscall.h>

#include "memory_reader.h"

namespace OHOS {
namespace HiviewDFX {

ThreadMemoryReader::ThreadMemoryReader(uintptr_t stackBegin, uintptr_t stackEnd) : MemoryReader(),
    stackBegin_(stackBegin), stackEnd_(stackEnd) {}

bool ThreadMemoryReader::ReadMemory(const uintptr_t src, void* dst, size_t dataSize)
{
    if (src >= stackBegin_ && src < stackEnd_  && (stackEnd_ - src) > dataSize) {
        for (size_t i = 0; i < dataSize; i++) {
            static_cast<unsigned char*>(dst)[i] = reinterpret_cast<const unsigned char*>(src)[i];
        }
        return true;
    }
    return false;
}

ProcessMemoryReader::ProcessMemoryReader() : MemoryReader()
{
    int pipe[2]{-1};
    if (pipe2(pipe, O_CLOEXEC) == 0) {
        readFd_ = SmartFd(pipe[0], false);
        writeFd_ = SmartFd(pipe[1], false);
        readablePageMask_ = ~static_cast<uintptr_t>(getpagesize() - 1);
    }
}

bool ProcessMemoryReader::ReadCharByPipe(uintptr_t src, void* dst) const
{
    return syscall(SYS_write, writeFd_.GetFd(), reinterpret_cast<const void*>(src), sizeof(char)) == sizeof(char) &&
        syscall(SYS_read, readFd_.GetFd(), dst, sizeof(char)) == sizeof(char);
}

bool ProcessMemoryReader::ReadMemory(const uintptr_t src, void* dst, size_t dataSize)
{
    for (uintptr_t i = 0; i < dataSize; i++) {
        uintptr_t pendingSrc = src + i;
        uintptr_t pendingDst = reinterpret_cast<uintptr_t>(dst) + i;
        if ((pendingSrc & readablePageMask_) == readablePage_) {
            *reinterpret_cast<unsigned char*>(pendingDst) = *reinterpret_cast<const unsigned char*>(pendingSrc);
            continue;
        }
        if (!ReadCharByPipe(pendingSrc, reinterpret_cast<void*>(pendingDst))) {
            return false;
        }
        readablePage_ = pendingSrc & readablePageMask_;
    }
    return true;
}
}
}
