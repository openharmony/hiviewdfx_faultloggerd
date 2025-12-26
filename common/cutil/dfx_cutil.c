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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <securec.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <time.h>

#include <sys/time.h>

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
    if (snprintf_s(threadNamePath, sizeof(threadNamePath), sizeof(threadNamePath) - 1, "/proc/%d/comm", tid) <= 0) {
        return false;
    }
    return ReadStringFromFile(threadNamePath, buffer, bufferSz);
}

bool GetProcessName(char* buffer, size_t bufferSz)
{
    return ReadStringFromFile(PROC_SELF_CMDLINE_PATH, buffer, bufferSz);
}

uint64_t GetTimeMilliseconds(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * NUMBER_ONE_THOUSAND) + // 1000 : second to millisecond convert ratio
        (((uint64_t)ts.tv_nsec) / NUMBER_ONE_MILLION); // 1000000 : nanosecond to millisecond convert ratio
}

uint64_t GetAbsTimeMilliSecondsCInterce(void)
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

bool IsNoNewPriv(const char* statusPath)
{
    int fd = open(statusPath, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    char buf[LINE_BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(fd);
        return false;
    }
    buf[sizeof(buf) - 1] = '\0';
    close(fd);

    const char key[] = "NoNewPrivs";
    char *p = strstr(buf, key);
    bool result = false;
    if (p) {
        char *val = p + strlen(key);
        while (val < buf + sizeof(buf) - 1 && !isdigit(*val)) {
            val++;
        }
        result = (*val == '1');
    }
    return result;
}

bool SafeStrtol(const char* numStr, long* out, int base)
{
    if (numStr == NULL || *numStr == '\0') {
        return false;
    }
    char* endPtr = NULL;
    errno = 0;
    long val = strtol(numStr, &endPtr, base);
    if (endPtr == numStr || *endPtr != '\0' || errno == ERANGE) {
        return false;
    }
    if (val == 0 && numStr[0] != '0') {
        return false;
    }
    *out = val;
    return true;
}
