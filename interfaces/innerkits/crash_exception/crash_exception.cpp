/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "crash_exception.h"

#include <map>
#include <regex>
#include <sys/time.h>
#include "dfx_errors.h"
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif

namespace OHOS {
namespace HiviewDFX {

static bool g_isInitProcessInfo = false;
static std::string g_crashProcessName = "";
static int32_t g_crashProcessPid = 0;
static int32_t g_crashProcessUid = 0;

uint64_t GetTimeMillisec(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * NUMBER_ONE_THOUSAND) +
            (((uint64_t)ts.tv_nsec) / NUMBER_ONE_MILLION);
}

void SetCrashProcInfo(std::string& name, int32_t pid, int32_t uid)
{
    if (pid <= 0) {
        return;
    }
    g_isInitProcessInfo = true;
    g_crashProcessName = name;
    g_crashProcessPid = pid;
    g_crashProcessUid = uid;
}

static const char* GetCrashDescription(const int32_t errCode)
{
    size_t i;

    for (i = 0; i < sizeof(g_crashExceptionMap) / sizeof(g_crashExceptionMap[0]); i++) {
        if (errCode == g_crashExceptionMap[i].errCode) {
            return g_crashExceptionMap[i].str;
        }
    }
    return g_crashExceptionMap[i - 1].str;    /* the end of map is "unknown reason" */
}

void ReportCrashException(const char* pName, int32_t pid, int32_t uid, int32_t errCode)
{
    if (pName == nullptr || strnlen(pName, NAME_BUF_LEN) == NAME_BUF_LEN) {
        return;
    }

    ReportCrashException(std::string(pName), pid, uid, errCode);
}

void ReportCrashException(std::string name, int32_t pid, int32_t uid, int32_t errCode)
{
#ifndef HISYSEVENT_DISABLE
    if (errCode == CrashExceptionCode::CRASH_ESUCCESS) {
        return;
    }
    HiSysEventWrite(
        HiSysEvent::Domain::RELIABILITY,
        "CPP_CRASH_EXCEPTION",
        HiSysEvent::EventType::FAULT,
        "PROCESS_NAME", name,
        "PID", pid,
        "UID", uid,
        "HAPPEN_TIME", GetTimeMillisec(),
        "ERROR_CODE", errCode,
        "ERROR_MSG", GetCrashDescription(errCode));
#endif
}

void ReportUnwinderException(uint16_t unwError)
{
    if (!g_isInitProcessInfo) {
        return;
    }

    const std::map<uint16_t, int32_t> unwMaps = {
        { UnwindErrorCode::UNW_ERROR_STEP_ARK_FRAME, CrashExceptionCode::CRASH_UNWIND_EFRAME },
        { UnwindErrorCode::UNW_ERROR_INVALID_CONTEXT, CrashExceptionCode::CRASH_UNWIND_ECONTEXT },
    };
    int32_t errCode = 0;
    auto iter = unwMaps.find(unwError);
    if (iter == unwMaps.end()) {
        return;
    }
    errCode = iter->second;
    ReportCrashException(g_crashProcessName, g_crashProcessPid, g_crashProcessUid, errCode);
}

bool CheckFaultSummaryValid(const std::string &summary)
{
    return (summary.find("#00") != std::string::npos) && (summary.find("#01") != std::string::npos) &&
           (summary.find("#02") != std::string::npos);
}

} // namespace HiviewDFX
} // namesapce OHOS
