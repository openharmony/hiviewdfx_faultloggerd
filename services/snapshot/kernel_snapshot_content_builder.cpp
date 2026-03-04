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

#include "kernel_snapshot_content_builder.h"

#include "parameter.h"
#include "parameters.h"

#include "dfx_log.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr const char * const KERNEL_SNAPSHOT_REASON = "CppCrashKernelSnapshot";

std::string FormatTimestamp(const std::string& timestamp)
{
    uint64_t time = strtoul(timestamp.c_str(), nullptr, DECIMAL_BASE);
    if (errno == ERANGE) {
        DFXLOGE("Failed to convert timestamp to uint64_t");
        time = 0;
    }
    return GetCurrentTimeStr(time);
}
} // namespace

bool KernelSnapshotContentBuilder::SnapshotUserSection::IsEmptyContent() const
{
    return content.empty();
}

size_t KernelSnapshotContentBuilder::SnapshotUserSection::Length() const
{
    if (IsEmptyContent()) {
        return 0;
    }
    return title.length() + content.length() + suffix.length();
}

void KernelSnapshotContentBuilder::SnapshotUserSection::WriteTo(std::string& output) const
{
    if (IsEmptyContent()) {
        return;
    }
    output += title;
    output += content;
    output += suffix;
}

std::string KernelSnapshotContentBuilder::GenerateSummary()
{
    contentSections_ = BuildSections();
    return ComposeSummary();
}

size_t KernelSnapshotContentBuilder::ComputeTotalLength() const
{
    size_t totalLength = 0;
    for (const auto& section : contentSections_) {
        if (!section.IsEmptyContent()) {
            totalLength += section.Length();
        }
    }
    return totalLength;
}

std::vector<KernelSnapshotContentBuilder::SnapshotUserSection> KernelSnapshotContentBuilder::BuildSections()
{
    std::vector<KernelSnapshotContentBuilder::SnapshotUserSection> sections = {
        {"Build info: ", GetDisplayVersion() != nullptr ? GetDisplayVersion() : "Unknown", "\n"},
        {"Timestamp: ", FormatTimestamp(crashData_[CrashSection::TIME_STAMP]), ""},
        {"Pid: ", crashData_[CrashSection::PID], "\n"},
        {"Uid: ", crashData_[CrashSection::UID], "\n"},
        {"Process name: ", crashData_[CrashSection::PROCESS_NAME], "\n"},
        {"Reason: ", KERNEL_SNAPSHOT_REASON, "\n"},
        {"Exception registers:\n", crashData_[CrashSection::EXCEPTION_REGISTERS], ""},
        {"Fault thread info:\n", crashData_[CrashSection::FAULT_THREAD_INFO], ""},
        {"Registers:\n", crashData_[CrashSection::CREGISTERS], ""}
    };
    if (isLocal_) {
        sections.emplace_back("Memory near registers:\n", crashData_[CrashSection::MEMORY_NEAR_REGISTERS], "");
        sections.emplace_back("FaultStack:\n", crashData_[CrashSection::FAULT_STACK], "");
    }
    sections.emplace_back("Elfs:\n", crashData_[CrashSection::MAPS], "");

    return sections;
}

std::string KernelSnapshotContentBuilder::ComposeSummary()
{
    const size_t totalLength = ComputeTotalLength();
    if (totalLength == 0) {
        return "";
    }

    std::string summary;
    summary.reserve(totalLength);

    for (const auto& section : contentSections_) {
        section.WriteTo(summary);
    }
    return summary;
}
} // namespace HiviewDFX
} // namespace OHOS
