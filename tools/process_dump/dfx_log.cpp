/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <faultloggerd_client.h>
#include <hilog/log.h>
#include "bytrace.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D21
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxProcessDump"
#endif


#define LOG_LEVEL_ALL   1
#define LOG_LEVEL_TRACE 2
#define LOG_LEVEL_DBG   3
#define LOG_LEVEL_INFO  4
#define LOG_LEVEL_WARN  5
#define LOG_LEVEL_ERR   6
#define LOG_LEVEL_FATAL 7
#define LOG_LEVEL_OFF   8

#define CONF_LINE_SIZE 1024
#define NUMBER_SIXTEEN 16

static const int LOG_LEVEL = LOG_LEVEL_INFO;

static const int32_t INVALID_FD = -1;
static int32_t g_StdErrFilleDes = INVALID_FD;
static int32_t g_DebugLogFilleDes = INVALID_FD;

static const OHOS::HiviewDFX::HiLogLabel g_LOG_LABEL = {LOG_CORE, 0xD002D20, "FaultLoggerd"};
struct DisplayConfig g_DisplayConfig = {1, 1, 1};
int DfxLogDebug(const char *format, ...)
{
    if (LOG_LEVEL_DBG < LOG_LEVEL) {
        return 0;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, LOG_BUF_LEN, LOG_BUF_LEN - 1, format, args);
    va_end(args);

    OHOS::HiviewDFX::HiLog::Debug(g_LOG_LABEL, "%{public}s", buf);

    if (g_DebugLogFilleDes != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}

int DfxLogInfo(const char *format, ...)
{
    if (LOG_LEVEL_INFO < LOG_LEVEL) {
        return 0;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, LOG_BUF_LEN, LOG_BUF_LEN - 1, format, args);
    va_end(args);

    OHOS::HiviewDFX::HiLog::Info(g_LOG_LABEL, "%{public}s", buf);

    if (g_DebugLogFilleDes != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}

int DfxLogWarn(const char *format, ...)
{
    if (LOG_LEVEL_WARN < LOG_LEVEL) {
        return 0;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, LOG_BUF_LEN, LOG_BUF_LEN - 1, format, args);
    va_end(args);

    OHOS::HiviewDFX::HiLog::Warn(g_LOG_LABEL, "%{public}s", buf);

    if (g_DebugLogFilleDes != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}

int DfxLogError(const char *format, ...)
{
    if (LOG_LEVEL_ERR < LOG_LEVEL) {
        return 0;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, LOG_BUF_LEN, LOG_BUF_LEN - 1, format, args);
    va_end(args);

    OHOS::HiviewDFX::HiLog::Error(g_LOG_LABEL, "%{public}s", buf);

    if (g_DebugLogFilleDes != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}

int DfxLogFatal(const char *format, ...)
{
    if (LOG_LEVEL_FATAL < LOG_LEVEL) {
        return 0;
    }

    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, LOG_BUF_LEN, LOG_BUF_LEN - 1, format, args);
    va_end(args);

    OHOS::HiviewDFX::HiLog::Fatal(g_LOG_LABEL, "%{public}s", buf);

    if (g_DebugLogFilleDes != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    return ret;
}

void DfxLogByTrace(bool start, const char *tag)
{
    if (start) {
        StartTrace(BYTRACE_TAG_OHOS, tag);
    } else {
        FinishTrace(BYTRACE_TAG_OHOS);
    }
}

int WriteLog(int32_t fd, const char *format, ...)
{
    int ret;
    char buf[LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, LOG_BUF_LEN, LOG_BUF_LEN - 1, format, args);
    va_end(args);

    OHOS::HiviewDFX::HiLog::Error(g_LOG_LABEL, "%{public}s", buf);

    if (g_DebugLogFilleDes != INVALID_FD) {
        fprintf(stderr, "%s\n", buf);
    }
    dprintf(fd, "%s", buf);
    return ret;
}

void DfxLogToSocket(const char *msg)
{
    int length = strlen(msg);
    if (length >= LOG_BUF_LEN) {
        return;
    }
    RequestPrintTHilog(msg, length);
}


void InitDebugLog(int type, int pid, int tid, int uid)
{
    if (g_DebugLogFilleDes != INVALID_FD) {
        return;
    }

    do {
        FILE *fp = nullptr;
        char line[CONF_LINE_SIZE] = {0};
        bool logPersist = false;

        fp = fopen("/system/etc/faultlogger.conf", "r");
        if (fp == nullptr) {
            break;
        }
        while (!feof(fp)) {
            if (fgets(line, CONF_LINE_SIZE - 1, fp) == nullptr) {
                continue;
            }
            if (!strncmp("faultlogLogPersist=true", line, strlen("faultlogLogPersist=true"))) {
                logPersist = true;
                continue;
            }
            if (!strncmp("displayRigister=false", line, strlen("displayRigister=false"))) {
                g_DisplayConfig.displayRigister = 0;
                continue;
            }
            if (!strncmp("displayBacktrace=false", line, strlen("displayBacktrace=false"))) {
                g_DisplayConfig.displayBacktrace = 0;
                continue;
            }
            if (!strncmp("displayMaps=false", line, strlen("displayMaps=false"))) {
                g_DisplayConfig.displayMaps = 0;
                continue;
            }
        }
        fclose(fp);

        if (logPersist == false) {
            return;
        }
    } while (0);

    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        return;
    }
    faultloggerdRequest.type = (int)type;
    faultloggerdRequest.pid = pid;
    faultloggerdRequest.tid = tid;
    faultloggerdRequest.uid = uid;

    g_DebugLogFilleDes = RequestLogFileDescriptor(&faultloggerdRequest);
    if (g_DebugLogFilleDes <= 0) {
        DfxLogError("InitDebugLog :: RequestLogFileDescriptor failed.");
        g_DebugLogFilleDes = INVALID_FD;
    } else {
        g_StdErrFilleDes = dup(STDERR_FILENO);

        if (dup2(g_DebugLogFilleDes, STDERR_FILENO) == -1) {
            DfxLogError("InitDebugLog :: dup2 failed.");
            close(g_DebugLogFilleDes);
            g_DebugLogFilleDes = INVALID_FD;
            g_StdErrFilleDes = INVALID_FD;
        }
    }
}

void CloseDebugLog()
{
    if (g_DebugLogFilleDes == INVALID_FD) {
        return;
    }

    if (g_StdErrFilleDes != INVALID_FD) {
        dup2(g_StdErrFilleDes, STDERR_FILENO);
    }
    close(g_DebugLogFilleDes);
    g_DebugLogFilleDes = INVALID_FD;
    g_StdErrFilleDes = INVALID_FD;
}

