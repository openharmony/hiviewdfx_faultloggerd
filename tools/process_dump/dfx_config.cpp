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

#include "dfx_config.h"
#include <securec.h>
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

void DfxConfig::readConfig()
{
    do {
        FILE *fp = nullptr;
        char line[CONF_LINE_SIZE] = {0};
        fp = fopen("/system/etc/faultlogger.conf", "r");
        if (fp == nullptr) {
            break;
        }
        while (!feof(fp)) {
            memset_s(line, sizeof(CONF_LINE_SIZE - 1), '\0', sizeof(line));
            if (fgets(line, 1023, fp) == nullptr) {
                continue;
            }
            if (!strncmp("faultlogLogPersist=true", line, strlen("faultlogLogPersist=true"))) {
                DfxConfig::GetInstance().SetLogPersist(true);
                continue;
            }
            if (!strncmp("displayRigister=false", line, strlen("displayRigister=false"))) {
                DfxConfig::GetInstance().SetDisplayRegister(false);
                continue;
            }
            if (!strncmp("displayBacktrace=false", line, strlen("displayBacktrace=false"))) {
                DfxConfig::GetInstance().SetDisplayBacktrace(false);
                continue;
            }
            if (!strncmp("displayMaps=false", line, strlen("displayMaps=false"))) {
                DfxConfig::GetInstance().SetDisplayMaps(false);
                continue;
            }
        }

        fclose(fp);
        return ;
    } while (0);
}


} // namespace HiviewDFX
} // namespace OHOS