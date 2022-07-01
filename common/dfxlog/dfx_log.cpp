/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "dfx_log.h"

#include <cstdarg>
#include <cstdio>
#include <securec.h>
#include <unistd.h>
#include <fcntl.h>

enum class LOG_LEVEL_CLASS {
    LOG_LEVEL_ALL = 1,
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DBG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_OFF
};

static const int MAX_LOG_SIZE = 1024;
static const int LOG_BUF_LEN = 1024;

static const LOG_LEVEL_CLASS LOG_LEVEL = LOG_LEVEL_CLASS::LOG_LEVEL_INFO;
#ifndef DFX_LOG_USE_HILOG_BASE
static const OHOS::HiviewDFX::HiLogLabel g_LOG_LABEL = {LOG_CORE, 0xD002D20, "DfxFaultLogger"};
#endif

#ifdef INIT_DMESG
static int g_fd = -1;
static const int LEVEL_DEBUG_OFFSET = 3;
void OpenLogDevice(void)
{
    int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd >= 0) {
        g_fd = fd;
    }
    return;
}

void LogToDmesg(LOG_LEVEL_CLASS logLevel, const char *info)
{
    static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    static const char *LOG_KLEVEL_STR[] = { "<7>", "<6>", "<4>", "<3>", "<3>" };

    if (UNLIKELY(g_fd < 0)) {
        OpenLogDevice();
        if (g_fd < 0) {
            return;
        }
    }
    char logInfo[MAX_LOG_SIZE];
    if (snprintf_s(logInfo, sizeof(logInfo), sizeof(logInfo) - 1, "%s[pid=%d %d][%s]%s",
        LOG_KLEVEL_STR[(int)logLevel - LEVEL_DEBUG_OFFSET], getpid(), getppid(),
        LOG_LEVEL_STR[(int)logLevel - LEVEL_DEBUG_OFFSET], info) == -1) {
        close(g_fd);
        g_fd = -1;
        return;
    }
    if (write(g_fd, logInfo, strlen(logInfo)) < 0) {
        close(g_fd);
        g_fd = -1;
    }
    return;
}
#endif

namespace {
int DfxLog(LOG_LEVEL_CLASS logLevel, const char *format, va_list args)
{
#ifdef DFX_NO_PRINT_LOG
    return 0;
#endif
    if (logLevel < LOG_LEVEL) {
        return 0;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
#ifdef DFX_LOG_USE_HILOG_BASE
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s", buf);
#else
    OHOS::HiviewDFX::HiLog::Debug(g_LOG_LABEL, "%{public}s", buf);
#endif

#ifdef INIT_DMESG
    LogToDmesg(logLevel, buf);
#endif
    return ret;
}
}

int checkDebugLevel()
{
    return LOG_LEVEL_CLASS::LOG_LEVEL_DBG >= LOG_LEVEL ? 1 : 0;
}

int DfxLogDebug(const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = DfxLog(LOG_LEVEL_CLASS::LOG_LEVEL_DBG, format, args);
    va_end(args);

    return ret;
}

int DfxLogInfo(const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = DfxLog(LOG_LEVEL_CLASS::LOG_LEVEL_INFO, format, args);
    va_end(args);

    return ret;
}

int DfxLogWarn(const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = DfxLog(LOG_LEVEL_CLASS::LOG_LEVEL_WARN, format, args);
    va_end(args);

    return ret;
}

int DfxLogError(const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = DfxLog(LOG_LEVEL_CLASS::LOG_LEVEL_ERR, format, args);
    va_end(args);

    return ret;
}

int DfxLogFatal(const char *format, ...)
{
    int ret;
    va_list args;
    va_start(args, format);
    ret = DfxLog(LOG_LEVEL_CLASS::LOG_LEVEL_FATAL, format, args);
    va_end(args);

    return ret;
}
