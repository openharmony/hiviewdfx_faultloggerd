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

#include "kernel_snapshot_printer.h"

#include "dfx_log.h"
#include "string_util.h"

#include "kernel_snapshot_util.h"
namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr const char * const KBOX_SNAPSHOT_DUMP_PATH = "/data/log/faultlog/temp/";
}

void KernelSnapshotPrinter::OutputToFile(const std::string& filePath, CrashMap& output)
{
    FILE* file = fopen(filePath.c_str(), "w");
    if (file == nullptr) {
        DFXLOGE("open file failed %{public}s errno %{public}d", filePath.c_str(), errno);
        return;
    }

    std::string outputCont = KernelSnapshotUtil::FillSummary(output, true);
    if (fwrite(outputCont.c_str(), sizeof(char), outputCont.length(), file) != outputCont.length()) {
        DFXLOGE("write file failed %{public}s errno %{public}d", filePath.c_str(), errno);
    }
    if (fclose(file) != 0) {
        DFXLOGE("close file failed %{public}s errno %{public}d", filePath.c_str(), errno);
    }
}

void KernelSnapshotPrinter::SaveSnapshot(CrashMap& output)
{
    if (output[CrashSection::PID].empty()) {
        DFXLOGE("pid is empty, not save snapshot");
        return;
    }

    std::string filePath = std::string(KBOX_SNAPSHOT_DUMP_PATH) + "cppcrash-" +
                            output[CrashSection::PID] + "-" +
                            output[CrashSection::TIME_STAMP];
    OutputToFile(filePath, output);
}

void KernelSnapshotPrinter::SaveSnapshots(std::vector<CrashMap>& outputs)
{
    for (auto& output : outputs) {
        SaveSnapshot(output);
    }
}
} // namespace HiviewDFX
} // namespace OHOS
