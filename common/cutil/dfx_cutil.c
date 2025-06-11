/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "dfx_cutil.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#ifndef DFX_SIGNAL_LIBC
#include <securec.h>
#endif
#include <stdio.h>
#include <string.h>
#include "dfx_define.h"

static bool ReadStringFromFile(const char* path, char* dst, size_t dstSz)
{
    if ((dst == NULL) || (path == NULL) || (dstSz == 0)) {
        return false;
    }
    int fd = OHOS_TEMP_FAILURE_RETRY(open(path, O_RDONLY));
    if (fd < 0) {
        return false;
    }

    char name[NAME_BUF_LEN] = {0};
    ssize_t nRead = OHOS_TEMP_FAILURE_RETRY(read(fd, name, NAME_BUF_LEN - 1));
    if (nRead <= 0) {
        syscall(SYS_close, fd);
        return false;
    }

    size_t i = 0;
    for (; i < dstSz - 1 && name[i] != '\n' && name[i] != '\0'; i++) {
        dst[i] = name[i];
    }
    dst[i] = '\0';

    syscall(SYS_close, fd);
    return true;
}

bool GetThreadName(char* buffer, size_t bufferSz)
{
    return ReadStringFromFile(PROC_SELF_COMM_PATH, buffer, bufferSz);
}

bool GetThreadNameByTid(int32_t tid, char* buffer, size_t bufferSz)
{
    char threadNamePath[NAME_BUF_LEN] = { 0 };
#ifdef DFX_SIGNAL_LIBC
    if (snprintf(threadNamePath, sizeof(threadNamePath), "/proc/%d/comm", tid) <= 0) {
#else
    if (snprintf_s(threadNamePath, sizeof(threadNamePath), sizeof(threadNamePath) - 1, "/proc/%d/comm", tid) <= 0) {
#endif
        return false;
    }
    return ReadStringFromFile(threadNamePath, buffer, bufferSz);
}

bool GetProcessName(char* buffer, size_t bufferSz)
{
    return ReadStringFromFile(PROC_SELF_CMDLINE_PATH, buffer, bufferSz);
}

pid_t GetRealPid(void)
{
    return syscall(SYS_getpid);
}

uint64_t GetTimeMilliseconds(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * NUMBER_ONE_THOUSAND) + // 1000 : second to millisecond convert ratio
        (((uint64_t)ts.tv_nsec) / NUMBER_ONE_MILLION); // 1000000 : nanosecond to millisecond convert ratio
}

bool TrimAndDupStr(const char* src, char* dst)
{
    if (src == NULL || dst == NULL) {
        return false;
    }

    for (char ch = *src; ch != '\0' && ch != '\n';) {
        if (ch != ' ') {
            *dst++ = ch;
        }
        ch = *++src;
    }
    *dst = '\0';
    return true;
}

uint64_t GetAbsTimeMilliSeconds(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)(ts.tv_sec) * NUMBER_ONE_THOUSAND) +
        ((uint64_t)(ts.tv_nsec) / NUMBER_ONE_MILLION);
}

void ParseSiValue(const siginfo_t* si, uint64_t* endTime, int* tid)
{
    const int flagOffset = 63;
    if (((uint64_t)si->si_value.sival_ptr & (1ULL << flagOffset)) != 0) {
        *endTime = (uint64_t)si->si_value.sival_ptr & (~(1ULL << flagOffset));
        *tid = 0;
    } else {
        *endTime = 0;
        *tid = si->si_value.sival_int;
    }
}
