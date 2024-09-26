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

int WriteLog(int32_t fd, const char *format, ...)
{
    int ret = -1;
    char buf[WRITE_LOG_BUF_LEN] = {0};
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    if (ret == -1) {
        DFXLOGW("WriteLog: vsnprintf_s fail");
    }
    va_end(args);

    if (fd >= 0) {
        ret = dprintf(fd, "%s", buf);
        if (ret < 0) {
            DFXLOGE("WriteLog :: write msg(%{public}s) to fd(%{public}d) failed, ret(%{public}d).", buf, fd, ret);
        }
    } else if (fd == INVALID_FD) {
        DFXLOGW("%{public}s", buf);
    } else {
        DFXLOGD("%{public}s", buf);
    }

    return ret;
}

void DfxLogToSocket(const char *msg)
{
    size_t length = strlen(msg);
    if (length >= LOG_BUF_LEN) {
        return;
    }
    int ret = RequestPrintTHilog(msg, length);
    if (ret < 0) {
        DFXLOGE("DfxLogToSocket :: request print msg(%{public}s) failed, ret(%{public}d).", msg, ret);
    }
}
