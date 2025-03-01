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
namespace OHOS {
namespace HiviewDFX {
constexpr int LOG_FILE_NUMBER = 50;
constexpr long LOG_FILE_SIZE = 1 * 1024 * 1024;
const std::string LOG_FILE_PATH = "/data/log/faultlog/temp";
const std::string DEBUG_LOG_FILE_PATH = "/data/log/faultlog/debug";
const std::vector<const std::string> VALID_FILE_PATH = { LOG_FILE_PATH, DEBUG_LOG_FILE_PATH };

class FaultLoggerConfig {
public:
    FaultLoggerConfig(const int number, const long size, const std::string& path, const std::string& debugPath);
    ~FaultLoggerConfig();
    int GetLogFileMaxNumber() const;
    bool SetLogFileMaxNumber(const int number);
    long GetLogFileMaxSize() const;
    bool SetLogFileMaxSize(const long size);
    std::string GetLogFilePath() const;
    bool SetLogFilePath(const std::string& path);
private:
    int logFileNumber_;
    long logFileSize_;
    std::string logFilePath_;
    std::string debugLogFilePath_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
