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
#ifndef DFX_MUSL_CUTIL_H
#define DFX_MUSL_CUTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>
#include "dfx_define.h"
#include "musl_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_MUSL_CUTIL
static const char PID_STR_NAME[] = "Pid:";

static bool ReadStringFromFile(const char* path, char* dst, size_t dstSz)
{
    char name[NAME_BUF_LEN];
    char nameFilter[NAME_BUF_LEN];
    memset(name, 0, sizeof(name));
    memset(nameFilter, 0, sizeof(nameFilter));

    int fd = -1;
    fd = OHOS_TEMP_FAILURE_RETRY(open(path, O_RDONLY));
    if (fd < 0) {
        return false;
    }

    int nRead = OHOS_TEMP_FAILURE_RETRY(read(fd, name, NAME_BUF_LEN -1));
    if (nRead == -1) {
        close(fd);
        return false;
    }

    char* p = name;
    int i = 0;
    while (*p != '\0') {
        if ((*p == '\n') || (i == NAME_BUF_LEN)) {
            break;
        }
        nameFilter[i] = *p;
        p++, i++;
    }
    nameFilter[NAME_BUF_LEN - 1] = '\0';

    size_t cpyLen = strlen(nameFilter) + 1;
    if (cpyLen > dstSz) {
        cpyLen = dstSz;
    }
    memcpy(dst, nameFilter, cpyLen);
    close(fd);
    return true;
}

bool GetThreadName(char* buffer, size_t bufferSz)
{
    return ReadStringFromFile(PROC_SELF_COMM_PATH, buffer, bufferSz);
}

bool GetThreadNameByTid(int32_t tid, char* buffer, size_t bufferSz)
{
    char threadNamePath[NAME_BUF_LEN] = { 0 };
    if (snprintf(threadNamePath, sizeof(threadNamePath), "/proc/%d/comm", tid) <= 0) {
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
    pid_t pid = syscall(SYS_getpid);
    int fd = OHOS_TEMP_FAILURE_RETRY(open(PROC_SELF_STATUS_PATH, O_RDONLY));
    if (fd < 0) {
        DFXLOG_ERROR("GetRealPid:: open failed! pid:(%ld), errno:(%d).", pid, errno);
        return pid;
    }

    char buf[LINE_BUF_SIZE];
    int i = 0;
    char b;
    ssize_t nRead = 0;
    while (1) {
        nRead = OHOS_TEMP_FAILURE_RETRY(read(fd, &b, sizeof(char)));
        if (nRead <= 0 || b == '\0') {
            DFXLOG_ERROR("GetRealPid:: read failed! pid:(%ld), errno:(%d).", pid, errno);
            break;
        }

        if (b == '\n' || i == LINE_BUF_SIZE) {
            if (strncmp(buf, PID_STR_NAME, strlen(PID_STR_NAME)) == 0) {
                (void)sscanf(buf, "%*[^0-9]%d", &pid);
                break;
            }
            i = 0;
            (void)memset(buf, '\0', sizeof(buf));
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

#endif
#ifdef __cplusplus
}
#endif
#endif