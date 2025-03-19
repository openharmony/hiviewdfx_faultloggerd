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

#include "kernel_snapshot_util.h"

#include "parameters.h"

#include "dfx_log.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace KernelSnapshotUtil {
namespace {
constexpr const char * const KERNEL_SNAPSHOT_REASON = "CppCrashKernelSnapshot";
}

std::string FilterEmptySection(const std::string& secHead, const std::string& secCont, const std::string& end)
{
    if (secCont.empty()) {
        return "";
    }
    return secHead + secCont + end;
}

std::string FormatTimestamp(const std::string& timestamp)
{
    uint64_t time = strtoul(timestamp.c_str(), nullptr, DECIMAL_BASE);
    if (errno == ERANGE) {
        DFXLOGE("Failed to convert timestamp to uint64_t");
        time = 0;
    }
    return GetCurrentTimeStr(time);
}

std::string GetBuildInfo()
{
    static std::string buildInfo = OHOS::system::GetParameter("const.product.software.version", "Unknown");
    return buildInfo;
}

std::string FillSummary(CrashMap& output, bool isLocal)
{
    std::string summary;
    summary += FilterEmptySection("Build info: ", GetBuildInfo(), "\n");
    summary += FilterEmptySection("Timestamp: ", FormatTimestamp(output[CrashSection::TIME_STAMP]), "");
    summary += FilterEmptySection("Pid: ", output[CrashSection::PID], "\n");
    summary += FilterEmptySection("Uid: ", output[CrashSection::UID], "\n");
    summary += FilterEmptySection("Process name: ", output[CrashSection::PROCESS_NAME], "\n");
    summary += FilterEmptySection("Reason: ", KERNEL_SNAPSHOT_REASON, "\n");
    summary += FilterEmptySection("Exception registers:\n", output[CrashSection::EXCEPTION_REGISTERS], "");
    summary += FilterEmptySection("Fault thread info:\n", output[CrashSection::FAULT_THREAD_INFO], "");
    summary += FilterEmptySection("Registers:\n", output[CrashSection::CREGISTERS], "");
    if (isLocal) {
        summary += FilterEmptySection("Memory near registers:\n", output[CrashSection::MEMORY_NEAR_REGISTERS], "");
        summary += FilterEmptySection("FaultStack:\n", output[CrashSection::FAULT_STACK], "");
    }
    summary += FilterEmptySection("Elfs:\n", output[CrashSection::MAPS], "");
    return summary;
}
}
} // namespace HiviewDFX
} // namespace OHOS
