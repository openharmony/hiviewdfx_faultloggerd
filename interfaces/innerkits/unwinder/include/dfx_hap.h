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
#include <sys/types.h>
#include "dfx_extractor_utils.h"

namespace OHOS {
namespace HiviewDFX {
class DfxMap;

class DfxHap {
public:
    DfxHap() = default;
    ~DfxHap() = default;

    bool ParseHapFileData(const std::string& name);
    bool ParseHapMemData(const pid_t pid, std::shared_ptr<DfxMap> map);

public:
    std::unique_ptr<uint8_t[]> abcDataPtr_ = nullptr;
    size_t abcDataSize_ = 0;
    uintptr_t abcLoadOffset_ = 0;
    std::unique_ptr<uint8_t[]> srcMapDataPtr_ = nullptr;
    size_t srcMapDataSize_ = 0;
    uintptr_t srcMapLoadOffset_ = 0;

private:
    std::unique_ptr<DfxExtractor> extractor_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif