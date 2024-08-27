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
#ifndef DFX_EXTRACTOR_UTILS_H
#define DFX_EXTRACTOR_UTILS_H

#include <memory>
#include <vector>
#include "dfx_define.h"
#ifdef ENABLE_HAP_EXTRACTOR
#include "dfx_log.h"
#include "extractor.h"
#include "string_util.h"
#include "zip_file.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#ifdef ENABLE_HAP_EXTRACTOR
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxExtractor"
#endif
}

struct HapExtractorInfo {
    uintptr_t loadOffset;
    uint8_t* abcData;
    size_t abcDataSize;
};

class DfxExtractor final {
public:
    explicit DfxExtractor(const std::string& file) { Init(file); }
    ~DfxExtractor() { Clear(); }

    bool GetHapAbcInfo(uintptr_t& loadOffset, std::unique_ptr<uint8_t[]>& abcDataPtr, size_t& abcDataSize)
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

            LOGU("Hap entry file: %s, offset: 0x%016" PRIx64 "", fileName.c_str(), (uint64_t)offset);
            loadOffset = static_cast<uintptr_t>(offset);
            if (zipFile_->ExtractToBufByName(fileName, abcDataPtr, abcDataSize)) {
                LOGU("Hap abc: %s, size: %zu", fileName.c_str(), abcDataSize);
                ret = true;
                break;
            }
        }
#endif
        return ret;
    }

    bool GetHapSourceMapInfo(uintptr_t& loadOffset, std::unique_ptr<uint8_t[]>& sourceMapPtr, size_t& sourceMapSize)
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

            LOGU("Hap entry file: %s, offset: 0x%016" PRIx64 "", fileName.c_str(), (uint64_t)offset);
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

protected:
    bool Init(const std::string& file)
    {
#ifdef ENABLE_HAP_EXTRACTOR
        if (zipFile_ == nullptr) {
            zipFile_ = std::make_shared<AbilityBase::ZipFile>(file);
        }
        if ((zipFile_ == nullptr) || (!zipFile_->Open())) {
            LOGE("Failed to open zip file(%s)", file.c_str());
            zipFile_ = nullptr;
            return false;
        }
        LOGU("Done load zip file %s", file.c_str());
        return true;
#endif
        return false;
    }

    void Clear()
    {
#ifdef ENABLE_HAP_EXTRACTOR
        if (zipFile_ == nullptr) {
            return;
        }
        zipFile_ = nullptr;
#endif
    }

private:
#ifdef ENABLE_HAP_EXTRACTOR
    std::shared_ptr<AbilityBase::ZipFile> zipFile_ = nullptr;
#endif
};
}   // namespace HiviewDFX
}   // namespace OHOS
#endif