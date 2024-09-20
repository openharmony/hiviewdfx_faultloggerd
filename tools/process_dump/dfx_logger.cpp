/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
#ifndef is_ohos_lite
static int32_t g_DebugLogFd = INVALID_FD;
static int32_t g_StdErrFd = INVALID_FD;
#endif

int WriteLog(int32_t fd, const char *format, ...)
{
    int ret = -1;
    char buf[WRITE_LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    if (ret == -1) {
        LOGWARN("%{public}s", "WriteLog: vsnprintf_s fail");
    }
    va_end(args);

#ifndef is_ohos_lite
    if (g_DebugLogFd != INVALID_FD) {
        fprintf(stderr, "%s", buf);
    }
#endif

    if (fd >= 0) {
        ret = dprintf(fd, "%s", buf);
        if (ret < 0) {
            LOGERROR("WriteLog :: write msg(%{public}s) to fd(%{public}d) failed, ret(%{public}d).", buf, fd, ret);
        }
    } else if (fd == INVALID_FD) {
        LOGWARN("%{public}s", buf);
    } else {
        LOGDEBUG("%{public}s", buf);
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
    int ret = RequestPrintTHilog(msg, length);
    if (ret < 0) {
        LOGERROR("DfxLogToSocket :: request print msg(%{public}s) failed, ret(%{public}d).", msg, ret);
    }
}

void InitDebugLog(int type, int pid, int tid, unsigned int uid)
{
#ifndef is_ohos_lite
    LOGINFO("InitDebugLog :: type(%{public}d), pid(%{public}d), tid(%{public}d), uid(%{public}d).",
        type, pid, tid, uid);
    if (g_DebugLogFd != INVALID_FD) {
        return;
    }

    struct FaultLoggerdRequest faultloggerdRequest;
    (void)memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest));
    faultloggerdRequest.type = (int)type;
    faultloggerdRequest.pid = pid;
    faultloggerdRequest.tid = tid;
    faultloggerdRequest.uid = uid;

    g_DebugLogFd = RequestLogFileDescriptor(&faultloggerdRequest);
    if (g_DebugLogFd <= 0) {
        LOGERROR("%{public}s", "InitDebugLog :: RequestLogFileDescriptor failed.");
        g_DebugLogFd = INVALID_FD;
    } else {
        g_StdErrFd = dup(STDERR_FILENO);

        if (dup2(g_DebugLogFd, STDERR_FILENO) == -1) {
            LOGERROR("%{public}s", "InitDebugLog :: dup2 failed.");
            close(g_DebugLogFd);
            g_DebugLogFd = INVALID_FD;
            g_StdErrFd = INVALID_FD;
        }
    }

    InitDebugFd(g_DebugLogFd);
#endif
}

void CloseDebugLog()
{
#ifndef is_ohos_lite
    if (g_DebugLogFd == INVALID_FD) {
        return;
    }

    if (g_StdErrFd != INVALID_FD) {
        dup2(g_StdErrFd, STDERR_FILENO);
    }
    close(g_DebugLogFd);
    g_DebugLogFd = INVALID_FD;
    g_StdErrFd = INVALID_FD;

    InitDebugFd(g_DebugLogFd);
#endif
}
