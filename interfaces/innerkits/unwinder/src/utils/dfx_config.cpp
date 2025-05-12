/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include "dfx_config.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <securec.h>
#include <string>
#include "dfx_define.h"
#include "dfx_log.h"
#include "string_util.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxConfig"

const char FAULTLOGGER_CONF_PATH[] = "/system/etc/faultlogger.conf";
const int CONF_LINE_SIZE = 256;
}

DfxConfigInfo& DfxConfig::GetConfig()
{
    static DfxConfigInfo config;
    static std::once_flag flag;
    std::call_once(flag, [&] {
        ReadAndParseConfig(config);
    });
    return config;
}

void DfxConfig::InitConfigMaps(DfxConfigInfo& config, std::map<std::string, bool *>& boolConfig,
    std::map<std::string, unsigned int *>& uintConfig)
{
    boolConfig = {
        {std::string("displayRegister"), &(config.displayRegister)},
        {std::string("displayBacktrace"), &(config.displayBacktrace)},
        {std::string("displayMaps"), &(config.displayMaps)},
        {std::string("displayFaultStack.switch"), &(config.displayFaultStack)},
        {std::string("dumpOtherThreads"), &(config.dumpOtherThreads)},
    };
    uintConfig = {
        {std::string("displayFaultStack.lowAddressStep"), &(config.lowAddressStep)},
        {std::string("displayFaultStack.highAddressStep"), &(config.highAddressStep)},
        {std::string("maxFrameNums"), &(config.maxFrameNums)},
        {std::string("reservedParseSymbolTime"), &(config.reservedParseSymbolTime)},
    };
}

void DfxConfig::ReadAndParseConfig(DfxConfigInfo& config)
{
    char codeBuffer[CONF_LINE_SIZE] = {0};
    FILE *fp = fopen(FAULTLOGGER_CONF_PATH, "r");
    if (fp == nullptr) {
        DFXLOGW("Failed to open %{public}s. Reason: %{public}s.", FAULTLOGGER_CONF_PATH, strerror(errno));
        return;
    }
    std::map<std::string, bool *> boolConfig;
    std::map<std::string, unsigned int *> uintConfig;
    InitConfigMaps(config, boolConfig, uintConfig);

    auto boolConfigParser = [](bool* configProp, const std::string& value) {
        *configProp = (value != "false");
    };
    auto uintConfigParser = [](unsigned int* configProp, const std::string& value) {
        unsigned int propValue = static_cast<unsigned int>(atoi(value.data()));
        if (propValue != 0) {
            *configProp = propValue;
        }
    };

    while (!feof(fp)) {
        (void)memset_s(codeBuffer, sizeof(codeBuffer), '\0', sizeof(codeBuffer));
        if (fgets(codeBuffer, CONF_LINE_SIZE - 1, fp) == nullptr) {
            continue;
        }
        std::string line(codeBuffer);
        std::string::size_type newLinePos = line.find_first_of("\n");
        if (newLinePos != line.npos) {
            line.resize(newLinePos);
        }
        std::string::size_type equalSignPos = line.find_first_of("=");
        if (equalSignPos != line.npos) {
            std::string key = line.substr(0, equalSignPos);
            std::string value = line.substr(equalSignPos + 1);
            Trim(key);
            Trim(value);
            if (boolConfig.find(key) != boolConfig.end()) {
                boolConfigParser(boolConfig[key], value);
            } else if (uintConfig.find(key) != uintConfig.end()) {
                uintConfigParser(uintConfig[key], value);
            }
        }
    }
    (void)fclose(fp);
}
} // namespace HiviewDFX
} // namespace OHOS
