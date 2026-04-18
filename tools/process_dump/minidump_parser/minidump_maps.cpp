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
#include "minidump_config.h"
#include "minidump_factory.h"
#include "minidump_memory_reader.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
MinidumpMapList::MinidumpMapList(std::shared_ptr<MinidumpMemoryReader> memoryReader)
    : MinidumpStream(memoryReader),
      contents_(),
      mapsLength_(0)
{
}

bool MinidumpMapList::Read(uint32_t expectedSize)
{
    isValid_ = false;
    contents_.clear();
    mapsLength_ = 0;

    if (expectedSize == 0 || expectedSize > MinidumpConfigManager::Instance().GetConfig().maxMemoryBytes) {
        DFXLOGE("MinidumpMapList expected size %{public}u invalid", expectedSize);
        return false;
    }

    // Read the entire content of the maps
    contents_.resize(expectedSize);
    if (!memoryReader_->ReadBytes(&contents_[0], expectedSize)) {
        DFXLOGE("MinidumpMapList cannot read map contents, size: %{public}u", expectedSize);
        return false;
    }
    mapsLength_ = expectedSize;

    while (!contents_.empty()) {
        char back = contents_.back();
        // If the tail is 0 or non-standard ASCII printed characters, simply pop them away
        if (back == '\0' || back == static_cast<char>(0xFF) || back == static_cast<char>(0xFE)) {
            contents_.pop_back();
            mapsLength_--;
        } else {
            break;
        }
    }
    contents_.push_back('\0');
    mapsLength_++;

    isValid_ = true;
    if (subject_ != nullptr) {
        subject_->NotifyStreamLoaded("MapList", mapsLength_);
    }
    return true;
}

void MinidumpMapList::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpMapList cannot print invalid data");
        return;
    }

    DFXLOGI("MinidumpMapList");
    DFXLOGI("  maps length = %{public}u bytes", mapsLength_);
    DFXLOGI("  maps contents = %{public}s", contents_.data());
}
}
}