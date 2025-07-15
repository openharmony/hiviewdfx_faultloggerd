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
#endif

#include "kernel_snapshot_content_builder.h"

namespace OHOS {
namespace HiviewDFX {
void KernelSnapshotReporter::ReportEvents(std::vector<CrashMap>& outputs)
{
    for (auto& output : outputs) {
        ReportCrashNoLogEvent(output);
    }
}

bool KernelSnapshotReporter::ReportCrashNoLogEvent(CrashMap& output)
{
    if (output[CrashSection::UID].empty()) {
        DFXLOGE("uid is empty, not report");
        return false;
    }
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
    DFXLOGI("Report kernel snapshot event done, ret %{public}d", ret);
    return ret == 0;
#else
    DFXLOGI("Not supported for kernel snapshot report.");
    return true;
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
