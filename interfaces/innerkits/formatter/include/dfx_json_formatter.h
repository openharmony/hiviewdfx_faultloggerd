/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#ifndef DFX_JSON_FORMATTER_H
#define DFX_JSON_FORMATTER_H

#include <memory>
#include <string>

namespace OHOS {
namespace HiviewDFX {
class DfxJsonFormatter {
public:
    DfxJsonFormatter() = default;
    ~DfxJsonFormatter() = default;

#ifndef is_ohos_lite
    /**
     * @brief Format Json string to stack string
     *
     * @param jsonStack input the json string
     * @param outStackStr output the stack string
     * @return bool
     */
    static bool FormatJsonStack(const std::string& jsonStack, std::string& outStackStr);

    /**
     * @brief Format kernel stack
     *
     * @param kernelStack kernel stack
     * @param formattedStack formatted stack, eg #00 pc xxxx xxxxx.so
     * @param jsonFormat whether return json format stack, default false
     * @return if succeed return true, otherwise return false
     */
    static bool FormatKernelStack(const std::string& kernelStack, std::string& formattedStack, bool jsonFormat = false,
        bool needParseSymbol = false, const std::string& bundleName = "");
#endif
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
