/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifdef DFX_LOG_DMESG
#include "dfx_log.h"
#include <securec.h>
#include <unistd.h>
#include <fcntl.h>
#include "dfx_define.h"

static int g_dmesgFd = INVALID_FD;

static void CloseDmesg()
{
    if (g_dmesgFd > 0) {
        close(g_dmesgFd);
        g_dmesgFd = -1;
    }
}

void LogToDmesg(const LogLevel logLevel, const char *tag, const char *info)
{
    static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    static const char *LOG_KLEVEL_STR[] = { "<7>", "<6>", "<4>", "<3>", "<3>" };
    int dmesgLevel = static_cast<int>(logLevel) - 2; // 2: DEBUG - FATAL

    if (UNLIKELY(g_dmesgFd < 0)) {
        g_dmesgFd = open("/dev/kmsg", O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    }
    char buf[LOG_BUF_LEN] = {0};
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%s[pid=%d %d][%s][%s]%s",
        LOG_KLEVEL_STR[dmesgLevel], getpid(), getppid(), tag, LOG_LEVEL_STR[dmesgLevel], info) < 0) {
        CloseDmesg();
        return;
    }
    if (write(g_dmesgFd, buf, strlen(buf)) < 0) {
        CloseDmesg();
    }
}
#endif