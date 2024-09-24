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
        ReadConfig(config);
    });
    return config;
}

void DfxConfig::ParserConfig(DfxConfigInfo& config, const std::string& key, const std::string& value)
{
    do {
        if (key.compare("displayRigister") == 0) {
            if (value.compare("false") == 0) {
                config.displayRegister = false;
            } else {
                config.displayRegister = true;
            }
            break;
        }
        if (key.compare("displayBacktrace") == 0) {
            if (value.compare("false") == 0) {
                config.displayBacktrace = false;
            } else {
                config.displayBacktrace = true;
            }
            break;
        }
        if (key.compare("displayMaps") == 0) {
            if (value.compare("false") == 0) {
                config.displayMaps = false;
            } else {
                config.displayMaps = true;
            }
            break;
        }
        if (key.compare("displayFaultStack.switch") == 0) {
            if (value.compare("false") == 0) {
                config.displayFaultStack = false;
            } else {
                config.displayFaultStack = true;
            }
            break;
        }
        if (key.compare("dumpOtherThreads") == 0) {
            if (value.compare("false") == 0) {
                config.dumpOtherThreads = false;
            } else {
                config.dumpOtherThreads = true;
            }
            break;
        }
        if (key.compare("displayFaultStack.lowAddressStep") == 0) {
            unsigned int lowAddressStep = static_cast<unsigned int>(atoi(value.data()));
            if (lowAddressStep != 0) {
                config.lowAddressStep = lowAddressStep;
            }
            break;
        }
        if (key.compare("displayFaultStack.highAddressStep") == 0) {
            unsigned int highAddressStep = static_cast<unsigned int>(atoi(value.data()));
            if (highAddressStep != 0) {
                config.highAddressStep = highAddressStep;
            }
            break;
        }
        if (key.compare("maxFrameNums") == 0) {
            unsigned int maxFrameNums = static_cast<unsigned int>(atoi(value.data()));
            if (maxFrameNums != 0) {
                config.maxFrameNums = maxFrameNums;
            }
            break;
        }
        if (key.compare("writeSleepTime") == 0) {
            unsigned int writeSleepTime = static_cast<unsigned int>(atoi(value.data()));
            if (writeSleepTime != 0) {
                config.writeSleepTime = writeSleepTime;
            }
            break;
        }
    } while (0);
}

void DfxConfig::ReadConfig(DfxConfigInfo& config)
{
    do {
        FILE *fp = nullptr;
        char codeBuffer[CONF_LINE_SIZE] = {0};
        fp = fopen(FAULTLOGGER_CONF_PATH, "r");
        if (fp == nullptr) {
            break;
        }
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
                ParserConfig(config, key, value);
            }
        }
        (void)fclose(fp);
    } while (0);
}
} // namespace HiviewDFX
} // namespace OHOS
