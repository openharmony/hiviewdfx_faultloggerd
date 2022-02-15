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

bool DfxConfig::parserConfig(char* filterKey, int keySize, char* filterValue, int valueSize)
{
    DfxLogDebug("Enter %s :: keySize(%d), valueSize(%d).", __func__, keySize, valueSize);

    bool ret = false;
    unsigned int  lowAddressStep = 0;
    unsigned int  highAddressStep = 0;

    do {
        if ((strcmp("faultlogLogPersist", filterKey) == 0) && (strcmp("false", filterValue) != 0)) {
            DfxConfig::GetInstance().SetLogPersist(true);
            ret = true;
            continue;
        }
        if ((strcmp("displayRigister", filterKey) == 0) && (strcmp("true", filterValue) != 0)) {
            DfxConfig::GetInstance().SetDisplayRegister(false);
            ret = true;
            continue;
        }
        if ((strcmp("displayBacktrace", filterKey) == 0) && (strcmp("true", filterValue) != 0)) {
            DfxConfig::GetInstance().SetDisplayBacktrace(false);
            ret = true;
            continue;
        }
        if ((strcmp("displayMaps", filterKey) == 0) && (strcmp("true", filterValue) != 0)) {
            DfxConfig::GetInstance().SetDisplayMaps(false);
            ret = true;
            continue;
        }
        if ((strcmp("displayFaultStack.switch", filterKey) == 0) && (strcmp("true", filterValue) != 0)) {
            DfxConfig::GetInstance().SetDisplayFaultStack(false);
            ret = true;
            continue;
        }
        if (strcmp("displayFaultStack.lowAddressStep", filterKey) == 0) {
            lowAddressStep = atoi(filterValue);
            DfxConfig::GetInstance().SetFaultStackLowAddressStep(lowAddressStep);
            ret = true;
            continue;
        }
        if (strcmp("displayFaultStack.highAddressStep", filterKey) == 0) {
            highAddressStep = atoi(filterValue);
            DfxConfig::GetInstance().SetFaultStackHighAddressStep(highAddressStep);
            ret = true;
            continue;
        }
    } while (0);

    DfxLogDebug("Exit %s :: ret(%d).", __func__, ret);
    return ret;
}

void DfxConfig::readConfig()
{
    DfxLogDebug("Enter %s.", __func__);
    do {
        FILE *fp = nullptr;
        char line[CONF_LINE_SIZE] = {0};
        char key[CONF_KEY_SIZE] = {};
        char value[CONF_VALUE_SIZE] = {};
        char filterKey[CONF_KEY_SIZE] = {};
        char filterValue[CONF_VALUE_SIZE] = {};
        unsigned int  lowAddressStep = 0;
        unsigned int  highAddressStep = 0;
        fp = fopen("/system/etc/faultlogger.conf", "r");
        if (fp == nullptr) {
            break;
        }
        while (!feof(fp)) {
            memset_s(line, sizeof(line), '\0', sizeof(line));
            memset_s(key, sizeof(key), '\0', sizeof(key));
            memset_s(value, sizeof(value), '\0', sizeof(value));
            memset_s(filterKey, sizeof(filterKey), '\0', sizeof(filterKey));
            memset_s(filterValue, sizeof(filterValue), '\0', sizeof(filterValue));
            if (fgets(line, CONF_LINE_SIZE -1, fp) == nullptr) {
                continue;
            }
            foramtKV(line, key, value);
            trim(key, filterKey);
            trim(value, filterValue);

            if (parserConfig(filterKey, CONF_KEY_SIZE, filterValue, CONF_VALUE_SIZE)) {
                continue;
            }
        }
        (void)fclose(fp);
    } while (0);
    DfxLogDebug("Exit %s.", __func__);
}

void DfxConfig::trim(char *strIn, char *strOut)
{
    char *temp = strIn;

    while (*temp == ' ') {
        temp++;
    }
    char *start = temp;

    temp = strIn + strlen(strIn) -1;
    while (*temp == ' ') {
        temp--;
    }
    char *end = temp;

    for (strIn = start; strIn <= end;) {
        *strOut++ = *strIn++;
    }
    *strOut = '\0';
}

void DfxConfig::foramtKV(char * line, char * key, char * value)
{
    char *p = NULL;
    char *end = line + strlen(line) -1;
    p = strstr(line, "=");
    if (p == NULL) {
        return ;
    }
    for (; line < p;) {
        *key++ = *line++;
    }
    *key = '\0';
    line++;

    for (; line <= end;) {
        if (*line == '\n') {
            break;
        }
        *value++ = *line++;
    }
    *value = '\0';
}
} // namespace HiviewDFX
} // namespace OHOS