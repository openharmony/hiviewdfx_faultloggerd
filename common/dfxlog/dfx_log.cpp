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

#ifdef DFX_LOG_USE_HILOG_BASE
#include <hilog_base/log_base.h>
#else
#include <hilog/log.h>
#endif
#include <cstdarg>
#include <cstdio>
#include <securec.h>
#include <unistd.h>
#include <fcntl.h>
#include "dfx_define.h"

static int g_debugFd = INVALID_FD;
static const Level CURRENT_LOG_LEVEL = Level::INFO;
#ifdef DFX_LOG_USE_DMESG
static int g_Fd = INVALID_FD;
#endif

#ifdef DFX_LOG_USE_DMESG
static void LogToDmesg(Level logLevel, const char *tag, const char *info)
{
    static const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    static const char *LOG_KLEVEL_STR[] = { "<7>", "<6>", "<4>", "<3>", "<3>" };

    if (UNLIKELY(g_Fd < 0)) {
        g_Fd = open("/dev/kmsg", O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    }
    char buf[LOG_BUF_LEN] = {0};
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "%s[pid=%d %d][%s][%s]%s",
        LOG_KLEVEL_STR[logLevel], getpid(), getppid(), tag, LOG_LEVEL_STR[logLevel], info) == -1) {
        close(g_Fd);
        g_Fd = -1;
        return;
    }
    if (write(g_Fd, buf, strlen(buf)) < 0) {
        close(g_Fd);
        g_Fd = -1;
    }
}
#endif

void InitDebugFd(int fd)
{
    g_debugFd = fd;
}

bool CheckDebugLevel(void)
{
    return Level::DEBUG >= CURRENT_LOG_LEVEL ? true : false;
}

int DfxLog(const Level logLevel, const unsigned int domain, const char* tag, const char *fmt, ...)
{
    if (logLevel < CURRENT_LOG_LEVEL) {
        return -1;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, fmt);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, fmt, args);
    va_end(args);
#ifdef DFX_LOG_USE_HILOG_BASE
    static const LogLevel LOG_LEVEL[] = { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
    if ((logLevel < Level::DEBUG) || (logLevel > Level::FATAL)) {
        return -1;
    }
    HiLogBasePrint(LOG_CORE, LOG_LEVEL[logLevel], domain, tag, "%{public}s", buf);
#else
    OHOS::HiviewDFX::HiLogLabel LOG_LABEL = {LOG_CORE, domain, tag};
    switch (static_cast<int>(logLevel)) {
        case static_cast<int>(DEBUG):
            OHOS::HiviewDFX::HiLog::Debug(LOG_LABEL, "%{public}s", buf);
            break;
        case static_cast<int>(INFO):
            OHOS::HiviewDFX::HiLog::Info(LOG_LABEL, "%{public}s", buf);
            break;
        case static_cast<int>(WARN):
            OHOS::HiviewDFX::HiLog::Warn(LOG_LABEL, "%{public}s", buf);
            break;
        case static_cast<int>(ERROR):
            OHOS::HiviewDFX::HiLog::Error(LOG_LABEL, "%{public}s", buf);
            break;
        case static_cast<int>(FATAL):
            OHOS::HiviewDFX::HiLog::Fatal(LOG_LABEL, "%{public}s", buf);
            break;
        default:
            break;
    }
#endif

#ifdef DFX_LOG_USE_DMESG
    LogToDmesg(logLevel, tag, buf);
#endif
    if (g_debugFd != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}
