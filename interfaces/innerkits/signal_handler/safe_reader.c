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

#include "include/safe_reader.h"

#include <errno.h>
#include <fcntl.h>
#include <securec.h>
#include <sys/syscall.h>
#include "dfx_define.h"
#include "dfx_log.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSafeReader"
#endif

static int g_pipeFd[] = {-1, -1};

size_t Min(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

static bool InitPipe()
{
    if (syscall(SYS_pipe2, g_pipeFd, O_CLOEXEC | O_NONBLOCK) != 0) {
        g_pipeFd[PIPE_READ] = -1;
        g_pipeFd[PIPE_WRITE] = -1;
        return false;
    }
    return true;
}

void DeinitPipe()
{
    if (g_pipeFd[PIPE_READ] > 0) {
        syscall(SYS_close, g_pipeFd[PIPE_READ]);
        g_pipeFd[PIPE_READ] = -1;
    }
    if (g_pipeFd[PIPE_WRITE] > 0) {
        syscall(SYS_close, g_pipeFd[PIPE_WRITE]);
        g_pipeFd[PIPE_WRITE] = -1;
    }
}

static uintptr_t GetCurrentPageEndAddr(uintptr_t addr)
{
    uintptr_t pageSize = (uintptr_t)getpagesize(); // default 4k
    return (addr + pageSize) & (~(pageSize - 1));
}

static bool IsReadableAddr(uintptr_t addr)
{
    if (g_pipeFd[PIPE_WRITE] < 0) {
        if (!InitPipe()) {
            return false;
        }
    }

    if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_write, g_pipeFd[PIPE_WRITE], addr, sizeof(char))) == -1) {
        return false;
    }
    return true;
}

size_t CopyReadableBufSafe(uintptr_t destPtr, size_t destLen, uintptr_t srcPtr, size_t srcLen)
{
    size_t copeSize = Min(destLen, srcLen);
    uintptr_t currentPtr = srcPtr;
    uintptr_t srcEndPtr = srcPtr + copeSize;
    size_t destIndex = 0;
    while (currentPtr < srcEndPtr) {
        uintptr_t pageEndPtr = GetCurrentPageEndAddr(currentPtr);
        pageEndPtr = Min(pageEndPtr, srcEndPtr);
        if (!IsReadableAddr(currentPtr) || !IsReadableAddr(pageEndPtr - 1)) {
            break;
        }
        while (currentPtr < pageEndPtr) {
            ((uint8_t*)destPtr)[destIndex++] = *((uint8_t*)currentPtr++);
        }
    }
    return destIndex;
}
