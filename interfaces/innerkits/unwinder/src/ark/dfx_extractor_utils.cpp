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

#include "dfx_extractor_utils.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {

bool DfxExtractor::GetHapAbcInfo(uintptr_t& loadOffset, std::unique_ptr<uint8_t[]>& abcDataPtr, size_t& abcDataSize)
{
    bool ret = false;
#ifdef ENABLE_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        return false;
    }

    auto &entrys = zipFile_->GetAllEntries();
    for (const auto &entry : entrys) {
        std::string fileName = entry.first;
        if (!EndsWith(fileName, "modules.abc")) {
            continue;
        }

        AbilityBase::ZipPos offset = 0;
        uint32_t length = 0;
        if (!zipFile_->GetDataOffsetRelative(entry.second, offset, length)) {
            break;
        }

        DFXLOGU("[%{public}d]: Hap entry file: %{public}s, offset: 0x%{public}016" PRIx64 "", __LINE__,
            fileName.c_str(), (uint64_t)offset);
        loadOffset = static_cast<uintptr_t>(offset);
        if (zipFile_->ExtractToBufByName(fileName, abcDataPtr, abcDataSize)) {
            DFXLOGU("Hap abc: %{public}s, size: %{public}zu", fileName.c_str(), abcDataSize);
            ret = true;
            break;
        }
    }
#endif
    return ret;
}

bool DfxExtractor::GetHapSourceMapInfo(uintptr_t& loadOffset, std::unique_ptr<uint8_t[]>& sourceMapPtr,
    size_t& sourceMapSize)
{
    bool ret = false;
#ifdef ENABLE_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        return false;
    }

    auto &entrys = zipFile_->GetAllEntries();
    for (const auto &entry : entrys) {
        std::string fileName = entry.first;
        if (!EndsWith(fileName, ".map")) {
            continue;
        }

        AbilityBase::ZipPos offset = 0;
        uint32_t length = 0;
        if (!zipFile_->GetDataOffsetRelative(entry.second, offset, length)) {
            break;
        }

        DFXLOGU("[%{public}d]: Hap entry file: %{public}s, offset: 0x%{public}016" PRIx64 "", __LINE__,
            fileName.c_str(), (uint64_t)offset);
        loadOffset = static_cast<uintptr_t>(offset);
        if (zipFile_->ExtractToBufByName(fileName, sourceMapPtr, sourceMapSize)) {
            DFXLOGU("Hap sourcemap: %{public}s, size: %{public}zu", fileName.c_str(), sourceMapSize);
            ret = true;
            break;
        }
    }
#endif
    return ret;
}

bool DfxExtractor::Init(const std::string& file)
{
#ifdef ENABLE_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        zipFile_ = std::make_shared<AbilityBase::ZipFile>(file);
    }
    if ((zipFile_ == nullptr) || (!zipFile_->Open())) {
        DFXLOGE("Failed to open zip file(%{public}s)", file.c_str());
        zipFile_ = nullptr;
        return false;
    }
    DFXLOGU("Done load zip file %{public}s", file.c_str());
    return true;
#endif
    return false;
}

void DfxExtractor::Clear()
{
#ifdef ENABLE_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        return;
    }
    zipFile_ = nullptr;
#endif
}
} // namespace HiviewDFX
} // namespace OHOS
