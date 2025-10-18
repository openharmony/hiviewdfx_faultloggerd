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
#include "dfx_hap.h"
#include "dfx_log.h"
#include "dfx_symbols.h"
#include "elf_factory.h"
#include "string_util.h"
#include "unwinder_config.h"

#ifndef HISYSEVENT_DISABLE
#include "file_util.h"
#include "hisysevent.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxOfflineParser"

const char* const SANDBOX_PATH_PREFIX = "/data/storage/el1/bundle/";
const char* const ARKWEBCORE_SANDBOX_PATH_PREFIX = "/data/storage/el1/bundle/arkwebcore/";
const char* const BUNDLE_PATH_PREFIX = "/data/app/el1/bundle/public/";
MAYBE_UNUSED const char* const SELF_CMDLINE_PATH = "/proc/self/cmdline";
const int MAX_SINGLE_FRAME_PARSE_TIME = 1000;
}
DfxOfflineParser::DfxOfflineParser(const std::string& bundleName) : bundleName_(bundleName)
{
    CachedEnableMiniDebugInfo_ = UnwinderConfig::GetEnableMiniDebugInfo();
    CachedEnableLoadSymbolLazily_ = UnwinderConfig::GetEnableLoadSymbolLazily();
    UnwinderConfig::SetEnableMiniDebugInfo(false);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
    dfxMaps_ = std::make_shared<DfxMaps>();
}

DfxOfflineParser::~DfxOfflineParser()
{
    UnwinderConfig::SetEnableMiniDebugInfo(CachedEnableMiniDebugInfo_);
    UnwinderConfig::SetEnableLoadSymbolLazily(CachedEnableLoadSymbolLazily_);
}

bool DfxOfflineParser::ParseSymbolWithFrame(DfxFrame& frame)
{
    counter_.Reset();
    bool result = IsJsFrame(frame) ? ParseJsSymbol(frame) : ParseNativeSymbol(frame);
    auto costTime = counter_.Elapsed<std::chrono::milliseconds>();
    if (costTime > MAX_SINGLE_FRAME_PARSE_TIME) {
        ReportDumpStats(frame, static_cast<uint32_t>(costTime));
    }
    return result;
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

std::shared_ptr<DfxMap> DfxOfflineParser::GetMapForFrame(const DfxFrame& frame)
{
    if (dfxMaps_ == nullptr) {
        return nullptr;
    }
    auto maps = dfxMaps_->GetMaps();
    auto it = std::find_if(maps.begin(), maps.end(), [&](const std::shared_ptr<DfxMap>& map) {
        return map->name == frame.mapName;
    });
    if (it != maps.end() && *it) {
        return *it;
    }
    return nullptr;
}

bool DfxOfflineParser::ParseJsSymbol(DfxFrame& frame)
{
    if (dfxMaps_ == nullptr) {
        return false;
    }
    auto dfxMap = GetMapForFrame(frame);
    auto dfxHap = dfxMap ? dfxMap->hap : std::make_shared<DfxHap>();
    if (!dfxHap) {
        return false;
    }
    if (!dfxMap) {
        dfxMap = std::make_shared<DfxMap>();
        dfxMap->name = GetBundlePath(frame.mapName);
        dfxMap->hap = dfxHap;
        dfxMaps_->AddMap(dfxMap);
    }
    JsFunction jsFunction;
    bool isSuccess = dfxHap->ParseHapInfo(0, frame.relPc, dfxMap, &jsFunction, true);
    if (!isSuccess) {
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
    if (bundleName_.empty() || !StartsWith(originPath, SANDBOX_PATH_PREFIX) ||
        StartsWith(originPath, ARKWEBCORE_SANDBOX_PATH_PREFIX)) {
        return originPath;
    }
    return BUNDLE_PATH_PREFIX + bundleName_ + "/" + originPath.substr(std::strlen(SANDBOX_PATH_PREFIX));
}

std::shared_ptr<DfxElf> DfxOfflineParser::GetElfForFrame(const DfxFrame& frame)
{
    if (dfxMaps_ == nullptr) {
        return nullptr;
    }
    auto dfxMap = GetMapForFrame(frame);
    if (dfxMap && dfxMap->elf) {
        return dfxMap->elf->IsValid() ? dfxMap->elf : nullptr;
    }
    RegularElfFactory factory(GetBundlePath(frame.mapName));
    auto elf = factory.Create();
    if (elf == nullptr) {
        DFXLOGE("elf is nullptr");
        return nullptr;
    }
    dfxMap = std::make_shared<DfxMap>();
    dfxMap->name = frame.mapName;
    dfxMap->elf = elf;
    dfxMaps_->AddMap(dfxMap);
    if (!elf->IsValid()) {
        DFXLOGE("elf is invalid");
        return nullptr;
    }
    return elf;
}

void DfxOfflineParser::ReportDumpStats(const ReportData& reportData)
{
#ifndef HISYSEVENT_DISABLE
    std::string cmdline;
    if (!LoadStringFromFile(SELF_CMDLINE_PATH, cmdline)) {
        DFXLOGE("Failed to read self cmdline, errno:%{public}d", errno);
    }
    Trim(cmdline);
    HiSysEventWrite(
        HiSysEvent::Domain::HIVIEWDFX,
        "DUMP_CATCHER_STATS",
        HiSysEvent::EventType::STATISTIC,
        "CALLER_PROCESS_NAME", cmdline.c_str(),
        "TARGET_PROCESS_NAME", reportData.bundleName.c_str(),
        "RESULT", reportData.parseCostType,
        "SUMMARY", reportData.summary.c_str(),
        "WRITE_DUMP_INFO_TIME", reportData.costTime,
        "TARGET_PROCESS_THREAD_COUNT", reportData.threadCount);
#endif
}

void DfxOfflineParser::ReportDumpStats(const DfxFrame& frame, uint32_t costTime)
{
    ReportData reportData;
    reportData.parseCostType = ParseCostType::PARSE_SINGLE_FRAME_TIME;
    reportData.bundleName = bundleName_;
    reportData.costTime = costTime;
    std::string summary = "";
    if (frame.isJsFrame) {
        summary += StringPrintf("at %s, %s, %s", frame.funcName.c_str(), frame.packageName.c_str(),
            frame.mapName.c_str());
    } else {
        summary += StringPrintf("relPc: %016" PRIx64 ", mapName:%s, buildId:%s", frame.relPc, frame.mapName.c_str(),
            frame.buildId.c_str());
    }
    reportData.summary = summary;
    ReportDumpStats(reportData);
}
} // namespace HiviewDFX
} // namespace OHOS