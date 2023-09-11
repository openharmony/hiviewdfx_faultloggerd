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

#include "dfx_log.h"
#include <cstdarg>
#include <cstdio>
#include <securec.h>
#include "dfx_define.h"

static int g_debugFd = INVALID_FD;
static LogLevel g_logLevel = LogLevel::LOG_INFO;
#ifdef DFX_LOG_DMESG
extern void LogToDmesg(const LogLevel logLevel, const char *tag, const char *info);
#endif

void InitDebugFd(int fd)
{
    g_debugFd = fd;
}

bool CheckDebugLevel(void)
{
    return LogLevel::LOG_DEBUG >= g_logLevel ? true : false;
}

void SetLogLevel(const LogLevel logLevel)
{
    g_logLevel = logLevel;
}

LogLevel GetLogLevel(void)
{
    return g_logLevel;
}

int DfxLogPrint(const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, ...)
{
#ifndef DFX_LOG_UNWIND
    if (logLevel < g_logLevel) {
        return -1;
    }
#endif

    va_list args;
    va_start(args, fmt);
    int ret = DFXLOG_PRINTV(logLevel, domain, tag, fmt, args);
    va_end(args);
    return ret;
}

int DfxLogPrintV(const LogLevel logLevel, const unsigned int domain, const char* tag, const char *fmt, va_list ap)
{
    if ((logLevel < LogLevel::LOG_DEBUG) || (logLevel > LogLevel::LOG_FATAL)) {
        return -1;
    }

    char buf[LOG_BUF_LEN] = {0};
    int ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, fmt, ap);
#ifdef DFX_LOG_HILOG_BASE
    ret = HiLogBasePrint(LOG_CORE, logLevel, domain, tag, "%{public}s", buf);
#else
    ret = HiLogPrint(LOG_CORE, logLevel, domain, tag, "%{public}s", buf);
#endif

#ifdef DFX_LOG_DMESG
    LogToDmesg(logLevel, tag, buf);
#endif

    if (g_debugFd != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}