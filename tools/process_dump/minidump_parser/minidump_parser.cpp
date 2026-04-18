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

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "dfx_log.h"
#include "minidump_factory.h"
#include "minidump_parser.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxMinidump"
#endif

namespace OHOS {
namespace HiviewDFX {
MinidumpParser::MinidumpParser(const std::string& path)
    : directory_(nullptr),
      streamMap_(std::make_shared<MinidumpStreamMap>()),
      path_(path),
      stream_(nullptr),
      isValid_(false),
      minidumpSubject_(std::make_shared<MinidumpSubject>())
{
}

MinidumpParser::MinidumpParser(std::shared_ptr<std::istream> input)

    : directory_(nullptr),
      streamMap_(std::make_shared<MinidumpStreamMap>()),
      stream_(input),
      isValid_(false),
      minidumpSubject_(std::make_shared<MinidumpSubject>())
{
}

MinidumpParser::~MinidumpParser()
{
    if (stream_) {
        DFXLOGI("MinidumpParser closing memoryReader");
    }
    stream_.reset();
    PerformanceOptimizer::Instance().Reset();
}

bool MinidumpParser::Open()
{
    if (path_.empty()) {
        if (!stream_) {
            return false;
        }
        return stream_->good();
    }

    stream_= std::make_shared<std::ifstream>(path_, std::ios::binary);
    std::ifstream* fileStream = static_cast<std::ifstream*>(stream_.get());
    if (!fileStream->is_open()) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_OPEN, "Cannot open file: " + path_, __LINE__);
        DFXLOGE("MinidumpParser could not open path: %{public}s errno%{public}d", path_.c_str(), errno);
        return false;
    }
    return true;
}

bool MinidumpParser::ValidateFileHeader()
{
    if (header_.signature != MINIDUMP_HEADER_SIGNATURE) {
        if (header_.signature == MINIDUMP_HEADER_SIGNATURE_SWAP) {
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_ENDIANNESS, "Big-endian file not supported", __LINE__);
            DFXLOGE("MinidumpParser file is big-endian, only little-endian files are supported");
        } else {
            lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_SIGNATURE,
            "Invalid signature: 0x" + std::to_string(header_.signature), __LINE__);
            DFXLOGE("MinidumpParser header signature mismatch: 0x%{public}x", header_.signature);
        }
        return false;
    }

    if ((header_.version & 0x0000ffff) != MINIDUMP_HEADER_VERSION) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_VERSION, "Version mismatch", __LINE__);
        DFXLOGE("MinidumpParser version mismatch: 0x%{public}u", header_.version);
        return false;
    }

    if (header_.numberOfStreams == 0 ||
        header_.numberOfStreams > MinidumpConfigManager::Instance().GetConfig().maxStreams) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_COUNT, "Stream count invalid", __LINE__);
        DFXLOGE("MinidumpParser stream count %{public}u exceeds maximum %{public}u",
                header_.numberOfStreams, MinidumpConfigManager::Instance().GetConfig().maxStreams);
        return false;
    }

    return true;
}

void MinidumpParser::RecordParseTime(uint64_t startTimeMs)
{
    if (MinidumpConfigManager::Instance().GetConfig().enablePerformanceStats) {
        auto endTime = std::chrono::steady_clock::now();
        auto endMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime.time_since_epoch()).count();
        auto& stats = MinidumpPerfMonitor::Instance().GetStats();
        stats.RecordParseTime(endMs - startTimeMs);
    }
}
void MinidumpParser::SetupObservers()
{
    auto progressObserver = std::make_shared<ProgressObserver>(
        [](uint32_t current, uint32_t total, const std::string& name) {
            if (!name.empty()) {
                DFXLOGI("Progress: %{public}u/%{public}u - %{public}s", current, total, name.c_str());
            }
        });
    minidumpSubject_->AddObserver(progressObserver);

    auto streamLoadObserver = std::make_shared<StreamLoadObserver>(
    [](const std::string& name, uint32_t progress) {
        if (!name.empty()) {
            DFXLOGI("Stream Load: %{public}s process count %{public}u", name.c_str(), progress);
        }
    });
    minidumpSubject_->AddObserver(streamLoadObserver);
}

bool MinidumpParser::ReadMinidumpHeader()
{
    if (!Open()) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_OPEN, "Cannot open file", __LINE__);
        DFXLOGE("MinidumpParser cannot open file");
        return false;
    }
    memoryReader_ = std::make_shared<MinidumpMemoryReader>(stream_);
    if (!memoryReader_->ReadBytes(&header_, sizeof(header_))) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_READ,
            "Cannot read header", __LINE__);
        DFXLOGE("MinidumpParser cannot read header");
        return false;
    }
    uint32_t curProcess = 1;
    uint32_t totalStep = header_.numberOfStreams + 2;
    minidumpSubject_->NotifyParseProgress(curProcess++, totalStep, "header");

    if (!ValidateFileHeader()) {
        return false;
    }
    minidumpSubject_->NotifyParseProgress(curProcess++, totalStep, "validation");
    return true;
}

bool MinidumpParser::ReadStreamDirectory()
{
    if (header_.streamDirectoryRva < sizeof(MDRawHeader)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_CORRUPTED_DATA,
            "Stream directory RVA overlaps with header", __LINE__);
        DFXLOGE("Stream directory RVA 0x%{public}u too small", header_.streamDirectoryRva);
        return false;
    }
    if (!memoryReader_->SeekSet(header_.streamDirectoryRva)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "Cannot seek to stream directory", __LINE__);
        DFXLOGE("MinidumpParser cannot seek to stream directory: 0x%{public}u", header_.streamDirectoryRva);
        return false;
    }

    auto directory = std::make_shared<MDRawDirectoryEntries>(header_.numberOfStreams);
    if (!memoryReader_->ReadBytes(directory->data(), sizeof(MDRawDirectory) * header_.numberOfStreams)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_STREAM_READ, "Cannot read stream directory", __LINE__);
        DFXLOGE("MinidumpParser cannot read stream directory");
        return false;
    }

    uint32_t currentStep = 3;
    uint32_t totalStep = header_.numberOfStreams + 2;
    for (uint32_t i = 0; i < header_.numberOfStreams; ++i) {
        MDRawDirectory* directoryEntry = &(*directory)[i];
        minidumpSubject_->NotifyParseProgress(currentStep++, totalStep, GetStreamTypeName(directoryEntry->streamType));

        switch (directoryEntry->streamType) {
            case MD_STREAM_THREAD_LIST:
            case MD_STREAM_THREAD_NAME_LIST:
            case MD_STREAM_MODULE_LIST:
            case MD_STREAM_MEMORY_LIST:
            case MD_STREAM_EXCEPTION:
            case MD_STREAM_SYSTEM_INFO:
            case MD_STREAM_MISC_INFO:
            case MD_STREAM_LINUX_MAPS: {
                if (streamMap_->find(directoryEntry->streamType) != streamMap_->end()) {
                    lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_ALREADY_EXISTS,
                        "Duplicate stream type", __LINE__);
                    DFXLOGE("MinidumpParser found multiple streams of type: %{public}u", directoryEntry->streamType);
                    return false;
                }
                [[fallthrough]];
            }
            default: {
                (*streamMap_)[directoryEntry->streamType].streamIndex_ = i;
                break;
            }
        }
    }
    directory_ = std::move(directory);
    return true;
}

bool MinidumpParser::Parse()
{
    auto startTime = std::chrono::steady_clock::now();
    auto startMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        startTime.time_since_epoch()).count();
    SetupObservers();

    minidumpSubject_->NotifyParseStart(path_);

    isValid_ = false;
    if (!ReadMinidumpHeader()) {
        return false;
    }

    if (!ReadStreamDirectory()) {
        return false;
    }

    isValid_ = true;
    lastError_.Clear();

    auto& stats = MinidumpPerfMonitor::Instance().GetStats();
    stats.IncrementSuccess();
    RecordParseTime(startMs);

    minidumpSubject_->NotifyParseComplete(true);

    return true;
}

std::string MinidumpParser::GetStreamTypeName(uint32_t streamType)
{
    switch (streamType) {
        case MD_STREAM_THREAD_LIST:
            return "ThreadList";
        case MD_STREAM_THREAD_NAME_LIST:
            return "ThreadNameList";
        case MD_STREAM_MODULE_LIST:
            return "ModuleList";
        case MD_STREAM_MEMORY_LIST:
            return "MemoryList";
        case MD_STREAM_EXCEPTION:
            return "Exception";
        case MD_STREAM_SYSTEM_INFO:
            return "SystemInfo";
        case MD_STREAM_MISC_INFO:
            return "MiscInfo";
        case MD_STREAM_LINUX_MAPS:
            return "LinuxMaps";
        case MD_STREAM_ESR_INFO:
            return "EsrInfo";
        default:
            return "Unknown(" + std::to_string(streamType) + ")";
    }
}

MinidumpThreadList* MinidumpParser::GetThreadList()
{
    return GetStream<MinidumpThreadList>();
}

MinidumpThreadNameList* MinidumpParser::GetThreadNameList()
{
    return GetStream<MinidumpThreadNameList>();
}

MinidumpModuleList* MinidumpParser::GetModuleList()
{
    return GetStream<MinidumpModuleList>();
}

MinidumpMemoryList* MinidumpParser::GetMemoryList()
{
    return GetStream<MinidumpMemoryList>();
}

MinidumpException* MinidumpParser::GetException()
{
    return GetStream<MinidumpException>();
}

MinidumpSystemInfo* MinidumpParser::GetSystemInfo()
{
    return GetStream<MinidumpSystemInfo>();
}

MinidumpMiscInfo* MinidumpParser::GetMiscInfo()
{
    return GetStream<MinidumpMiscInfo>();
}

MinidumpMapList* MinidumpParser::GetMapList()
{
    return GetStream<MinidumpMapList>();
}

MinidumpEsrInfo* MinidumpParser::GetEsrInfo()
{
    return GetStream<MinidumpEsrInfo>();
}

template<typename T>
T* MinidumpParser::GetStream()
{
    if (!isValid_) {
        return nullptr;
    }
    const uint32_t streamType = T::streamType;
    MinidumpStreamMap::iterator it = streamMap_->find(streamType);
    if (it == streamMap_->end()) {
        return nullptr;
    }
    MinidumpStreamInfo& info = it->second;
    if (!info.stream_) {
        uint32_t streamLength;
        if (!SeekToStreamType(streamType, streamLength)) {
            return nullptr;
        }
        auto minidumpStream = MinidumpStreamFactory::Instance().CreateStream(streamType, memoryReader_);
        if (minidumpStream == nullptr) {
            return nullptr;
        }
        minidumpStream->SetMinidumpSubject(minidumpSubject_);
        if (!minidumpStream->Read(streamLength)) {
            lastError_ = minidumpStream->GetLastError();
            auto& stats = MinidumpPerfMonitor::Instance().GetStats();
            stats.IncrementError();
            return nullptr;
        }
        info.stream_ = std::move(minidumpStream);
    }
    lastError_.Clear();
    return static_cast<T*>(info.stream_.get());
}

void MinidumpParser::Print()
{
    if (!isValid_) {
        DFXLOGE("MinidumpParser cannot print invalid data");
        return;
    }

    DFXLOGI("MDRawHeader");
    DFXLOGI("  signature            = 0x%{public}x", header_.signature);
    DFXLOGI("  version              = 0x%{public}x", header_.version);
    DFXLOGI("  streamCount          = %{public}u", header_.numberOfStreams);
    DFXLOGI("  streamDirectoryRva   = 0x%{public}x", header_.streamDirectoryRva);
    DFXLOGI("  checksum             = 0x%{public}x", header_.checksum);
    DFXLOGI("  timeStamp            = 0x%{public}x", header_.timeStamp);
    DFXLOGI("  flags                = 0x%{public}" PRIx64, header_.flags);
    DFXLOGI("\n");

    for (uint32_t i = 0; i < header_.numberOfStreams; ++i) {
        MDRawDirectory* directoryEntry = &(*directory_)[i];

        DFXLOGI("mDirectory[%{public}u]", i);
        DFXLOGI("MDRawDirectory");
        DFXLOGI("  streamType        = %{public}u", directoryEntry->streamType);
        DFXLOGI("  location.dataSize = %{public}u", directoryEntry->location.dataSize);
        DFXLOGI("  location.rva      = 0x%{public}x", directoryEntry->location.rva);
        DFXLOGI(" ");
    }
    DFXLOGI("\n");
}

bool MinidumpParser::SeekToStreamType(uint32_t streamType, uint32_t& streamLength)
{
    if (!isValid_) {
        return false;
    }

    const MDRawDirectory* directoryEntry = GetDirectoryEntryAtType(streamType);
    if (!directoryEntry) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_NOT_FOUND,
            "Stream type not found: " + std::to_string(streamType), __LINE__);
        DFXLOGE("SeekToStreamType: stream type %{public}u not found", streamType);
        return false;
    }

    if (!memoryReader_->SeekSet(directoryEntry->location.rva)) {
        lastError_ = MinidumpErrorInfo(MinidumpError::ERROR_FILE_SEEK, "Cannot seek to stream type", __LINE__);
        DFXLOGE("SeekToStreamType: could not seek to stream type %{public}u", streamType);
        return false;
    }

    streamLength = directoryEntry->location.dataSize;
    lastError_.Clear();
    return true;
}

const MDRawDirectory* MinidumpParser::GetDirectoryEntryAtType(uint32_t streamType) const
{
    if (!isValid_) {
        return nullptr;
    }

    MinidumpStreamMap::const_iterator it = streamMap_->find(streamType);
    if (it == streamMap_->end()) {
        return nullptr;
    }

    return GetDirectoryEntryAtIndex(it->second.streamIndex_);
}

const MDRawDirectory* MinidumpParser::GetDirectoryEntryAtIndex(uint32_t index) const
{
    if (!isValid_ || index >= header_.numberOfStreams) {
        return nullptr;
    }
    return &(*directory_)[index];
}

void MinidumpParser::SetPerformanceConfig(const PerformanceOptimizer::Config& config)
{
    PerformanceOptimizer::Instance().SetConfig(config);
}

void MinidumpParser::PrintPerformanceStats() const
{
    auto optStats = PerformanceOptimizer::Instance().GetStatistics();
    DFXLOGI("Optimizer Statistics:");
    DFXLOGI(" Memory interval tree size: %{public}zu", optStats.intervalTreeMemorySize);
    DFXLOGI(" Module interval tree size: %{public}zu", optStats.intervalTreeModuleSize);
    DFXLOGI(" Bitmap marked regions: %{public}zu", optStats.bitmapMarkedCount);
    DFXLOGI(" Memory range map size: %{public}zu", optStats.rangeMapMemorySize);
    DFXLOGI(" Module range map size: %{public}zu", optStats.rangeMapModuleSize);

    DFXLOGI("Perf Statistics:");
    DFXLOGI(" Total read success count: %{public}" PRIu64 "",
        MinidumpPerfMonitor::Instance().GetStats().successCount.load());
    DFXLOGI(" Total read error count: %{public}" PRIu64 "",
        MinidumpPerfMonitor::Instance().GetStats().errorCount.load());
    DFXLOGI(" Total read bytes: %{public}" PRIu64 "",
        MinidumpPerfMonitor::Instance().GetStats().totalReadBytes.load());
    DFXLOGI(" Total read count: %{public}" PRIu64 "",
        MinidumpPerfMonitor::Instance().GetStats().totalReadCount.load());
    DFXLOGI(" Total seek count: %{public}" PRIu64 "",
        MinidumpPerfMonitor::Instance().GetStats().totalSeekCount.load());
    DFXLOGI(" Total parse time Ms: %{public}" PRIu64 "",
        MinidumpPerfMonitor::Instance().GetStats().totalParseTimeMs.load());
}
}
}