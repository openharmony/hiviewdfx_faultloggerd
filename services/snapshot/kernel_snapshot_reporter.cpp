/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "kernel_snapshot_reporter.h"

#include <dlfcn.h>

#include "dfx_log.h"
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#include "hisysevent_c.h"
#endif

#include "kernel_snapshot_content_builder.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
void KernelSnapshotReporter::ReportEvents(std::vector<CrashMap>& outputs, const std::string& snapshot)
{
    for (auto& output : outputs) {
        if (output[CrashSection::UID].empty()) {
            DFXLOGE("msg format fail, report raw msg");
            ReportRawMsg(snapshot);
            return;
        }
        ReportCrashNoLogEvent(output);
    }
}

bool KernelSnapshotReporter::ReportCrashNoLogEvent(CrashMap& output)
{
#ifndef HISYSEVENT_DISABLE
    int32_t uid = static_cast<int32_t>(strtol(output[CrashSection::UID].c_str(), nullptr, DECIMAL_BASE));
    int32_t pid = static_cast<int32_t>(strtol(output[CrashSection::PID].c_str(), nullptr, DECIMAL_BASE));
    int64_t timeStamp = strtoll(output[CrashSection::TIME_STAMP].c_str(), nullptr, DECIMAL_BASE);
    if (errno == ERANGE) {
        DFXLOGE("Failed to convert string to number, error: %{public}s", strerror(errno));
        return false;
    }

    int ret = HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::RELIABILITY, "CPP_CRASH_NO_LOG",
        OHOS::HiviewDFX::HiSysEvent::EventType::FAULT,
        "UID", uid,
        "PID", pid,
        "PROCESS_NAME", output[CrashSection::PROCESS_NAME],
        "HAPPEN_TIME", timeStamp,
        "SUMMARY", KernelSnapshotContentBuilder(output).GenerateSummary());
    DFXLOGI("Report pid %{public}d kernel snapshot complate event ret %{public}d", pid, ret);
    return ret == 0;
#else
    DFXLOGI("Not supported for kernel snapshot report.");
    return true;
#endif
}

int32_t KernelSnapshotReporter::GetSnapshotPid(const std::string& content)
{
    std::string pidKey = "pid=";
    auto pos = content.find(pidKey);
    if (pos == std::string::npos) {
        return 0;
    }
    auto end = content.find_first_of(')', pos);
    if (end == std::string::npos) {
        return 0;
    }
    pos += pidKey.size();
    std::string pidStr = content.substr(pos, end - pos);
    auto pid = static_cast<int32_t>(strtol(pidStr.c_str(), nullptr, 10));
    if (errno == ERANGE) {
        return 0;
    }
    return pid;
}

bool KernelSnapshotReporter::ReportRawMsg(const std::string& content)
{
    if (content.empty()) {
        return false;
    }
#ifndef HISYSEVENT_DISABLE
    int32_t pid = GetSnapshotPid(content);
    char procName[] = "encrypt_snapshot_proc";
    HiSysEventParam params[] = {
        {.name = "PID", .t = HISYSEVENT_UINT32, .v = { .ui32 = pid}, .arraySize = 0},
        {.name = "PROCESS_NAME", .t = HISYSEVENT_STRING, .v = {.s = procName}, .arraySize = 0},
        {.name = "HAPPEN_TIME", .t = HISYSEVENT_UINT64, .v = {.ui64 = GetTimeMilliSeconds()}, .arraySize = 0},
    };
    int ret = OH_HiSysEvent_Write("RELIABILITY", "CPP_CRASH_NO_LOG",
                                  HISYSEVENT_FAULT, params, sizeof(params) / sizeof(params[0]));
    DFXLOGI("Report pid %{public}d kernel snapshot raw event ret %{public}d", pid, ret);
    return ret == 0;
#else
    DFXLOGI("Not supported for kernel snapshot report.");
    return true;
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
