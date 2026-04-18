/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "minidump_config.h"

namespace OHOS {
namespace HiviewDFX {

MinidumpConfigManager& MinidumpConfigManager::Instance()
{
    static MinidumpConfigManager instance;
    return instance;
}

MinidumpConfig MinidumpConfigManager::GetConfig() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void MinidumpConfigManager::SetConfig(const MinidumpConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void MinidumpConfigManager::SetMaxStreams(uint32_t value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.maxStreams = value;
}

void MinidumpConfigManager::SetMaxThreads(uint32_t value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.maxThreads = value;
}

void MinidumpConfigManager::SetMaxModules(uint32_t value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.maxModules = value;
}

void MinidumpConfigManager::SetMaxMemoryBytes(uint32_t value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.maxMemoryBytes = value;
}

void MinidumpConfigManager::SetEnableChecksumValidation(bool enable)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.enableChecksumValidation = enable;
}

MinidumpConfigManager::MinidumpConfigManager()
    : config_()
{
}

void MinidumpPerfStats::Reset()
{
    totalReadCount.store(0);
    totalReadBytes.store(0);
    totalSeekCount.store(0);
    totalParseTimeMs.store(0);
    successCount.store(0);
    errorCount.store(0);
}

void MinidumpPerfStats::IncrementRead(uint32_t bytes)
{
    totalReadCount.fetch_add(1);
    totalReadBytes.fetch_add(bytes);
}

void MinidumpPerfStats::IncrementSeek()
{
    totalSeekCount.fetch_add(1);
}

void MinidumpPerfStats::IncrementSuccess()
{
    successCount.fetch_add(1);
}

void MinidumpPerfStats::IncrementError()
{
    errorCount.fetch_add(1);
}

void MinidumpPerfStats::RecordParseTime(uint64_t ms)
{
    totalParseTimeMs.fetch_add(ms);
}

MinidumpPerfMonitor& MinidumpPerfMonitor::Instance()
{
    static MinidumpPerfMonitor instance;
    return instance;
}

MinidumpPerfStats& MinidumpPerfMonitor::GetStats()
{
    return stats_;
}

void MinidumpPerfMonitor::ResetStats()
{
    stats_.Reset();
}

MinidumpPerfMonitor::MinidumpPerfMonitor()
    : stats_()
{
}

}
}