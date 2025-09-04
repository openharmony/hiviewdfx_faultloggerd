/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "dfx_offline_parser.h"

#include <fcntl.h>
#include "dfx_ark.h"
#include "dfx_log.h"
#include "dfx_symbols.h"
#include "elf_factory.h"
#include "string_util.h"
#include "unwinder_config.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxOfflineParser"

const char* const SANDBOX_PATH_PREFIX = "/data/storage/el1/bundle/";
const char* const BUNDLE_PATH_PREFIX = "/data/app/el1/bundle/public/";
}
DfxOfflineParser::DfxOfflineParser(const std::string& bundleName) : bundleName_(bundleName)
{
    CachedEnableMiniDebugInfo_ = UnwinderConfig::GetEnableMiniDebugInfo();
    CachedEnableLoadSymbolLazily_ = UnwinderConfig::GetEnableLoadSymbolLazily();
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
    dfxMaps_ = std::make_shared<DfxMaps>();
}

DfxOfflineParser::~DfxOfflineParser()
{
    UnwinderConfig::SetEnableMiniDebugInfo(CachedEnableMiniDebugInfo_);
    UnwinderConfig::SetEnableLoadSymbolLazily(CachedEnableLoadSymbolLazily_);
    if (arkSymbolExtractorPtr_ != 0) {
        DfxArk::Instance().ArkDestoryJsSymbolExtractor(arkSymbolExtractorPtr_);
        arkSymbolExtractorPtr_ = 0;
    }
}

bool DfxOfflineParser::ParseSymbolWithFrame(DfxFrame& frame)
{
    return IsJsFrame(frame) ? ParseJsSymbol(frame) : ParseNativeSymbol(frame);
}

bool DfxOfflineParser::IsJsFrame(const DfxFrame& frame)
{
    return DfxMaps::IsArkHapMapItem(frame.mapName) || DfxMaps::IsArkCodeMapItem(frame.mapName);
}

bool DfxOfflineParser::ParseNativeSymbol(DfxFrame& frame)
{
    auto elf = GetElfForFrame(frame);
    if (elf == nullptr) {
        return false;
    }
    frame.buildId = elf->GetBuildId();
    if (!DfxSymbols::GetFuncNameAndOffsetByPc(frame.relPc, elf, frame.funcName, frame.funcOffset)) {
        DFXLOGU("Failed to get symbol, relPc: %{public}" PRIx64 ", mapName: %{public}s",
            frame.relPc, frame.mapName.c_str());
        return false;
    }
    frame.parseSymbolState.SetParseSymbolState(true);
    return true;
}

bool DfxOfflineParser::ParseJsSymbolForArkHap(const DfxFrame& frame, JsFunction& jsFunction)
{
    std::string realPath = GetBundlePath(frame.mapName);
    bool success = DfxArk::Instance().ParseArkFileInfo(
        static_cast<uintptr_t>(frame.relPc), 0,
        realPath.c_str(), arkSymbolExtractorPtr_, &jsFunction) >= 0;
    if (!success) {
        DFXLOGW("Failed to parse ark file info, relPc: %{private}p, hapName: %{private}s",
            reinterpret_cast<void *>(frame.relPc), realPath.c_str());
    }
    return success;
}

bool DfxOfflineParser::ParseJsSymbolForArkCode(const DfxFrame& frame, JsFunction& jsFunction)
{
    std::string realPath = GetBundlePath(frame.mapName);
    SmartFd smartFd(open(realPath.c_str(), O_RDONLY));
    if (!smartFd) {
        DFXLOGE("Failed to open file: %{public}s, errno(%{public}d)", realPath.c_str(), errno);
        return false;
    }
    off_t size = lseek(smartFd.GetFd(), 0, SEEK_END);
    if (size <= 0) {
        DFXLOGE("fd is empty or error, fd(%{public}d)", smartFd.GetFd());
        return false;
    }
    void* mptr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, smartFd.GetFd(), 0);
    if (mptr == MAP_FAILED) {
        DFXLOGE("mmap failed, fd(%{public}d), errno(%{public}d)", smartFd.GetFd(), errno);
        return false;
    }
    bool success = DfxArk::Instance().ParseArkFrameInfo(
        static_cast<uintptr_t>(frame.relPc), 0, 0,
        static_cast<uint8_t*>(mptr),
        size, arkSymbolExtractorPtr_, &jsFunction) >= 0;
    if (!success) {
        DFXLOGW("Failed to parse ark frame info, relPc: %{private}p, codeName: %{private}s",
            reinterpret_cast<void *>(frame.relPc), realPath.c_str());
    }
    munmap(mptr, size);
    return success;
}

bool DfxOfflineParser::ParseJsSymbol(DfxFrame& frame)
{
    if (arkSymbolExtractorPtr_ == 0) {
        if (DfxArk::Instance().ArkCreateJsSymbolExtractor(&arkSymbolExtractorPtr_) < 0) {
            DFXLOGE("Failed to create ark js symbol extractor");
            return false;
        }
    }
    JsFunction jsFunction;
    bool success = DfxMaps::IsArkHapMapItem(frame.mapName)
                        ? ParseJsSymbolForArkHap(frame, jsFunction)
                        : ParseJsSymbolForArkCode(frame, jsFunction);
    if (!success) {
        return false;
    }
    frame.isJsFrame = true;
    frame.mapName = std::string(jsFunction.url);
    frame.funcName = std::string(jsFunction.functionName);
    frame.packageName = std::string(jsFunction.packageName);
    frame.line = static_cast<int32_t>(jsFunction.line);
    frame.column = jsFunction.column;
    return true;
}

std::string DfxOfflineParser::GetBundlePath(const std::string& originPath) const
{
    if (!StartsWith(originPath, SANDBOX_PATH_PREFIX) || bundleName_.empty()) {
        return originPath;
    }
    return BUNDLE_PATH_PREFIX + bundleName_ + "/" + originPath.substr(std::strlen(SANDBOX_PATH_PREFIX));
}

std::shared_ptr<DfxElf> DfxOfflineParser::GetElfForFrame(const DfxFrame& frame)
{
    if (dfxMaps_ == nullptr) {
        return nullptr;
    }
    auto maps = dfxMaps_->GetMaps();
    auto it = std::find_if(maps.begin(), maps.end(), [&](const std::shared_ptr<DfxMap>& map) {
        return map->name == frame.mapName;
    });
    if (it != maps.end() && *it) {
        return (*it)->elf;
    }
    RegularElfFactory factory(GetBundlePath(frame.mapName));
    auto elf = factory.Create();
    if (elf == nullptr) {
        DFXLOGE("elf is nullptr");
        return nullptr;
    }
    if (!elf->IsValid()) {
        DFXLOGE("elf is invalid");
        return nullptr;
    }
    auto newMap = std::make_shared<DfxMap>();
    newMap->name = frame.mapName;
    newMap->elf = elf;
    dfxMaps_->AddMap(newMap);
    return elf;
}
} // namespace HiviewDFX
} // namespace OHOS