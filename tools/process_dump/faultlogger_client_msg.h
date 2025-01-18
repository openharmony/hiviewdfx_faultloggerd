/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef FAULTLOGGER_CLIENT_MSG_H_
#define FAULTLOGGER_CLIENT_MSG_H_

#include <cstdint>
#include <map>
#include <string>

/**
* @brief  information of fault log
*
*/
struct FaultDFXLOGIInner {
    uint64_t time{0};
    uint32_t id{0};
    int32_t pid{-1};
    int32_t pipeFd{-1};
    int32_t faultLogType{0};
    std::string module;
    std::string reason;
    std::string summary;
    std::string logPath;
    std::string registers;
    std::map<std::string, std::string> sectionMaps;
};

#if defined(__arm__)
static_assert(sizeof(FaultDFXLOGIInner) == 96, "faultlogger plugin must be modify together!!!");
#elif defined(__aarch64__)
static_assert(sizeof(FaultDFXLOGIInner) == 168, "faultlogger plugin must be modify together!!!");
#endif

#endif //FAULTLOGGER_CLIENT_MSG_H_
