/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#ifndef CPPCRASH_FORMATTER_H
#define CPPCRASH_FORMATTER_H

#include <string>
#include <cstdint>
#include "cppcrash_info_collector.h"

#ifndef is_ohos_lite
#include "cJSON.h"
#endif

namespace OHOS {
namespace HiviewDFX {

class ICrashFormatter {
public:
    virtual std::string FormatCrashInfo() = 0;
    virtual bool NeedFormatCrashInfo() = 0;
};

#ifndef is_ohos_lite
class CppCrashJsonFormatter : public ICrashFormatter {
public:
    std::string FormatCrashInfo() override;
    bool NeedFormatCrashInfo() override;

private:
    void AddHeadInfo(cJSON* root, CppCrashInfoCollector& collector);
    void AddSignalInfo(cJSON* root, CppCrashInfoCollector& collector);
    void AddThreadInfo(cJSON* root, CppCrashInfoCollector& collector);
    void AddSubmitterStacktrace(cJSON* root, CppCrashInfoCollector& collector);
    void AddRegistersInfo(cJSON* root, CppCrashInfoCollector& collector);
    void AddExtraCrashInfo(cJSON* root, CppCrashInfoCollector& collector);
    void AddMemoryNearRegister(cJSON* root, CppCrashInfoCollector& collector);
    void AddFaultStack(cJSON* root, CppCrashInfoCollector& collector);
    void AddMapsInfo(cJSON* root, CppCrashInfoCollector& collector);
    void AddOpenFiles(cJSON* root, CppCrashInfoCollector& collector);
    bool AddStringValue(cJSON* root, const char* key, const std::string& value, bool isRequire = false);
    bool AddBoolValue(cJSON* root, const char* key, bool value);
    cJSON* FillThreadJson(const ThreadInfo& threadInfo);
    cJSON* FillFramesJson(const std::vector<DfxFrame>& frames);
    cJSON* FillNativeFrameJson(const DfxFrame& frame);
    cJSON* FillJsFrameJson(const DfxFrame& frame);
    std::string FormatPc(uint64_t relPc);
    static bool IsLastValidFrame(const DfxFrame& frame);
};
#else
class CppCrashTextFormatter : public ICrashFormatter {
public:
    std::string FormatCrashInfo() override;
    bool NeedFormatCrashInfo() override;
};
#endif

class CppCrashFormatterFactory {
public:
    static ICrashFormatter& Create();

private:
    CppCrashFormatterFactory() = delete;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // CPPCRASH_FORMATTER_H