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

#include "kernel_snapshot_processor_impl.h"

#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
void KernelSnapshotProcessorImpl::Process(const std::string& snapshot)
{
    if (!parser_) {
        DFXLOGE("parser is null");
        return;
    }
    auto crashMaps = parser_->ParseAll(snapshot);

    if (!printer_) {
        DFXLOGE("printer is null");
        return;
    }
    printer_->SaveSnapshots(crashMaps);

    if (!reporter_) {
        DFXLOGE("reporter is null");
        return;
    }
    reporter_->ReportEvents(crashMaps, snapshot);
}
} // namespace HiviewDFX
} // namespace OHOS
