/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

class DfxStackInfoJsonFormatter {
public:
    bool GetJsonFormatInfo(bool isDump, std::string& jsonStringInfo, const ProcessDumpRequest& request,
        DfxProcess& process) const;

private:
#ifndef is_ohos_lite
    void GetCrashJsonFormatInfo(Json::Value& jsonInfo, const ProcessDumpRequest& request, DfxProcess& process) const;
    void GetDumpJsonFormatInfo(Json::Value& jsonInfo, DfxProcess& process) const;
    static bool FillFrames(DfxThread& thread, Json::Value& jsonInfo);
    static void FillNativeFrame(const DfxFrame& frame, Json::Value& jsonInfo);
    void AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads, Json::Value& jsonInfo) const;
#endif
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
