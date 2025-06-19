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
#include "cJSON.h"
#endif

namespace OHOS {
namespace HiviewDFX {

class DumpInfoJsonFormatter {
public:
    bool GetJsonFormatInfo(const ProcessDumpRequest& request, DfxProcess& process,
        std::string& jsonStringInfo);

private:
#ifndef is_ohos_lite
    bool AddItemToJsonInfo(const ProcessDumpRequest& request, DfxProcess& process, cJSON *jsonInfo);
    void GetCrashJsonFormatInfo(const ProcessDumpRequest& request, DfxProcess& process, cJSON *jsonInfo);
    void GetDumpJsonFormatInfo(DfxProcess& process, cJSON *jsonInfo);
    void AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads, cJSON *jsonInfo) const;
    bool FillFramesJson(const std::vector<DfxFrame>& frames, cJSON *jsonInfo) const;
    void FillJsFrameJson(const DfxFrame& frame, cJSON *jsonInfo) const;
    void FillNativeFrameJson(const DfxFrame& frame, cJSON *jsonInfo) const;
#endif
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
