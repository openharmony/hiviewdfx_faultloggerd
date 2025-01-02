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
#ifndef DFX_HAP_H
#define DFX_HAP_H

#include <cstdint>
#include <memory>
#include <unistd.h>
#include "dfx_ark.h"
#include "dfx_extractor_utils.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMap;

class DfxHap {
public:
    DfxHap() = default;
    ~DfxHap();

    bool ParseHapInfo(pid_t pid, uint64_t pc, std::shared_ptr<DfxMap> map, JsFunction *jsFunction);

private:
    bool ParseHapFileInfo(uint64_t pc, std::shared_ptr<DfxMap> map, JsFunction *jsFunction);
    bool ParseHapMemInfo(pid_t pid, uint64_t pc, std::shared_ptr<DfxMap> map,
        JsFunction *jsFunction);

    bool ParseHapFileData(const std::string& name);
    bool ParseHapMemData(const pid_t pid, std::shared_ptr<DfxMap> map);

private:
    MAYBE_UNUSED uintptr_t arkSymbolExtractorPtr_ = 0;
    MAYBE_UNUSED std::unique_ptr<uint8_t[]> abcDataPtr_ = nullptr;
    MAYBE_UNUSED size_t abcDataSize_ = 0;
    MAYBE_UNUSED uintptr_t abcLoadOffset_ = 0;
    MAYBE_UNUSED std::unique_ptr<uint8_t[]> srcMapDataPtr_ = nullptr;
    MAYBE_UNUSED size_t srcMapDataSize_ = 0;
    MAYBE_UNUSED uintptr_t srcMapLoadOffset_ = 0;
    MAYBE_UNUSED std::unique_ptr<DfxExtractor> extractor_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif