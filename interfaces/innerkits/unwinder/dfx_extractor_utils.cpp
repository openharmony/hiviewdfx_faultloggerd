/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_map.h"
#ifdef HAS_OHOS_HAP_EXTRACTOR
#include "extractor.h"
#include "string_util.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxExtractor"
}

DfxExtractor::DfxExtractor(const std::string& file)
{
    Init(file);
}

bool DfxExtractor::Init(const std::string& file)
{
    bool ret = false;
#ifdef HAS_OHOS_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        zipFile_ = std::make_shared<AbilityBase::ZipFile>(file);
    }
    if (zipFile_ != nullptr) {
        if (!zipFile_->Open()) {
            LOGE("Failed to open zip file(%s)", file.c_str());
            return false;
        }
    }
    LOGU("Done load zip file %s", file.c_str());
    ret = true;
#endif
    return ret;
}

void DfxExtractor::Clear()
{
#ifdef HAS_OHOS_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        return;
    }
    zipFile_ = nullptr;
#endif
}

bool DfxExtractor::GetHapInfos(std::vector<HapExtractorInfo>& hapInfos)
{
    hapInfos.clear();
#ifdef HAS_OHOS_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        return false;
    }

    auto &entrys = zipFile_->GetAllEntries();
    for (const auto &entry : entrys) {
        std::string fileName = entry.first;
        if (!EndsWith(fileName, ".abc")) {
            continue;
        }

        AbilityBase::ZipPos offset = 0;
        uint32_t length = 0;
        if (!zipFile_->GetDataOffsetRelative(fileName, offset, length)) {
            break;
        }

        LOGU("Hap entry file: %s, offset: 0x%016" PRIx64 ", length: %u", fileName.c_str(), (uint64_t)offset, length);
        HapExtractorInfo hapInfo;
        hapInfo.loadOffset = static_cast<uintptr_t>(offset);

        std::unique_ptr<uint8_t[]> abcDataPtr;
        size_t abcDataSize;
        if (zipFile_->ExtractToBufByName(fileName, abcDataPtr, abcDataSize)) {
            LOGU("Hap abc: %s, size: %zu", fileName.c_str() , abcDataSize);
            hapInfo.abcData = abcDataPtr.get();
            hapInfo.abcDataSize = abcDataSize;
        }
        hapInfos.emplace_back(hapInfo);
    }
#endif
    return (hapInfos.size() > 0);
}

bool DfxExtractor::GetHapAbcInfo(uint64_t pc, std::shared_ptr<DfxMap> map, std::vector<HapExtractorInfo> hapInfos,
    HapExtractorInfo& hapInfo)
{
    if (hapInfos.empty()) {
        return false;
    }
    if (hapInfos.size() == 1) {
        hapInfo = hapInfos.back();
        return true;
    }

    if (map == nullptr) {
        return false;
    }

    uint64_t relPc = (pc - map->begin + map->offset);
    LOGU("relPc: %" PRIx64 "", relPc);
    std::sort(hapInfos.begin(), hapInfos.end(), [](const HapExtractorInfo& info1, const HapExtractorInfo& info2) {
        return info1.loadOffset < info2.loadOffset;
    });
    hapInfos.shrink_to_fit();
    if (relPc < hapInfos.front().loadOffset) {
        return false;
    }
    for (size_t i = 0; i < hapInfos.size(); ++i) {
        if ((relPc >= hapInfos[i].loadOffset) && (relPc < hapInfos[i + 1].loadOffset)) {
            hapInfo = hapInfos[i];
            return true;
        }
    }
    return false;
}

bool DfxExtractor::GetHapAbcInfo(uintptr_t& loadOffset,
    std::unique_ptr<uint8_t[]>& abcDataPtr, size_t& abcDataSize)
{
    bool ret = false;
#ifdef HAS_OHOS_HAP_EXTRACTOR
    if (zipFile_ == nullptr) {
        return false;
    }

    auto &entrys = zipFile_->GetAllEntries();
    for (const auto &entry : entrys) {
        std::string fileName = entry.first;
        if (!EndsWith(fileName, ".abc")) {
            continue;
        }

        AbilityBase::ZipPos offset = 0;
        uint32_t length = 0;
        if (!zipFile_->GetDataOffsetRelative(fileName, offset, length)) {
            break;
        }

        LOGU("Hap entry file: %s, offset: 0x%016" PRIx64 ", length: %u", fileName.c_str(), (uint64_t)offset, length);
        loadOffset = static_cast<uintptr_t>(offset);
        if (zipFile_->ExtractToBufByName(fileName, abcDataPtr, abcDataSize)) {
            LOGU("Hap abc: %s, size: %zu", fileName.c_str() , abcDataSize);
            ret = true;
            break;
        }
    }
#endif
    return ret;
}

bool DfxExtractor::GetHapSourceMapInfo(uintptr_t& loadOffset,
    std::unique_ptr<uint8_t[]>& sourceMapPtr, size_t& sourceMapSize)
{
    bool ret = false;
#ifdef HAS_OHOS_HAP_EXTRACTOR
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
        if (!zipFile_->GetDataOffsetRelative(fileName, offset, length)) {
            break;
        }

        LOGU("Hap entry file: %s, offset: 0x%016" PRIx64 ", length: %u", fileName.c_str(), (uint64_t)offset, length);
        loadOffset = static_cast<uintptr_t>(offset);
        if (zipFile_->ExtractToBufByName(fileName, sourceMapPtr, sourceMapSize)) {
            LOGU("Hap sourcemap: %s, size: %zu", fileName.c_str(), sourceMapSize);
            ret = true;
            break;
        }
    }
#endif
    return ret;
}
}   // namespace HiviewDFX
}   // namespace OHOS
