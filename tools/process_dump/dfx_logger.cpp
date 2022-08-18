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
#include "dfx_logger.h"

#include <cstdio>
#include <securec.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "faultloggerd_client.h"

static const int WRITE_LOG_BUF_LEN = 2048;
static const int32_t INVALID_FD = -1;
static int32_t g_DebugLogFilleDes = INVALID_FD;
#ifndef DFX_LOG_USE_HILOG_BASE
static int32_t g_StdErrFilleDes = INVALID_FD;
#endif

int WriteLog(int32_t fd, const char *format, ...)
{
    int ret;
    char buf[WRITE_LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    if (ret == -1) {
        DfxLogWarn("WriteLog: vsnprintf_s fail");
    }
    va_end(args);
    if (fd == -1) {
        ret = DfxLogDebug(buf);
    }

    if (g_DebugLogFilleDes != INVALID_FD) {
        fprintf(stderr, "%s", buf);
    }

    if (fd != -1) {
        ret = dprintf(fd, "%s", buf);
        if (ret < 0) {
            DfxLogError("WriteLog :: write msg(%s) to fd(%d) failed, ret(%d).", buf, fd, ret);
        }
    }

    return ret;
}

void DfxLogToSocket(const char *msg)
{
    if (CheckDebugLevel()) {
        return;
    }

    size_t length = strlen(msg);
    if (length >= LOG_BUF_LEN) {
        return;
    }
    RequestPrintTHilog(msg, length);
}

#ifndef DFX_LOG_USE_HILOG_BASE
void InitDebugLog(int type, int pid, int tid, unsigned int uid, int isLogPersist)
{
    DfxLogInfo("InitDebugLog :: type(%d), pid(%d), tid(%d), uid(%d), isLogPersist(%d).",
        type, pid, tid, uid, isLogPersist);
    if (g_DebugLogFilleDes != INVALID_FD) {
        return;
    }
    if (!isLogPersist) {
        return;
    }

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

    InitDebugFd(g_DebugLogFilleDes);
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

    InitDebugFd(g_DebugLogFilleDes);
}
#endif