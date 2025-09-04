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

namespace OHOS {
namespace HiviewDFX {
class DfxOfflineParser {
public:
    DfxOfflineParser(const std::string& bundleName);
    ~DfxOfflineParser();
    DfxOfflineParser(const DfxOfflineParser&) = delete;
    DfxOfflineParser& operator= (const DfxOfflineParser&) = delete;
    bool ParseSymbolWithFrame(DfxFrame& frame);
private:
    static bool IsJsFrame(const DfxFrame& frame);
    bool ParseNativeSymbol(DfxFrame& frame);
    bool ParseJsSymbol(DfxFrame& frame);
    bool ParseJsSymbolForArkHap(const DfxFrame& frame, JsFunction& jsFunction);
    bool ParseJsSymbolForArkCode(const DfxFrame& frame, JsFunction& jsFunction);
    std::string GetBundlePath(const std::string& originPath) const;
    std::shared_ptr<DfxElf> GetElfForFrame(const DfxFrame& frame);
    bool CachedEnableMiniDebugInfo_ {false};
    bool CachedEnableLoadSymbolLazily_ {false};
    uintptr_t arkSymbolExtractorPtr_ {0};
    std::string bundleName_ {""};
    std::shared_ptr<DfxMaps> dfxMaps_ {nullptr};
};
}
}
#endif