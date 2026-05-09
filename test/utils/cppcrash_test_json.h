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

#ifndef CPPCRASH_TEST_JSON_H
#define CPPCRASH_TEST_JSON_H

#include <string>
#include "cJSON.h"
#include "string_printf.h"

namespace OHOS {
namespace HiviewDFX {

class CppCrashTestJson {
public:
    CppCrashTestJson();
    ~CppCrashTestJson();

    void InitFromFile(const std::string& filePath);
    std::string ToTextString();
    void Clear();

private:
    std::string AppendHeaderFields();
    std::string AppendConfigInfo();
    std::string AppendProcessFields();
    std::string AppendStackTraceFields();
    std::string AppendOtherThreadInfo();
    std::string AppendMemoryFields();
    std::string AppendKeyThreadInfo();

    std::string ThreadJsonToText(cJSON* threadJson);
    std::string FramesJsonToText(cJSON* framesJson);
    std::string FrameJsonToText(cJSON* frameJson, int index);
    std::string JsFrameJsonToText(cJSON* frameJson, int index);
    std::string NativeFrameJsonToText(cJSON* frameJson, int index);

    cJSON* root_;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // CPPCRASH_TEST_JSON_H