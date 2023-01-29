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

/* This files contains faultlog secure module. */

#include "fault_logger_secure.h"

#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include <string>
#include "dfx_log.h"
#include "iosfwd"

namespace OHOS {
namespace HiviewDFX {
namespace {
static const std::string FAULTLOGGERSECURE_TAG = "FaultLoggerSecure";
static constexpr int32_t ROOT_UID = 0;
static constexpr int32_t BMS_UID = 1000;
static constexpr int32_t HIVIEW_UID = 1201;
static constexpr int32_t HIDUMPER_SERVICE_UID = 1212;
static constexpr int32_t MAX_RESP_LEN = 128;
static constexpr int32_t MAX_CMD_LEN = 1024;
}

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
        FAULTLOGGERSECURE_TAG.c_str(), uid, pid);

    errno_t err = memset_s(resp, sizeof(resp), '\0', sizeof(resp));
    if (err != EOK) {
        DfxLogError("%s :: memset_s resp failed..", __func__);
    }
    err = memset_s(cmd, sizeof(cmd), '\0', sizeof(cmd));
    if (err != EOK) {
        DfxLogError("%s :: memset_s cmd failed..", __func__);
    }
    auto pms = sprintf_s(cmd, sizeof(cmd), "/bin/ps -u %d -o PID", uid);
    if (pms <= 0) {
        return ret;
    }
    DfxLogInfo("%s :: CheckUidAndPid :: cmd(%s).\n", FAULTLOGGERSECURE_TAG.c_str(), cmd);

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
        FAULTLOGGERSECURE_TAG.c_str(), ret);
    return ret;
}

bool FaultLoggerSecure::CheckCallerUID(const int callingUid, const int32_t pid)
{
    bool ret = false;
    if ((callingUid < 0) || (pid <= 0)) {
        return false;
    }

    // If caller's is BMS / root or caller's uid/pid is validate, just return true
    if ((callingUid == BMS_UID) ||
        (callingUid == ROOT_UID) ||
        (callingUid == HIVIEW_UID) ||
        (callingUid == HIDUMPER_SERVICE_UID)) {
        ret = true;
    } else {
        ret = false;
        DfxLogWarn("%s :: CheckCallerUID :: callingUid(%d).\n", FAULTLOGGERSECURE_TAG.c_str(), callingUid);
    }
    return ret;
}
} // namespace HiviewDfx
} // namespace OHOS
