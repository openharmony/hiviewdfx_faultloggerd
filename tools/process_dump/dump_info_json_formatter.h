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

#ifndef DFX_STACK_INFO_FORMATTER_H
#define DFX_STACK_INFO_FORMATTER_H

#include <memory>
#include <string>

#include "dfx_dump_request.h"
#include "dfx_process.h"
#ifndef is_ohos_lite
#include "json/json.h"
#endif

namespace OHOS {
namespace HiviewDFX {

class DumpInfoJsonFormatter {
public:
    static bool GetJsonFormatInfo(const ProcessDumpRequest& request, DfxProcess& process,
        std::string& jsonStringInfo, int dumpError);

private:
#ifndef is_ohos_lite
    static void GetCrashJsonFormatInfo(const ProcessDumpRequest& request, DfxProcess& process, Json::Value& jsonInfo);
    static void GetDumpJsonFormatInfo(DfxProcess& process, Json::Value& jsonInfo);
    static void AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads, Json::Value& jsonInfo);
    static bool FillFramesJson(const std::vector<DfxFrame>& frames, Json::Value& jsonInfo);
    static void FillJsFrameJson(const DfxFrame& frame, Json::Value& jsonInfo);
    static void FillNativeFrameJson(const DfxFrame& frame, Json::Value& jsonInfo);
    static void FillThreadstatInfo(Json::Value& jsonInfo, const std::shared_ptr<ProcessInfo> info);
#endif
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
