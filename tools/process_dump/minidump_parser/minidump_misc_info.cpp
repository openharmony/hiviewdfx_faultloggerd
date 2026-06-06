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

#include <algorithm>
#include <securec.h>

#include "dfx_log.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpMiscInfo::MinidumpMiscInfo(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader)
{
}

bool MinidumpMiscInfo::Read(uint32_t expectedSize)
{
    isValid_ = false;
    uint32_t sizeOfInfo = 0;

    if (!memoryReader_->ReadBytes(&sizeOfInfo, sizeof(sizeOfInfo))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ,
            std::string("Cannot read misc info size"), __LINE__);
        DFXLOGE("MDRawMiscInfo cannot read sizeOfInfo");
        return false;
    }

    if (!memoryReader_->SeekSet(memoryReader_->Tell() - static_cast<off_t>(sizeof(sizeOfInfo)))) {
        return false;
    }

    size_t readSize = std::min(static_cast<size_t>(sizeOfInfo), sizeof(MDRawMiscInfo));
    (void)memset_s(&miscInfo_, sizeof(MDRawMiscInfo), 0, sizeof(MDRawMiscInfo));

    if (!memoryReader_->ReadBytes(&miscInfo_, readSize)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ,
            std::string("Cannot read misc info data"), __LINE__);
        DFXLOGE("MDRawMiscInfo cannot read misc info");
        return false;
    }

    isValid_ = true;
    lastError_.Clear();
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("MiscInfo", 1);
    }

    return true;
}

bool MinidumpMiscInfo::ProcessId(uint32_t& processId)
{
    if (!isValid_) {
        return false;
    }

    if (!(miscInfo_.flags1 & MD_MISCINFO_FLAGS1_PROCESS_ID)) {
        return false;
    }
    processId = miscInfo_.processId;
    return true;
}

bool MinidumpMiscInfo::ProcessTimes(uint32_t& processCreateTime,
    uint32_t& processUserTime, uint32_t& processKernelTime)
{
    if (!isValid_) {
        return false;
    }

    if (!(miscInfo_.flags1 & MD_MISCINFO_FLAGS1_PROCESS_TIMES)) {
        return false;
    }
    processCreateTime = miscInfo_.processCreateTime;
    processUserTime = miscInfo_.processUserTime;
    processKernelTime = miscInfo_.processKernelTime;
    return true;
}

void MinidumpMiscInfo::Print()
{
    if (!isValid_) {
        DFXLOGE("MDRawMiscInfo cannot print invalid data");
        return;
    }

    DFXLOGI("MDRawMiscInfo");
    DFXLOGI("  sizeOfInfo    = %{public}u", miscInfo_.sizeOfInfo);
    DFXLOGI("  flags1        = 0x%{public}x", miscInfo_.flags1);
    DFXLOGI("\n");
}
}
}