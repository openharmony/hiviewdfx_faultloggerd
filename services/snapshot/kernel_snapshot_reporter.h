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

#ifndef KERNEL_SNAPSHOT_REPORTER_H
#define KERNEL_SNAPSHOT_REPORTER_H

#include "i_kernel_snapshot_reporter.h"

namespace OHOS {
namespace HiviewDFX {
class KernelSnapshotReporter : public IKernelSnapshotReporter {
public:
    void ReportEvents(std::vector<CrashMap>& outputs, const std::string& snapshot) override;
private:
    bool ReportRawMsg(const std::string& content);
    bool ReportCrashNoLogEvent(CrashMap& output);
    int32_t GetSnapshotPid(const std::string& content);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
