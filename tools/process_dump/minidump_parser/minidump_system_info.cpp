/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "dfx_log.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpSystemInfo::MinidumpSystemInfo(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader)
{
}

bool MinidumpSystemInfo::Read(uint32_t expectedSize)
{
    isValid_ = false;

    if (!memoryReader_->ReadBytes(&systemInfo_, sizeof(systemInfo_))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ,
            std::string("Cannot read system info"), __LINE__);
        DFXLOGE("MDRawSystemInfo cannot read system info");
        return false;
    }

    isValid_ = true;
    lastError_.Clear();
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("SystemInfo", 1);
    }
    return true;
}

std::string MinidumpSystemInfo::GetOS()
{
    if (!isValid_) return "";

    switch (systemInfo_.platformId) {
        case MINIDUMP_OS_LINUX:
            return "linux";
        case MINIDUMP_OS_HONGMENG:
            return "hongmeng";
        default:
            return "unknown";
    }
}

std::string MinidumpSystemInfo::GetCPU()
{
    if (!isValid_) return "";

    switch (systemInfo_.processorArchitecture) {
        case MD_CPU_ARCH_ARM64:
            return "arm64";
        default:
            return "unknown";
    }
}

void MinidumpSystemInfo::Print()
{
    if (!isValid_) {
        DFXLOGE("MDRawSystemInfo cannot print invalid data");
        return;
    }

    DFXLOGI("MDRawSystemInfo");
    DFXLOGI("  processorArchitecture = %{public}d", systemInfo_.processorArchitecture);
    DFXLOGI("  processorLevel        = %{public}d", systemInfo_.processorLevel);
    DFXLOGI("  processorRevision     = %{public}d", systemInfo_.processorRevision);
    DFXLOGI("  numberOfProcessors    = %{public}d", systemInfo_.numberOfProcessors);
    DFXLOGI("  platformId            = %{public}d", systemInfo_.platformId);
    DFXLOGI("  os                    = %{public}s", GetOS().c_str());
    DFXLOGI("  cpu                   = %{public}s", GetCPU().c_str());
    DFXLOGI("\n");
}

}
}