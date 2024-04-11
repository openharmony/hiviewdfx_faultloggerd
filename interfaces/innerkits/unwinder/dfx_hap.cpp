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

#include "dfx_hap.h"

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_map.h"
#include "dfx_memory.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxHap"
}

bool DfxHap::ParseHapFileData(const std::string& name)
{
#ifndef is_ohos_lite
    if (name.empty()) {
        return false;
    }
    if (abcDataPtr_ != nullptr) {
        return true;
    }
    LOGU("name: %s", name.c_str());
    if (extractor_ == nullptr) {
        extractor_ = std::make_unique<DfxExtractor>(name);
    }
    if (!extractor_->GetHapAbcInfo(abcLoadOffset_, abcDataPtr_, abcDataSize_)) {
        LOGW("Failed to get hap abc info: %s", name.c_str());
        abcDataPtr_ = nullptr;
        return false;
    }

    if (extractor_->GetHapSourceMapInfo(srcMapLoadOffset_, srcMapDataPtr_, srcMapDataSize_)) {
        LOGU("Failed to get hap source map info: %s", name.c_str());
    }
    return true;
#endif
}

bool DfxHap::ParseHapMemData(const pid_t pid, std::shared_ptr<DfxMap> map)
{
#if is_ohos && !is_mingw
#ifndef is_ohos_lite
    if (pid < 0 || map == nullptr) {
        return false;
    }
    if (abcDataPtr_ != nullptr) {
        return true;
    }
    LOGU("pid: %d", pid);
    abcLoadOffset_ = map->offset;
    abcDataSize_ = map->end - map->begin;
    abcDataPtr_ = std::make_unique<uint8_t[]>(abcDataSize_);
    auto size = DfxMemory::ReadProcMemByPid(pid, map->begin, abcDataPtr_.get(), abcDataSize_);
    if (size != abcDataSize_) {
        LOGW("ReadProcMemByPid(%d) return size(%zu), real size(%zu)", pid, size, abcDataSize_);
        abcDataPtr_ = nullptr;
        return false;
    }
    return true;
#endif
#endif
    return false;
}
} // namespace HiviewDFX
} // namespace OHOS
