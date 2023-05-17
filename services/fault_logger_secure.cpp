/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files contains faultlog secure module. */

#include "fault_logger_secure.h"

#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include <string>

#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static const std::string FAULTLOGGERSECURE_TAG = "FaultLoggerSecure";
static constexpr int32_t ROOT_UID = 0;
static constexpr int32_t BMS_UID = 1000;
static constexpr int32_t HIVIEW_UID = 1201;
static constexpr int32_t HIDUMPER_SERVICE_UID = 1212;
}

FaultLoggerSecure::FaultLoggerSecure()
{
}

FaultLoggerSecure::~FaultLoggerSecure()
{
}

bool FaultLoggerSecure::CheckCallerUID(const int callingUid, const int32_t pid)
{
    bool ret = false;
    if ((callingUid < 0) || (pid <= 0)) {
        return false;
    }

    // If caller's is BMS / root or caller's uid/pid is validate, just return true
    if ((callingUid == BMS_UID) ||
        (callingUid == ROOT_UID) ||
        (callingUid == HIVIEW_UID) ||
        (callingUid == HIDUMPER_SERVICE_UID)) {
        ret = true;
    } else {
        ret = false;
        DFXLOG_WARN("%s :: CheckCallerUID :: callingUid(%d).\n", FAULTLOGGERSECURE_TAG.c_str(), callingUid);
    }
    return ret;
}
} // namespace HiviewDfx
} // namespace OHOS
