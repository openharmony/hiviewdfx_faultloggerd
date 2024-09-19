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

#include "fault_logger_config.h"

#include <string>
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static const std::string FAULTLOGGER_CONFIG_TAG = "FaultLoggerConfig";
}

FaultLoggerConfig::FaultLoggerConfig(const int number, const long size,
                                     const std::string& path, const std::string& debugPath)
    :logFileNumber_(number), logFileSize_(size), logFilePath_(path), debugLogFilePath_(debugPath)
{
    LOGDEBUG("%{public}s :: %{public}d, %{public}ld, %{public}s, %{public}s.",
        FAULTLOGGER_CONFIG_TAG.c_str(), number, size, path.c_str(), debugPath.c_str());
}

FaultLoggerConfig::~FaultLoggerConfig()
{
}

int FaultLoggerConfig::GetLogFileMaxNumber() const
{
    LOGDEBUG("%{public}s :: GetLogFileMaxNumber(%{public}d).",
        FAULTLOGGER_CONFIG_TAG.c_str(), logFileNumber_);
    return logFileNumber_;
}

bool FaultLoggerConfig::SetLogFileMaxNumber(const int number)
{
    logFileNumber_ = number;
    LOGDEBUG("%{public}s :: SetLogFileMaxNumber(%{public}d).",
        FAULTLOGGER_CONFIG_TAG.c_str(), logFileNumber_);
    return true;
}

long FaultLoggerConfig::GetLogFileMaxSize() const
{
    LOGDEBUG("%{public}s :: GetLogFileMaxSize(%{public}ld).",
        FAULTLOGGER_CONFIG_TAG.c_str(), logFileSize_);
    return logFileSize_;
}

bool FaultLoggerConfig::SetLogFileMaxSize(const long size)
{
    logFileSize_ = size;
    LOGDEBUG("%{public}s :: SetLogFileMaxSize(%{public}ld).",
        FAULTLOGGER_CONFIG_TAG.c_str(), logFileSize_);
    return true;
}

std::string FaultLoggerConfig::GetLogFilePath() const
{
    LOGDEBUG("%{public}s :: GetLogFilePath(%{public}s).",
        FAULTLOGGER_CONFIG_TAG.c_str(), logFilePath_.c_str());
    return logFilePath_;
}

bool FaultLoggerConfig::SetLogFilePath(const std::string& path)
{
    logFilePath_ = path;
    LOGDEBUG("%{public}s :: SetLogFilePath(%{public}s).",
        FAULTLOGGER_CONFIG_TAG.c_str(), logFilePath_.c_str());
    return true;
}

std::string FaultLoggerConfig::GetDebugLogFilePath() const
{
    LOGDEBUG("%{public}s :: GetDebugLogFilePath(%{public}s).",
        FAULTLOGGER_CONFIG_TAG.c_str(), debugLogFilePath_.c_str());
    return debugLogFilePath_;
}

bool FaultLoggerConfig::SetDebugLogFilePath(const std::string& path)
{
    debugLogFilePath_ = path;
    LOGDEBUG("%{public}s :: SetDebugLogFilePath(%{public}s).",
        FAULTLOGGER_CONFIG_TAG.c_str(), debugLogFilePath_.c_str());
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
