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
#include <fcntl.h>
#include <securec.h>
#include <unistd.h>

#include "dfx_define.h"

static int g_debugFd = INVALID_FD;
static LogLevel g_logLevel = LogLevel::LOG_INFO;

#ifdef DFX_LOG_DMESG
static int g_dmesgFd = INVALID_FD;

static void CloseDmesg()
{
    if (g_dmesgFd > 0) {
        close(g_dmesgFd);
        g_dmesgFd = -1;
    }
}

static void LogToDmesg(const LogLevel logLevel, const char *tag, const char *info)
{
    static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    static const char *LOG_KLEVEL_STR[] = { "<7>", "<6>", "<4>", "<3>", "<3>" };
    int dmesgLevel = static_cast<int>(logLevel) - LOG_DEBUG;

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
    if (ret == -1) {
        return ret;
    }
#ifdef DFX_LOG_HILOG_BASE
    ret = HiLogBasePrint(LOG_CORE, logLevel, domain, tag, "%{public}s", buf);
#else
    ret = HiLogPrint(LOG_CORE, logLevel, domain, tag, "%{public}s", buf);
#endif
    if (ret < 0) {
        fprintf(stderr, "print to hilog failed\n");
    }

#ifdef DFX_LOG_DMESG
    LogToDmesg(logLevel, tag, buf);
#endif

    if (g_debugFd != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}