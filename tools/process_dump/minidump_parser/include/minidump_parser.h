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

#ifndef MINIDUMP_PARSER_H
#define MINIDUMP_PARSER_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "minidump_optimizer.h"
#include "minidump_config.h"
#include "minidump_error.h"
#include "minidump_factory.h"
#include "minidump_format.h"
#include "minidump_observer.h"
#include "minidump_stream.h"

namespace OHOS {
namespace HiviewDFX {
using namespace MinidumpFormat;

class MinidumpParser {
public:
    explicit MinidumpParser(const std::string& path);
    explicit MinidumpParser(std::shared_ptr<std::istream> input);
    MinidumpParser(const MinidumpParser&) = delete;
    void operator=(const MinidumpParser&) = delete;
    ~MinidumpParser();

    virtual std::string Path() const { return path_; }
    const MDRawHeader* Header() const { return isValid_ ? &header_ : nullptr; }

    MinidumpThreadList* GetThreadList();
    MinidumpThreadNameList* GetThreadNameList();
    MinidumpModuleList* GetModuleList();
    MinidumpMemoryList* GetMemoryList();
    MinidumpException* GetException();
    MinidumpSystemInfo* GetSystemInfo();
    MinidumpMiscInfo* GetMiscInfo();
    MinidumpMapList* GetMapList();
    MinidumpEsrInfo* GetEsrInfo();
    template<typename T>
    T* GetStream();

    uint32_t GetDirectoryEntryCount() const
    {
        return isValid_ ? header_.numberOfStreams : 0;
    }

    const MDRawDirectory* GetDirectoryEntryAtType(uint32_t streamType) const;
    const MDRawDirectory* GetDirectoryEntryAtIndex(uint32_t index) const;

    bool Parse();

    bool SeekToStreamType(uint32_t streamType, uint32_t& streamLen);

    bool Valid() const { return isValid_; }
    const MinidumpErrorInfo& GetLastError() const { return lastError_; }
    void Print();
    void SetupObservers();
    void SetPerformanceConfig(const PerformanceOptimizer::Config& config);
    void PrintPerformanceStats() const;

private:
    MinidumpParser() = default;
    struct MinidumpStreamInfo {
        MinidumpStreamInfo() : streamIndex_(0), stream_(nullptr) {}
        ~MinidumpStreamInfo() = default;

        uint32_t streamIndex_;
        std::shared_ptr<MinidumpStream> stream_;
    };

    typedef std::vector<MDRawDirectory> MDRawDirectoryEntries;
    typedef std::map<uint32_t, MinidumpStreamInfo> MinidumpStreamMap;

    bool Open();
    bool ReadMinidumpHeader();
    bool ReadStreamDirectory();
    bool ValidateFileHeader();
    void RecordParseTime(uint64_t startTimeMs);
    std::string GetStreamTypeName(uint32_t streamType);

    MDRawHeader header_;
    std::shared_ptr<MDRawDirectoryEntries> directory_;
    std::shared_ptr<MinidumpStreamMap> streamMap_;
    const std::string path_;
    std::shared_ptr<std::istream> stream_;
    std::shared_ptr<MinidumpMemoryReader> memoryReader_;
    bool isValid_;
    std::shared_ptr<MinidumpSubject> minidumpSubject_;
    MinidumpErrorInfo lastError_;
};
}
}
#endif