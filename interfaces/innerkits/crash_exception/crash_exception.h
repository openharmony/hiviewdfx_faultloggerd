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

#ifndef CRASH_EXCEPTION_H
#define CRASH_EXCEPTION_H

#include <cinttypes>
#include <string>
#include "dfx_exception.h"
#include "dfx_dump_request.h"

namespace OHOS {
namespace HiviewDFX {

/**
 * @brief fault log file check keyword info
*/
struct LogValidCheckInfo {
    /** key words for check */
    std::string key;
    /** regex rule */
    std::string regx;
    /** offset of the key words in file */
    std::string::size_type start;
    /** error code when file invalid */
    int32_t errCode;
};

/**
 * @brief get current time
*/
uint64_t GetTimeMillisec(void);

/**
 * @brief save crashed process info
*/
void SetCrashProcInfo(const ProcessDumpType& dumpType, const std::string& name, int32_t pid, int32_t uid);

/**
 * @brief report crash to sysevent
*/
void ReportCrashException(int32_t errCode);

/**
 * @brief report crash to sysevent
*/
void ReportCrashException(std::string name, int32_t pid, int32_t uid, int32_t errCode);

/**
 * @brief report crash unwinder error to sysevent
*/
void ReportUnwinderException(uint16_t unwError);

/**
 * @brief Check summary valid,The valid is if the string contains stack information for the first three frames
 *
 * @param summary Stack information in the fault info
 * @return if check valid return true, otherwise return false
*/
bool CheckFaultSummaryValid(const std::string &summary);
}
}
#endif