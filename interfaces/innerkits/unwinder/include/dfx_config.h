/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#ifndef DFX_CONFIG_H
#define DFX_CONFIG_H

#include <map>
#include <string>

namespace OHOS {
namespace HiviewDFX {
struct DfxConfigInfo {
    bool displayBacktrace = true;
    bool displayRegister = true;
    bool displayMaps = true;
    bool displayFaultStack = true;
    bool dumpOtherThreads = true;
    unsigned int lowAddressStep = 16;
    unsigned int highAddressStep = 512;
    unsigned int maxFrameNums = 256;
    unsigned int reservedParseSymbolTime = 100;
};

class DfxConfig {
public:
    static DfxConfigInfo& GetConfig();
private:
    static void ReadAndParseConfig(DfxConfigInfo& config);
    static void InitConfigMaps(DfxConfigInfo& config, std::map<std::string, bool *>& boolConfig,
        std::map<std::string, unsigned int *>& uintConfig);
private:
    DfxConfig() = default;
    ~DfxConfig() = default;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
