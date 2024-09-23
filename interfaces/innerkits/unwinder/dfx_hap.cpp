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
#include "dfx_maps.h"
#include "dfx_memory.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxHap"
}

DfxHap::~DfxHap()
{
#if is_ohos && !is_mingw
    if (arkSymbolExtractorPtr_ != 0) {
        DfxArk::ArkDestoryJsSymbolExtractor(arkSymbolExtractorPtr_);
        arkSymbolExtractorPtr_ = 0;
    }
#endif
}

bool DfxHap::ParseHapInfo(pid_t pid, uint64_t pc, uintptr_t methodid, std::shared_ptr<DfxMap> map,
    JsFunction *jsFunction)
{
#if is_ohos && !is_mingw
    if (jsFunction == nullptr || map == nullptr) {
        return false;
    }

    if (arkSymbolExtractorPtr_ == 0) {
        if (DfxArk::ArkCreateJsSymbolExtractor(&arkSymbolExtractorPtr_) < 0) {
            LOGUNWIND("Failed to create ark js symbol extractor");
        }
    }

    if (DfxMaps::IsArkHapMapItem(map->name)) {
        if (!ParseHapFileInfo(pc, methodid, map, jsFunction)) {
            LOGWARN("Failed to parse hap file info");
            return false;
        }
    } else {
        if (!ParseHapMemInfo(pid, pc, methodid, map, jsFunction)) {
            LOGWARN("Failed to parse hap mem info");
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

bool DfxHap::ParseHapFileInfo(uint64_t pc, uintptr_t methodid, std::shared_ptr<DfxMap> map, JsFunction *jsFunction)
{
#if is_ohos && !is_mingw
    if (jsFunction == nullptr || map == nullptr || map->name.empty()) {
        return false;
    }

    if (DfxArk::ParseArkFileInfo(static_cast<uintptr_t>(pc), methodid, static_cast<uintptr_t>(map->begin),
        map->name.c_str(), arkSymbolExtractorPtr_, jsFunction) < 0) {
        LOGWARN("Failed to parse ark file info, pc: %{public}p, begin: %{public}p",
            reinterpret_cast<void *>(pc), reinterpret_cast<void *>(map->begin));
        return false;
    }
    return true;
#endif
    return false;
}

bool DfxHap::ParseHapMemInfo(pid_t pid, uint64_t pc, uintptr_t methodid, std::shared_ptr<DfxMap> map,
    JsFunction *jsFunction)
{
#if is_ohos && !is_mingw
    if (pid < 0 || jsFunction == nullptr || map == nullptr) {
        return false;
    }

    if (!ParseHapMemData(pid, map)) {
        LOGWARN("Failed to parse hap mem data, pid: %{public}d", pid);
        return false;
    }
    if (DfxArk::ParseArkFrameInfo(static_cast<uintptr_t>(pc), methodid, static_cast<uintptr_t>(map->begin),
        abcLoadOffset_, abcDataPtr_.get(), abcDataSize_, arkSymbolExtractorPtr_, jsFunction) < 0) {
        LOGWARN("Failed to parse ark frame info, pc: %{public}p, begin: %{public}p",
            reinterpret_cast<void *>(pc), reinterpret_cast<void *>(map->begin));
        return false;
    }
    return true;
#endif
    return false;
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
    LOGUNWIND("name: %{public}s", name.c_str());
    if (extractor_ == nullptr) {
        extractor_ = std::make_unique<DfxExtractor>(name);
    }
    if (!extractor_->GetHapAbcInfo(abcLoadOffset_, abcDataPtr_, abcDataSize_)) {
        LOGWARN("Failed to get hap abc info: %{public}s", name.c_str());
        abcDataPtr_ = nullptr;
        return false;
    }

    if (!extractor_->GetHapSourceMapInfo(srcMapLoadOffset_, srcMapDataPtr_, srcMapDataSize_)) {
        LOGUNWIND("Failed to get hap source map info: %{public}s", name.c_str());
    }
    return true;
#endif
    return false;
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
    LOGUNWIND("pid: %{public}d", pid);
    abcLoadOffset_ = map->offset;
    abcDataSize_ = map->end - map->begin;
    abcDataPtr_ = std::make_unique<uint8_t[]>(abcDataSize_);
    auto size = DfxMemory::ReadProcMemByPid(pid, map->begin, abcDataPtr_.get(), abcDataSize_);
    if (size != abcDataSize_) {
        LOGWARN("ReadProcMemByPid(%{public}d) return size(%{public}zu), real size(%{public}zu)",
            pid, size, abcDataSize_);
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
