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
#include "zip_file.h"
#endif

namespace OHOS {
namespace HiviewDFX {
class DfxMap;

struct HapExtractorInfo {
    uintptr_t loadOffset;
    uint8_t* abcData;
    size_t abcDataSize;
};

class DfxExtractor final {
public:
    explicit DfxExtractor(const std::string& file);
    ~DfxExtractor() { Clear(); }

    bool GetHapInfos(std::vector<HapExtractorInfo>& hapInfos);
    bool GetHapAbcInfo(uint64_t pc, std::shared_ptr<DfxMap> map, std::vector<HapExtractorInfo> hapInfos,
        HapExtractorInfo& hapInfo);
    bool GetHapAbcInfo(uintptr_t& loadOffset, std::unique_ptr<uint8_t[]>& abcDataPtr, size_t& abcDataSize);
    bool GetHapSourceMapInfo(uintptr_t& loadOffset, std::unique_ptr<uint8_t[]>& sourceMapPtr, size_t& sourceMapSize);

protected:
    bool Init(const std::string& file);
    void Clear();

private:
#ifdef ENABLE_HAP_EXTRACTOR
    std::shared_ptr<AbilityBase::ZipFile> zipFile_ = nullptr;
#endif
};
}   // namespace HiviewDFX
}   // namespace OHOS
#endif