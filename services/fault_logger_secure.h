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

/* This files contains header of secure module. */

#ifndef _FAULT_LOGGER_SECURE_H
#define _FAULT_LOGGER_SECURE_H

#include <string>

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerSecure {
public:
    FaultLoggerSecure();
    ~FaultLoggerSecure();
    static bool CheckCallerUID(const int callingUid, const int32_t pid);

public:
    constexpr static int32_t INVALID_UID = -1;
    constexpr static int32_t INVALID_GID = -1;
    constexpr static int32_t ROOT_UID = 0;
    constexpr static int32_t BMS_UID = 1000;
    constexpr static int32_t BMS_GID = 1000;
    constexpr static int32_t HIVIEW_UID = 1201;
    constexpr static int32_t BASE_SYS_UID = 2100;
    constexpr static int32_t MAX_SYS_UID = 2899;
    constexpr static int32_t BASE_SYS_VEN_UID = 5000;
    constexpr static int32_t MAX_SYS_VEN_UID = 5999;
    constexpr static int32_t BASE_APP_UID = 10000;
    constexpr static int32_t MAX_APP_UID = 65535;
    constexpr static int32_t MAX_RESP_LEN = 128;
    constexpr static int32_t MAX_CMD_LEN = 1024;

private:
    static bool CheckUidAndPid(const int uid, const int32_t pid);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
