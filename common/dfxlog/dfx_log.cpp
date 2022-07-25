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

#ifndef UNLIKELY
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)
#endif

#ifdef DFX_LOG_USE_HILOG_BASE
static LogLevel g_LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
#else
static const OHOS::HiviewDFX::HiLogLabel g_LOG_LABEL = {LOG_CORE, 0xD002D20, "DfxFaultLogger"};
#endif

static const Level g_logLevel = Level::DEBUG;
static const int LOG_BUF_LEN = 1024;
#ifdef DFX_LOG_USE_DMESG
static const int MAX_LOG_SIZE = 1024;
static int g_fd = -1;
#endif

#ifdef DFX_LOG_USE_DMESG
void LogToDmesg(Level logLevel, const char *tag, const char *info)
{
    static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    static const char *LOG_KLEVEL_STR[] = { "<7>", "<6>", "<4>", "<3>", "<3>" };

    if (UNLIKELY(g_fd < 0)) {
        g_fd = open("/dev/kmsg", O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    }
    char logInfo[MAX_LOG_SIZE];
    if (snprintf_s(logInfo, MAX_LOG_SIZE, MAX_LOG_SIZE - 1, "%s[pid=%d %d][%s][%s]%s",
        LOG_KLEVEL_STR[logLevel], getpid(), getppid(), tag, LOG_LEVEL_STR[logLevel], info) == -1) {
        close(g_fd);
        g_fd = -1;
        return;
    }
    if (write(g_fd, logInfo, strlen(logInfo)) < 0) {
        close(g_fd);
        g_fd = -1;
    }
}
#endif

bool CheckDebugLevel(void)
{
    return Level::DEBUG >= g_logLevel ? true : false;
}

int DfxLog(const Level logLevel, const unsigned int domain, const char* tag, const char *fmt, ...)
{
    if (logLevel < g_logLevel) {
        return -1;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, fmt);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, fmt, args);
    va_end(args);
#ifdef DFX_LOG_USE_HILOG_BASE
    if ((logLevel < DEBUG) || (logLevel > FATAL)) {
        return -1;
    }
    HiLogBasePrint(LOG_CORE, g_LOG_LEVEL[logLevel], domain, tag, "%{public}s", buf);
#else
    switch ((int)logLevel) {
        case (int)DEBUG:
            OHOS::HiviewDFX::HiLog::Debug(g_LOG_LABEL, "%{public}s", buf);
            break;
        case (int)INFO:
            OHOS::HiviewDFX::HiLog::Info(g_LOG_LABEL, "%{public}s", buf);
            break;
        case (int)WARN:
            OHOS::HiviewDFX::HiLog::Warn(g_LOG_LABEL, "%{public}s", buf);
            break;
        case (int)ERROR:
            OHOS::HiviewDFX::HiLog::Error(g_LOG_LABEL, "%{public}s", buf);
            break;
        case (int)FATAL:
            OHOS::HiviewDFX::HiLog::Fatal(g_LOG_LABEL, "%{public}s", buf);
            break;
        default:
            break;
    }
#endif

#ifdef DFX_LOG_USE_DMESG
    LogToDmesg(logLevel, tag, buf);
#endif
    return ret;
}