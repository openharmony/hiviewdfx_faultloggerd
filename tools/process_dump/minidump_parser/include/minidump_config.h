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

#ifndef MINIDUMP_CONFIG_H
#define MINIDUMP_CONFIG_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace OHOS {
namespace HiviewDFX {

struct MinidumpConfig {
    uint32_t maxStreams = 128;
    uint32_t maxStringLength = 1024;
    uint32_t maxThreads = 400;
    uint32_t maxModules = 2048;
    uint32_t maxMemoryRegions = 4096;
    uint32_t maxMemoryBytes = 64 * 1024 * 1024;
    bool enableChecksumValidation = true;
    bool enablePerformanceStats = true;
};

class MinidumpConfigManager {
public:
    static MinidumpConfigManager& Instance();
    MinidumpConfig GetConfig() const;
    void SetConfig(const MinidumpConfig& config);
    void SetMaxStreams(uint32_t value);
    void SetMaxThreads(uint32_t value);
    void SetMaxModules(uint32_t value);
    void SetMaxMemoryBytes(uint32_t value);
    void SetEnableChecksumValidation(bool enable);

    MinidumpConfigManager(const MinidumpConfigManager&) = delete;
    MinidumpConfigManager& operator=(const MinidumpConfigManager&) = delete;

private:
    MinidumpConfigManager();
    mutable std::mutex mutex_;
    MinidumpConfig config_;
};

struct MinidumpPerfStats {
    std::atomic<uint64_t> totalReadCount{0};
    std::atomic<uint64_t> totalReadBytes{0};
    std::atomic<uint64_t> totalSeekCount{0};
    std::atomic<uint64_t> totalParseTimeMs{0};
    std::atomic<uint64_t> successCount{0};
    std::atomic<uint64_t> errorCount{0};

    void Reset();
    void IncrementRead(uint32_t bytes);
    void IncrementSeek();
    void IncrementSuccess();
    void IncrementError();
    void RecordParseTime(uint64_t ms);
};

class MinidumpPerfMonitor {
public:
    static MinidumpPerfMonitor& Instance();
    MinidumpPerfStats& GetStats();
    void ResetStats();

    MinidumpPerfMonitor(const MinidumpPerfMonitor&) = delete;
    MinidumpPerfMonitor& operator=(const MinidumpPerfMonitor&) = delete;

private:
    MinidumpPerfMonitor();
    MinidumpPerfStats stats_;
};

}
}

#endif