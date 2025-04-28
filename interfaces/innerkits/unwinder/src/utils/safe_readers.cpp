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

#include "safe_readers.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include <sys/syscall.h>
#include <sys/types.h>

SafeReaders::SafeReaders()
{
    if (syscall(SYS_pipe2, pfd_, O_CLOEXEC | O_NONBLOCK) != 0) {
        pfd_[PIPE_READ] = -1;
        pfd_[PIPE_WRITE] = -1;
    }
}

SafeReaders::~SafeReaders()
{
    if (pfd_[PIPE_READ] > 0) {
        syscall(SYS_close, pfd_[PIPE_READ]);
        pfd_[PIPE_READ] = -1;
    }
    if (pfd_[PIPE_WRITE] > 0) {
        syscall(SYS_close, pfd_[PIPE_WRITE]);
        pfd_[PIPE_WRITE] = -1;
    }
}

NO_SANITIZE bool SafeReaders::IsReadbaleAddr(uintptr_t addr)
{
    if (pfd_[PIPE_WRITE] < 0) {
        return false;
    }

    if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_write, pfd_[PIPE_WRITE], addr, sizeof(char))) == -1) {
        return false;
    }

    return true;
}

NO_SANITIZE bool SafeReaders::CopyReadbaleBufSafe(uintptr_t destPtr, size_t destLen, uintptr_t srcPtr, size_t srcLen)
{
    size_t copeSize = std::min(destLen, srcLen);
    uintptr_t currentPtr = srcPtr;
    uintptr_t srcEndPtr = srcPtr + copeSize;
    size_t destIndex = 0;
    bool status = false;
    while (currentPtr < srcEndPtr) {
        uintptr_t pageEndPtr = GetCurrentPageEndAddr(currentPtr);
        pageEndPtr = std::min(pageEndPtr, srcEndPtr);
        if (!IsReadbaleAddr(currentPtr) || !IsReadbaleAddr(pageEndPtr - 1)) {
            break;
        }
        while (currentPtr < pageEndPtr) {
            reinterpret_cast<uint8_t*>(destPtr)[destIndex++] = *reinterpret_cast<uint8_t*>(currentPtr++);
        }
        status = true;
    }
    return status;
}

uintptr_t SafeReaders::GetCurrentPageEndAddr(uintptr_t addr)
{
    int pageSize = getpagesize();
    return (addr + pageSize) & (~(pageSize - 1));
}
