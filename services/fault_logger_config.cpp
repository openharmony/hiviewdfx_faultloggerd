/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "fault_logger_config.h"

#include <algorithm>
#include <limits>

#ifndef is_ohos_lite
#include "cJSON.h"
#endif

#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "file_ex.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
#ifndef is_ohos_lite
constexpr const char* FAULTLOGGER_CONFIG_TAG = "FAULT_LOGGER_CONFIG";
constexpr const char* const CONFIG_FILE_PATH = "/system/etc/faultloggerd_config.json";
constexpr int32_t KB_TO_B = 10;
constexpr int32_t MAX_NUM_INT32 = std::numeric_limits<int32_t>::max();

constexpr const char* const OVER_TIME_FILE_DELETE_ACTIVE = "ACTIVE";
constexpr const char* const OVER_SIZE_ACTION_DELETE = "DELETE";

std::string GetStringValueFromJson(const cJSON* json, std::string defaultValue = "")
{
    if (!cJSON_IsString(json)) {
        return defaultValue;
    }
    return json->valuestring;
}

int32_t GetInt32ValueFromJson(const cJSON* json, int32_t defaultValue = 0)
{
    if (!cJSON_IsNumber(json)) {
        return defaultValue;
    }
    return json->valueint;
}

void ParseSingleFileConfig(const cJSON* tempFile,  std::vector<SingleFileConfig>& singleFileConfigs)
{
    if (tempFile == nullptr) {
        DFXLOGE("%{public}s :: failed to Parse TempFileConfig", FAULTLOGGER_CONFIG_TAG);
        return;
    }
    auto& tempFileConfig = singleFileConfigs.emplace_back();
    tempFileConfig.type = GetInt32ValueFromJson(cJSON_GetObjectItem(tempFile, "type"));
    tempFileConfig.fileNamePrefix = GetStringValueFromJson(cJSON_GetObjectItem(tempFile, "fileNamePrefix"));
    int32_t maxFileSize = std::clamp(GetInt32ValueFromJson(cJSON_GetObjectItem(tempFile, "maxSingleFileSizeInKB")),
                                     0, MAX_NUM_INT32);
    tempFileConfig.maxSingleFileSize = static_cast<uint64_t>(maxFileSize) << KB_TO_B;
    int32_t fileExistTime = GetInt32ValueFromJson(cJSON_GetObjectItem(tempFile, "fileExistTimeInSecond"), -1);
    tempFileConfig.fileExistTime = std::clamp(fileExistTime, -1, MAX_NUM_INT32);
    int32_t keeFileCount = GetInt32ValueFromJson(cJSON_GetObjectItem(tempFile, "keepFileCount"), -1);
    tempFileConfig.keepFileCount = std::clamp(keeFileCount, -1, MAX_NUM_INT32);
    int32_t maxFileCount = GetInt32ValueFromJson(cJSON_GetObjectItem(tempFile, "maxFileCount"), -1);
    tempFileConfig.maxFileCount = std::clamp(maxFileCount, tempFileConfig.keepFileCount, MAX_NUM_INT32);
    if (GetStringValueFromJson(cJSON_GetObjectItem(tempFile, "overTimeFileDeleteType")) ==
        OVER_TIME_FILE_DELETE_ACTIVE) {
        tempFileConfig.overTimeFileDeleteType = OverTimeFileDeleteType::ACTIVE;
    }
    if (GetStringValueFromJson(cJSON_GetObjectItem(tempFile, "overFileSizeAction")) ==
        OVER_SIZE_ACTION_DELETE) {
        tempFileConfig.overFileSizeAction = OverFileSizeAction::DELETE;
    }
}

void ParseDirectoryConfig(const cJSON* json, DirectoryConfig& directoryConfig)
{
    if (json == nullptr) {
        DFXLOGE("%{public}s :: failed to Parse directoryConfig", FAULTLOGGER_CONFIG_TAG);
        return;
    }
    directoryConfig.tempFilePath = GetStringValueFromJson(cJSON_GetObjectItem(json, "tempFilePath"));
    int32_t maxFileSize = std::clamp(GetInt32ValueFromJson(cJSON_GetObjectItem(json, "maxTempFilesSizeInKB")),
                                     0, MAX_NUM_INT32);
    directoryConfig.maxTempFilesSize = static_cast<uint64_t>(maxFileSize) << KB_TO_B;
    int32_t configClearTime = GetInt32ValueFromJson(cJSON_GetObjectItem(json, "fileClearTimeAfterBootInSecond"));
    directoryConfig.fileClearTimeAfterBoot = std::clamp(configClearTime, 0, MAX_NUM_INT32);
    auto* tempFiles = cJSON_GetObjectItem(json, "tempFiles");
    if (tempFiles != nullptr && cJSON_IsArray(tempFiles)) {
        int arraySize = cJSON_GetArraySize(tempFiles);
        for (int i = 0; i < arraySize; i++) {
            ParseSingleFileConfig(cJSON_GetArrayItem(tempFiles, i), directoryConfig.singleFileConfigs);
        }
    }
}
#endif

#ifdef FAULTLOGGERD_TEST
void InitFaultloggerTestConfig(DirectoryConfig& directoryConfig)
{
    directoryConfig.tempFilePath = "/data/test/faultloggerd/temp";
    constexpr uint64_t maxTempFilesSize = 4ull << 10; // 4KB
    directoryConfig.maxTempFilesSize = maxTempFilesSize;
    constexpr int32_t fileClearTimeAfterBoot = 3; // 3S
    directoryConfig.fileClearTimeAfterBoot = fileClearTimeAfterBoot;
    directoryConfig.singleFileConfigs = {
        {
            .type = FaultLoggerType::CPP_CRASH,
            .fileNamePrefix = "cppcrash",
            .maxSingleFileSize = 5 << 10, // 5KB
            .fileExistTime = 60,
            .keepFileCount = 5,
            .maxFileCount = 7
        },
        {
            .type = FaultLoggerType::JS_HEAP_SNAPSHOT,
            .fileNamePrefix = "jsheap",
            .maxSingleFileSize = 5 << 10, // 5KB
            .overFileSizeAction = OverFileSizeAction::DELETE,
            .fileExistTime = 3,
            .overTimeFileDeleteType = OverTimeFileDeleteType::ACTIVE,
            .keepFileCount = 5,
            .maxFileCount = 7
        },
        {
            .type = FaultLoggerType::LEAK_STACKTRACE,
            .fileNamePrefix = "leakstack",
            .overTimeFileDeleteType = OverTimeFileDeleteType::ACTIVE,
            .keepFileCount = -1,
            .maxFileCount = 2,
        }
    };
}
#endif
}

FaultLoggerConfig& FaultLoggerConfig::GetInstance()
{
    static FaultLoggerConfig faultLoggerConfig;
    return faultLoggerConfig;
}

#ifndef is_ohos_lite
FaultLoggerConfig::FaultLoggerConfig()
{
    std::string content;
    OHOS::LoadStringFromFile(CONFIG_FILE_PATH, content);
    cJSON* json = cJSON_Parse(content.c_str());
    if (json == nullptr) {
        DFXLOGE("%{public}s :: failed to parse json from the content of file(%{public}s).",
                FAULTLOGGER_CONFIG_TAG, CONFIG_FILE_PATH);
    }
    ParseDirectoryConfig(json, directoryConfig_);
    cJSON_Delete(json);
#ifdef FAULTLOGGERD_TEST
    InitFaultloggerTestConfig(directoryConfig_);
#endif
}
#else
FaultLoggerConfig::FaultLoggerConfig()
{
    directoryConfig_.tempFilePath = "/data/log/faultlog/temp";
    constexpr uint64_t maxTempFilesSize = 2ull << 30; // 2GB
    directoryConfig_.maxTempFilesSize = maxTempFilesSize;
    constexpr int32_t fileClearTimeAfterBoot = 300; // 300s
    directoryConfig_.fileClearTimeAfterBoot = fileClearTimeAfterBoot;
    directoryConfig_.singleFileConfigs = {
        {
            .type = FaultLoggerType::CPP_CRASH,
            .fileNamePrefix = "cppcrash",
            .maxSingleFileSize = 2 << 20, // 2M
            .keepFileCount = 20,
            .maxFileCount = 40
        },
        {
            .type = FaultLoggerType::CPP_STACKTRACE,
            .fileNamePrefix = "stacktrace",
            .maxSingleFileSize = 2 << 20, // 2M
            .keepFileCount = 20,
            .maxFileCount = 40
        },
        {
            .type = FaultLoggerType::LEAK_STACKTRACE,
            .fileNamePrefix = "leakstack",
            .maxSingleFileSize = 512 << 10, // 512K
            .fileExistTime = 60, // 60s
            .keepFileCount = 20,
            .maxFileCount = 40
        },
        {
            .type = FaultLoggerType::JIT_CODE_LOG,
            .fileNamePrefix = "jitcode",
            .maxSingleFileSize = 128 << 10, // 128K
            .fileExistTime = 60, // 60s
            .keepFileCount = 20,
            .maxFileCount = 40
        }
    };
#ifdef FAULTLOGGERD_TEST
    InitFaultloggerTestConfig(directoryConfig_);
#endif
}
#endif

const DirectoryConfig& FaultLoggerConfig::GetTempFileConfig() const
{
    return directoryConfig_;
}
} // namespace HiviewDFX
} // namespace OHOS
