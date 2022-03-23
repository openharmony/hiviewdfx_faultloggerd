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

/* This files contains faultlog secure module. */

#include "fault_logger_secure.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <ctime>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <cstdlib>
#include <cstdio>

#include <directory_ex.h>
#include <file_ex.h>
#include <securec.h>
#include "dfx_log.h"

static const std::string FaultLoggerSecure_TAG = "FaultLoggerSecure";

namespace OHOS {
namespace HiviewDFX {
FaultLoggerSecure::FaultLoggerSecure()
{
}

FaultLoggerSecure::~FaultLoggerSecure()
{
}

static int DelSpace(char *src)
{
    char* pos = src;
    unsigned int count = 0;

    while (*src != '\0') {
        if (*src != ' ') {
            *pos++ = *src;
        } else {
            count++;
        }
        src++;
    }
    *pos = '\0';
    return count;
}

bool FaultLoggerSecure::CheckUidAndPid(const int uid, const int32_t pid)
{
    bool ret = false;
    char resp[MAX_RESP_LEN] = { '\0' };
    char cmd[MAX_CMD_LEN] = { '\0' };

    DfxLogInfo("%s :: CheckUidAndPid :: uid(%d), pid(%d).\n",
        FaultLoggerSecure_TAG.c_str(), uid, (int)pid);

    errno_t err = memset_s(resp, sizeof(resp), '\0', sizeof(resp));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s resp failed..", __func__);
    }
    err = memset_s(cmd, sizeof(cmd), '\0', sizeof(cmd));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s cmd failed..", __func__);
    }
    auto pms = sprintf_s(cmd, sizeof(cmd), "/bin/ps -u %d -o PID", uid);
    if (pms <= 0) {
        return ret;
    }
    DfxLogInfo("%s :: CheckUidAndPid :: cmd(%s).\n",
        FaultLoggerSecure_TAG.c_str(), (char *)cmd);

    FILE *fp = popen(cmd, "r");
    if (fp == nullptr) {
        return ret;
    }

    int count = static_cast<int>(fread(resp, 1, MAX_RESP_LEN - 1, fp));
    pclose(fp);
    if (count < 0) {
        return ret;
    }

    DelSpace(reinterpret_cast<char *>(resp));

    char delim[] = "\n";
    char *buf;
    char *token = strtok_s(reinterpret_cast<char *>(resp), delim, &buf);
    if (token == nullptr) {
        return ret;
    }
    token = strtok_s(nullptr, delim, &buf);
    while (token != nullptr) {
        int tokenPID = atoi(token);
        if (pid == tokenPID) {
            ret = true;
            break;
        }
        token = strtok_s(nullptr, delim, &buf);
    }

    DfxLogInfo("%s :: CheckUidAndPid :: ret(%d).\n",
        FaultLoggerSecure_TAG.c_str(), ret);
    return ret;
}

bool FaultLoggerSecure::CheckCallerUID (const int callingUid, const int32_t pid)
{
    DfxLogInfo("%s :: CheckCallerUID, callingUid(%d), MAX_SYS_UID(%d).\n",
        FaultLoggerSecure_TAG.c_str(), callingUid, FaultLoggerSecure::MAX_SYS_UID);

    bool ret = false;
    if ((callingUid < 0) || (pid <= 0)) {
        return false;
    }

    // If caller's is BMS / root or caller's uid/pid is validate, just return true
    if ((callingUid == FaultLoggerSecure::BMS_UID) || \
        (callingUid == FaultLoggerSecure::ROOT_UID) ||\
        CheckUidAndPid(callingUid, pid)) {
        ret = true;
    } else {
        ret = false;
    }

    DfxLogInfo("%s :: CheckCallerUID :: ret(%d).\n",
        FaultLoggerSecure_TAG.c_str(), ret);
    return ret;
}
} // namespace HiviewDfx
} // namespace OHOS
