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

/* This files contains faultlog config modules. */

#include "fault_logger_config.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <ctime>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <file_ex.h>
#include <securec.h>

#include "directory_ex.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
const std::string FaultLoggerConfig_TAG = "FaultLoggerConfig";
}

FaultLoggerConfig::FaultLoggerConfig(const int number, const long size, const std::string& path, const std::string& debugPath)
    :logFileNumber_(number), logFileSize_(size), logFilePath_(path), debugLogFilePath_(debugPath)
{
    DfxLogDebug("%s :: constructor :: %d, %ld, %s, %s.",
        FaultLoggerConfig_TAG.c_str(), number, size, path.c_str(), debugPath.c_str());
}

FaultLoggerConfig::~FaultLoggerConfig()
{
    DfxLogDebug("%s :: destructor.", FaultLoggerConfig_TAG.c_str());
}

int FaultLoggerConfig::GetLogFileMaxNumber() const
{
    DfxLogDebug("%s :: GetLogFileMaxNumber :: logFileNumber(%d).",
        FaultLoggerConfig_TAG.c_str(), logFileNumber_);
    return logFileNumber_;
}

bool FaultLoggerConfig::SetLogFileMaxNumber(const int number)
{
    logFileNumber_ = number;
    DfxLogDebug("%s :: SetLogFileMaxNumber :: logFileNumber(%d).",
        FaultLoggerConfig_TAG.c_str(), logFileNumber_);
    return true;
}

long FaultLoggerConfig::GetLogFileMaxSize() const
{
    DfxLogDebug("%s :: GetLogFileMaxSize :: logFileSize(%ld).",
        FaultLoggerConfig_TAG.c_str(), logFileSize_);
    return logFileSize_;
}

bool FaultLoggerConfig::SetLogFileMaxSize(const long size)
{
    logFileSize_ = size;
    DfxLogDebug("%s :: SetLogFileMaxSize :: logFileSize(%ld).",
        FaultLoggerConfig_TAG.c_str(), logFileSize_);
    return true;
}

std::string FaultLoggerConfig::GetLogFilePath() const
{
    DfxLogDebug("%s :: GetLogFilePath :: logFilePath(%s).",
        FaultLoggerConfig_TAG.c_str(), logFilePath_.c_str());
    return logFilePath_;
}

bool FaultLoggerConfig::SetLogFilePath(const std::string& path)
{
    logFilePath_ = path;
    DfxLogDebug("%s :: SetLogFilePath :: logFilePath(%s).",
        FaultLoggerConfig_TAG.c_str(), logFilePath_.c_str());
    return true;
}

std::string FaultLoggerConfig::GetDebugLogFilePath() const
{
    return debugLogFilePath_;
}

bool FaultLoggerConfig::SetDebugLogFilePath(const std::string& path)
{
    debugLogFilePath_ = path;
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
