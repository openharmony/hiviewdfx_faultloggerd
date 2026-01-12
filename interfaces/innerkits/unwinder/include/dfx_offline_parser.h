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
#ifndef DFX_OFFLINE_PARSER_H
#define DFX_OFFLINE_PARSER_H

#include "dfx_ark.h"
#include "dfx_elf.h"
#include "dfx_maps.h"
#include "dfx_frame.h"
#include "elapsed_time.h"

namespace OHOS {
namespace HiviewDFX {
enum ParseCostType : int32_t {
    PARSE_ALL_FRAME_TIME = 100,
    PARSE_SINGLE_FRAME_TIME = 101,
    PARSE_ALL_BUILDID_TIME = 102,
    PARSE_SINGLE_JS_SYMBOL_TIME = 103,
    PARSE_SINGLE_SO_BUILDID_TIME = 104,
    PARSE_SINGLE_SO_SYMBOL_TIME = 105,
};

struct ParseTime {
    uint32_t formatKernelStackTime {0};
    uint32_t parseBuildIdJsSymbolTime {0};
    uint32_t parseNativeSymbolTime {0};
};

struct ReportData {
    ParseCostType parseCostType {PARSE_ALL_FRAME_TIME};
    std::string bundleName {""};
    std::string summary {""};
    uint32_t costTime {0};
    uint32_t threadCount {0};
    uint32_t pssMemory {0};
    ParseTime parseTime;
};
class DfxOfflineParser {
public:
    DfxOfflineParser();
    DfxOfflineParser(const std::string& bundleName, bool onlyParseBuildId = false);
    ~DfxOfflineParser();
    DfxOfflineParser(const DfxOfflineParser&) = delete;
    DfxOfflineParser& operator= (const DfxOfflineParser&) = delete;
    bool ParseSymbolWithFrame(DfxFrame& frame);
    bool ParseBuildIdJsSymbolWithFrames(std::vector<DfxFrame>& frames);
    bool ParseNativeSymbolWithFrames(std::vector<DfxFrame>& frames);
    static bool ReportDumpStats(const ReportData& reportData);
private:
    static bool IsJsFrame(const DfxFrame& frame);
    bool ParseBuildIdAndNativeSymbol(DfxFrame& frame);
    bool ParseJsSymbol(DfxFrame& frame);
    bool ParseBuildId(DfxFrame& frame);
    bool ParseNativeSymbol(DfxFrame& frame);
    bool ParseBuildIdJsSymbol(DfxFrame& frame);
    std::string GetBundlePath(const std::string& originPath) const;
    std::shared_ptr<DfxElf> GetElfForFrame(const DfxFrame& frame);
    std::shared_ptr<DfxMap> GetMapForFrame(const DfxFrame& frame);
    bool ReportDumpStats(const DfxFrame& frame, uint32_t costTime, ParseCostType type);
    bool onlyParseBuildId_ {false};
    bool cachedEnableMiniDebugInfo_ {false};
    bool cachedEnableLoadSymbolLazily_ {false};
    std::string bundleName_ {""};
    std::shared_ptr<DfxMaps> dfxMaps_ {nullptr};
    ElapsedTime counter_;
    uint32_t uid_ {0};
};
}
}
#endif