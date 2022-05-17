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

/* This files contains config module. */

#include <securec.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_config.h"

namespace OHOS {
namespace HiviewDFX {
DfxConfig &DfxConfig::GetInstance()
{
    static DfxConfig instance;
    return instance;
}

void DfxConfig::SetDisplayBacktrace(bool displayBacktrace)
{
    displayBacktrace_ = displayBacktrace;
}

bool DfxConfig::GetDisplayBacktrace() const
{
    return displayBacktrace_;
}

void DfxConfig::SetDisplayMaps(bool displayMaps)
{
    displayMaps_ = displayMaps;
}

bool DfxConfig::GetDisplayMaps() const
{
    return displayMaps_;
}

void DfxConfig::SetDisplayFaultStack(bool displayFaultStack)
{
    displayFaultStack_ = displayFaultStack;
}

bool DfxConfig::GetDisplayFaultStack() const
{
    return displayFaultStack_;
}

void DfxConfig::SetFaultStackLowAddressStep(unsigned int lowAddressStep)
{
    if (lowAddressStep == 0) {
        return ;
    }
    lowAddressStep_ = lowAddressStep;
}

unsigned int DfxConfig::GetFaultStackLowAddressStep() const
{
    return lowAddressStep_;
}

void DfxConfig::SetFaultStackHighAddressStep(unsigned int highAddressStep)
{
    if (highAddressStep == 0) {
        return ;
    }
    highAddressStep_ = highAddressStep;
}

unsigned int DfxConfig::GetFaultStackHighAddressStep() const
{
    return highAddressStep_;
}

void DfxConfig::SetDisplayRegister(bool displayRegister)
{
    displayRegister_ = displayRegister;
}

bool DfxConfig::GetDisplayRegister() const
{
    return displayRegister_;
}

void DfxConfig::SetLogPersist(bool logPersist)
{
    logPersist_ = logPersist;
}

bool DfxConfig::GetLogPersist() const
{
    return logPersist_;
}

void DfxConfig::SetDumpOtherThreads(bool dumpOtherThreads)
{
    dumpOtherThreads_ = dumpOtherThreads;
}

bool DfxConfig::GetDumpOtherThreads() const
{
    return dumpOtherThreads_;
}

void DfxConfig::ParserConfig(std::string key, std::string value)
{
    unsigned int  lowAddressStep = 0;
    unsigned int  highAddressStep = 0;
        do {
        if ((key.compare("faultlogLogPersist") == 0) && (value.compare("true") == 0)) {
            DfxConfig::GetInstance().SetLogPersist(true);
            continue;
        }
        if ((key.compare("displayRigister") == 0) && (value.compare("false") == 0)) {
            DfxConfig::GetInstance().SetDisplayRegister(false);
            continue;
        }
        if ((key.compare("displayBacktrace") == 0) && (value.compare("false") == 0)) {
            DfxConfig::GetInstance().SetDisplayBacktrace(false);
            continue;
        }
        if ((key.compare("displayMaps") == 0) && (value.compare("false") == 0)) {
            DfxConfig::GetInstance().SetDisplayMaps(false);
            continue;
        }
        if ((key.compare("displayFaultStack.switch") == 0) && (value.compare("false") == 0)) {
            DfxConfig::GetInstance().SetDisplayFaultStack(false);
            continue;
        }
        if (key.compare("displayFaultStack.lowAddressStep") == 0) {
            lowAddressStep = (unsigned int)atoi(value.data());
            DfxConfig::GetInstance().SetFaultStackLowAddressStep(lowAddressStep);
            continue;
        }
        if (key.compare("displayFaultStack.highAddressStep") == 0) {
            highAddressStep = (unsigned int)atoi(value.data());
            DfxConfig::GetInstance().SetFaultStackHighAddressStep(highAddressStep);
            continue;
        }
        if ((key.compare("dumpOtherThreads") == 0) && (value.compare("true") == 0)) {
            DfxConfig::GetInstance().SetDumpOtherThreads(true);
            continue;
        }
    } while (0);
}

void DfxConfig::ReadConfig()
{
    DfxLogDebug("Enter %s.", __func__);
    do {
        FILE *fp = nullptr;
        char codeBuffer[CONF_LINE_SIZE] = {0};
#ifdef SMALL_DEVICES
        fp = fopen("/etc/faultlogger.conf", "r");
#else
        fp = fopen("/system/etc/faultlogger.conf", "r");
#endif
        if (fp == nullptr) {
            break;
        }
        while (!feof(fp)) {
            errno_t err = memset_s(codeBuffer, sizeof(codeBuffer), '\0', sizeof(codeBuffer));
            if (err != EOK) {
                DfxLogError("%s :: msmset_s codeBuffer failed..", __func__);
            }
            if (fgets(codeBuffer, CONF_LINE_SIZE -1, fp) == nullptr) {
                continue;
            }
            std::string line(codeBuffer, sizeof(codeBuffer) -1);
            std::string key, value;
            std::string::size_type newLinePos = line.find_first_of("\n");
            if (newLinePos != line.npos) {
                line = line.substr(0, newLinePos);
            }
            std::string::size_type equalSignPos = line.find_first_of("=");
            if (equalSignPos != line.npos) {
                key = line.substr(0, equalSignPos);
                value = line.substr(equalSignPos + 1);
                DfxConfig::Trim(key);
                DfxConfig::Trim(value);
                DfxConfig::ParserConfig(key, value);
            }
        }
        (void)fclose(fp);
    } while (0);
    DfxLogDebug("Exit %s.", __func__);
}

void DfxConfig::Trim(std::string &s)
{
    if (s.empty()) {
        return ;
    }
    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    return ;
}
} // namespace HiviewDFX
} // namespace OHOS
