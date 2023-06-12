/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <fcntl.h>
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <securec.h>
#include <stdio.h>
#include <string.h>
#include "dfx_define.h"

static const char PID_STR_NAME[] = "Pid:";

static bool ReadStringFromFile(const char* path, char* dst, size_t dstSz)
{
    if ((dst == NULL) || (path == NULL)) {
        return false;
    }
    char name[NAME_LEN];
    char nameFilter[NAME_LEN];
    (void)memset_s(name, sizeof(name), '\0', sizeof(name));
    (void)memset_s(nameFilter, sizeof(nameFilter), '\0', sizeof(nameFilter));

    int fd = -1;
    fd = OHOS_TEMP_FAILURE_RETRY(open(path, O_RDONLY));
    if (fd < 0) {
        return false;
    }

    ssize_t nRead = OHOS_TEMP_FAILURE_RETRY(read(fd, name, NAME_LEN - 1));
    if (nRead <= 0) {
        close(fd);
        return false;
    }

    char* p = name;
    int i = 0;
    while (*p != '\0') {
        if ((*p == '\n') || (i == NAME_LEN)) {
            break;
        }
        nameFilter[i] = *p;
        p++, i++;
    }
    nameFilter[NAME_LEN - 1] = '\0';

    if (memcpy_s(dst, dstSz, nameFilter, strlen(nameFilter) + 1) != 0) {
        close(fd);
        return false;
    }

    close(fd);
    return true;
}

bool GetThreadName(char* buffer, size_t bufferSz)
{
    return ReadStringFromFile(PROC_SELF_COMM_PATH, buffer, bufferSz);
}

bool GetProcessName(char* buffer, size_t bufferSz)
{
    return ReadStringFromFile(PROC_SELF_CMDLINE_PATH, buffer, bufferSz);
}

pid_t GetRealPid(void)
{
    pid_t pid = syscall(SYS_getpid);
    int fd = OHOS_TEMP_FAILURE_RETRY(open(PROC_SELF_STATUS_PATH, O_RDONLY));
    if (fd < 0) {
        return pid;
    }

    char buf[LOG_BUF_LEN];
    int i = 0;
    char b;
    ssize_t nRead = 0;
    while (1) {
        nRead = OHOS_TEMP_FAILURE_RETRY(read(fd, &b, sizeof(char)));
        if (nRead <= 0 || b == '\0') {
            break;
        }

        if (b == '\n' || i == LOG_BUF_LEN) {
            if (strncmp(buf, PID_STR_NAME, strlen(PID_STR_NAME)) == 0) {
                (void)sscanf_s(buf, "%*[^0-9]%d", &pid);
                break;
            }
            i = 0;
            (void)memset_s(buf, sizeof(buf), '\0', sizeof(buf));
            continue;
        }
        buf[i] = b;
        i++;
    }
    close(fd);
    return pid;
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
    if ((src == NULL) || (dst == NULL)) {
        return false;
    }

    size_t i = 0, j = 0;
    for (; i < strlen(src); ++i) {
        if (src[i] != ' ') {
            dst[j++] = src[i];
        }
    }
    for (; j <= i; j++) {
        dst[j] = '\0';
    }

    dst = strchr(dst, '\n');
    if (dst != NULL) {
        *dst = '\0';
    }
    return true;
}