/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef FAULT_LOGGER_CONFIG_H
#define FAULT_LOGGER_CONFIG_H

#include <string>
#include <vector>
#include <cstdint>

namespace OHOS {
namespace HiviewDFX {

enum class OverFileSizeAction {
    CROP = 0, // default
    DELETE,
};

enum class OverTimeFileDeleteType {
    PASSIVE = 0, // default
    ACTIVE,
};

struct SingleFileConfig {
    int32_t type;
    std::string fileNamePrefix;
    uint64_t maxSingleFileSize;
    OverFileSizeAction overFileSizeAction;
    int32_t fileExistTime;
    OverTimeFileDeleteType overTimeFileDeleteType;
    int32_t keepFileCount;
    int32_t maxFileCount;
};

struct DirectoryConfig {
    std::string tempFilePath;
    uint64_t maxTempFilesSize;
    int32_t fileClearTimeAfterBoot;
    std::vector<SingleFileConfig> singleFileConfigs;
};

class FaultLoggerConfig {
public:
    FaultLoggerConfig(const FaultLoggerConfig&) = delete;
    FaultLoggerConfig(FaultLoggerConfig&&) = delete;

    FaultLoggerConfig &operator=(const FaultLoggerConfig&) = delete;
    FaultLoggerConfig &operator=(FaultLoggerConfig&&) = delete;
    static FaultLoggerConfig& GetInstance();
    [[nodiscard]] const DirectoryConfig& GetTempFileConfig() const;
private:
    FaultLoggerConfig();
    ~FaultLoggerConfig() = default;
    DirectoryConfig directoryConfig_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
