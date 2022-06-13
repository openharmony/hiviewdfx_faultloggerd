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

#include "log_wrapper.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/time.h>

#include "hilog_base/log_base.h"
#include "securec.h"

#define MAX_LOG_SIZE 1024

void LogToDmesg(Level logLevel, const char *tag, const char *info)
{
    static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    static const char *LOG_KLEVEL_STR[] = { "<7>", "<6>", "<4>", "<3>", "<3>" };

    int fd = open("/dev/kmsg", O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    char logInfo[MAX_LOG_SIZE];
    if (snprintf_s(logInfo, MAX_LOG_SIZE, MAX_LOG_SIZE - 1, "%s[pid=%d %d][%s][%s]%s",
        LOG_KLEVEL_STR[logLevel], getpid(), getppid(), tag, LOG_LEVEL_STR[logLevel], info) == -1) {
        close(fd);
        return;
    }
    if (fd > 0) {
        write(fd, logInfo, strlen(logInfo));
        close(fd);
        return;
    }
}

void Log(Level logLevel, unsigned int domain, const char *tag, const char *fmt, ...)
{
    va_list vargs;
    va_start(vargs, fmt);
    char tmpFmt[MAX_LOG_SIZE] = {0};
    if (vsnprintf_s(tmpFmt, sizeof(tmpFmt), sizeof(tmpFmt) - 1, fmt, vargs) == -1) {
        va_end(vargs);
        return;
    }

    va_end(vargs);
    LogToDmesg(logLevel, tag, tmpFmt);
    static LogLevel LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
    HiLogBasePrint(LOG_CORE, LOG_LEVEL[logLevel], domain, tag, "%{public}s", tmpFmt);
}
