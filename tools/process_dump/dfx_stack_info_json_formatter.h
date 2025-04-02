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

#include <cinttypes>
#include <csignal>
#include <map>
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
    DfxStackInfoJsonFormatter(std::shared_ptr<DfxProcess> process, std::shared_ptr<ProcessDumpRequest> request)
        : process_(process), request_(request) {};
    virtual ~DfxStackInfoJsonFormatter() {};
    bool GetJsonFormatInfo(bool isDump, std::string& jsonStringInfo) const;

private:
    DfxStackInfoJsonFormatter() = delete;
#ifndef is_ohos_lite
    void GetCrashJsonFormatInfo(Json::Value& jsonInfo) const;
    void GetDumpJsonFormatInfo(Json::Value& jsonInfo) const;
    bool FillFrames(const std::shared_ptr<DfxThread>& thread, Json::Value& jsonInfo) const;
    void FillNativeFrame(const DfxFrame& frame, Json::Value& jsonInfo) const;
    void AppendThreads(const std::vector<std::shared_ptr<DfxThread>>& threads, Json::Value& jsonInfo) const;
#endif

private:
    std::shared_ptr<DfxProcess> process_ = nullptr;
    std::shared_ptr<ProcessDumpRequest> request_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
