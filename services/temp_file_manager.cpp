/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include "temp_file_manager.h"

#include <bitset>
#include <chrono>
#include <cmath>
#include <fcntl.h>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "dfx_trace.h"
#include "directory_ex.h"
#include "file_ex.h"

#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr const char *const TEMP_FILE_MANAGER_TAG = "TEMP_FILE_MANAGER";
constexpr uint64_t SECONDS_TO_MILLISECONDS = 1000;

const SingleFileConfig* GetTargetFileConfig(const std::function<bool(const SingleFileConfig&)>& filter)
{
    const auto& fileConfigs = FaultLoggerConfig::GetInstance().GetTempFileConfig().singleFileConfigs;
    auto iter = std::find_if(fileConfigs.begin(), fileConfigs.end(), filter);
    return iter == fileConfigs.end() ? nullptr : iter.base();
}

uint64_t GetCurrentTime()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

uint64_t GetFileSize(const std::string& filePath)
{
    struct stat64 statBuf{};
    if (stat64(filePath.c_str(), &statBuf) == 0) {
        return static_cast<uint64_t>(statBuf.st_size);
    }
    return 0;
}

uint64_t GetTimeFromFileName(const std::string& fileName)
{
    constexpr int timeStrLen = 13;
    constexpr char timeStrSplit = '-';
    constexpr int decimal = 10;
    auto pos = fileName.find_last_of(timeStrSplit);
    if (pos == std::string::npos) {
        return 0;
    }
    auto timeStr = fileName.substr(pos + 1, timeStrLen);
    errno = 0;
    uint64_t num = strtoull(timeStr.c_str(), nullptr, decimal);
    if (errno == ERANGE) {
        DFXLOGE("%{public}s :: invalid timeStr for file: %{public}s", TEMP_FILE_MANAGER_TAG, timeStr.c_str());
        return 0;
    }
    return num;
}

bool CreateFileDir(const std::string& filePath)
{
    if (access(filePath.c_str(), F_OK) == 0 || OHOS::ForceCreateDirectory(filePath)) {
        return true;
    }
    DFXLOGE("%{public}s :: failed to create dirs: %{public}s.", TEMP_FILE_MANAGER_TAG, filePath.c_str());
    return false;
}

bool RemoveTempFile(const std::string& filePath)
{
    if (access(filePath.c_str(), F_OK) != 0) {
        return true;
    }
    if (!OHOS::RemoveFile(filePath)) {
        DFXLOGE("%{public}s :: failed to remove file: %{public}s.", TEMP_FILE_MANAGER_TAG, filePath.c_str());
        return false;
    }
    DFXLOGI("%{public}s :: success to remove file: %{public}s.", TEMP_FILE_MANAGER_TAG, filePath.c_str());
    return true;
}

void RemoveTimeOutFileIfNeed(const SingleFileConfig& fileConfig, std::list<std::string>& tempFiles)
{
    if (fileConfig.fileExistTime < 0 || fileConfig.keepFileCount < 0 ||
        tempFiles.size() <= static_cast<size_t>(fileConfig.keepFileCount)) {
        return;
    }
    auto currentTime = GetCurrentTime();
    tempFiles.erase(std::remove_if(tempFiles.begin(), tempFiles.end(),
        [currentTime, &fileConfig](const std::string& tempFile) {
            auto fileCreateTime = GetTimeFromFileName(tempFile);
            if (fileCreateTime > currentTime ||
                (fileCreateTime + fileConfig.fileExistTime * SECONDS_TO_MILLISECONDS < currentTime)) {
                return RemoveTempFile(tempFile);
            }
            return false;
        }), tempFiles.end());
}

void ForceRemoveFileIfNeed(const SingleFileConfig& fileConfig, std::list<std::string>& tempFiles)
{
    if (fileConfig.maxFileCount < 0 || tempFiles.size() <= static_cast<size_t>(fileConfig.maxFileCount)) {
        return;
    }
    if (fileConfig.keepFileCount != 0) {
        tempFiles.sort([](std::string& lhs, const std::string& rhs) -> int {
            return GetTimeFromFileName(lhs) < GetTimeFromFileName(rhs);
        });
    }
    auto deleteNum = fileConfig.keepFileCount < 0 ? 1 : tempFiles.size() - fileConfig.keepFileCount;
    tempFiles.erase(std::remove_if(tempFiles.begin(), tempFiles.end(),
        [&deleteNum](const std::string& tempFile) {
            if (deleteNum > 0 && RemoveTempFile(tempFile)) {
                deleteNum--;
                return true;
            }
            return false;
        }), tempFiles.end());
}

/**
 * Check and handle the size of a temporary file with large file support.
 *
 * @param fileConfig Configuration parameters for the file:
 *        - maxSingleFileSize: Maximum allowed file size (0 indicates unlimited).
 *        - overFileSizeAction: Action to take when file size exceeds the limit.
 * @param filePath Path to the file to be checked.
 * @return The final size of the file after processing (in bytes):
 *         - Normal file: Original size.
 *         - Deleted file: 0.
 *         - Truncated file: the maxSingleFileSize of fileConfig.
 */
uint64_t CheckTempFileSize(const SingleFileConfig& fileConfig, const std::string& filePath)
{
    uint64_t originFileSize = GetFileSize(filePath);
    if (originFileSize > 0 &&
        (originFileSize <= fileConfig.maxSingleFileSize || fileConfig.maxSingleFileSize == 0)) {
        return originFileSize;
    }
    DFXLOGW("%{public}s :: invalid file size %{public}" PRIu64 " of: %{public}s.",
        TEMP_FILE_MANAGER_TAG, originFileSize, filePath.c_str());
    if (fileConfig.overFileSizeAction == OverFileSizeAction::DELETE || originFileSize == 0) {
        RemoveTempFile(filePath);
        return 0;
    }
    truncate64(filePath.c_str(), static_cast<off64_t>(fileConfig.maxSingleFileSize));
    return fileConfig.maxSingleFileSize;
}

uint64_t CheckTempFileSize(const SingleFileConfig& tempFileConfig, std::list<std::string>& tempFiles)
{
    uint64_t totalFileSize = 0;
    tempFiles.remove_if([&](const std::string& tempFile) {
        auto fileSize = CheckTempFileSize(tempFileConfig, tempFile);
        totalFileSize += fileSize;
        return fileSize == 0;
    });
    return totalFileSize;
}

uint64_t CheckTempFileSize(std::map<const SingleFileConfig*, std::list<std::string>>& tempFilesMap)
{
    uint64_t fileSize = 0;
    for (auto& pair : tempFilesMap) {
        if (pair.first == nullptr) {
            pair.second.remove_if(RemoveTempFile);
        } else {
            fileSize += CheckTempFileSize(*pair.first, pair.second);
        }
    }
    return fileSize;
}

void GetTempFiles(const SingleFileConfig& tempFileConfig, std::list<std::string>& tempFiles)
{
    std::vector<std::string> files;
    const auto& tempFilePath = FaultLoggerConfig::GetInstance().GetTempFileConfig().tempFilePath;
    OHOS::GetDirFiles(tempFilePath, files);
    for (const auto& file : files) {
        if (file.find(tempFileConfig.fileNamePrefix, tempFilePath.size()) != std::string::npos) {
            tempFiles.emplace_back(file);
        }
    }
}

void GetTempFiles(std::map<const SingleFileConfig*, std::list<std::string>>& tempFilesMap)
{
    std::vector<std::string> files;
    const auto& tempFilePath = FaultLoggerConfig::GetInstance().GetTempFileConfig().tempFilePath;
    OHOS::GetDirFiles(tempFilePath, files);
    for (const auto& file : files) {
        auto fileConfig = GetTargetFileConfig([&file, &tempFilePath](const SingleFileConfig& fileConfig) {
            return file.find(fileConfig.fileNamePrefix, tempFilePath.size()) != std::string::npos;
        });
        tempFilesMap[fileConfig].emplace_back(file);
    }
}
}

int32_t& TempFileManager::GetTargetFileCount(int32_t type)
{
    auto iter = std::find_if(fileCounts_.begin(), fileCounts_.end(),
        [type] (const std::pair<int32_t, int32_t>& pair) {
            return pair.first == type;
        });
    if (iter == fileCounts_.end()) {
        fileCounts_.emplace_back(type, 0);
        return fileCounts_.back().second;
    }
    return iter->second;
}

bool TempFileManager::InitTempFileWatcher()
{
    auto& config = FaultLoggerConfig::GetInstance().GetTempFileConfig();
    auto tempFileWatcher = TempFileWatcher::CreateInstance(*this);
    if (tempFileWatcher == nullptr) {
        return false;
    }
    constexpr uint32_t watchEvent = IN_CLOSE_WRITE | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF;
    if (!tempFileWatcher->AddWatchEvent(config.tempFilePath.c_str(), watchEvent)) {
        return false;
    }
    return EpollManager::GetInstance().AddListener(std::move(tempFileWatcher));
}

void TempFileManager::RestartDeleteTaskOnStart(int32_t existTimeInSecond, std::list<std::string>& files)
{
    uint32_t fileClearTimeInSecond = FaultLoggerConfig::GetInstance().GetTempFileConfig().fileClearTimeAfterBoot;
    uint64_t currentTime = GetCurrentTime();
    for (const auto& file : files) {
        uint64_t createTime = GetTimeFromFileName(file);
        int32_t existDuration = currentTime > createTime ?
            static_cast<int32_t>((currentTime - createTime) / SECONDS_TO_MILLISECONDS) : 0;
        int32_t existTimeLeft = existTimeInSecond > existDuration ? existTimeInSecond - existDuration : 0;
        int32_t fileClearTime = existTimeLeft > static_cast<int32_t>(fileClearTimeInSecond) ?
            existTimeLeft : static_cast<int32_t>(fileClearTimeInSecond);
        DelayTaskQueue::GetInstance().AddDelayTask(
            [file] {
                RemoveTempFile(file);
            }, static_cast<uint32_t>(fileClearTime));
    }
}

void TempFileManager::ScanTempFilesOnStart()
{
    std::map<const SingleFileConfig*, std::list<std::string>> tempFilesMap;
    GetTempFiles(tempFilesMap);
    uint64_t filesSize = CheckTempFileSize(tempFilesMap);
    bool isOverLimit = filesSize > FaultLoggerConfig::GetInstance().GetTempFileConfig().maxTempFilesSize;
    for (auto& pair : tempFilesMap) {
        if (!pair.first) {
            continue;
        }
        int32_t fileType = pair.first->type;
#ifdef FAULTLOGGERD_TEST
        constexpr uint64_t bigFileLimit = 5 * 1024;
#else
        constexpr uint64_t bigFileLimit = 100 * 1024 * 1024;
#endif
        if (isOverLimit && pair.first->maxSingleFileSize >= bigFileLimit) {
            std::for_each(pair.second.begin(), pair.second.end(), RemoveTempFile);
            pair.second.clear();
            GetTargetFileCount(fileType) = 0;
            continue;
        }
        ForceRemoveFileIfNeed(*pair.first, pair.second);
        GetTargetFileCount(fileType) = static_cast<int32_t>(pair.second.size());
        if (pair.first->overTimeFileDeleteType == OverTimeFileDeleteType::ACTIVE) {
            RestartDeleteTaskOnStart(pair.first->fileExistTime, pair.second);
        }
    }
}

bool TempFileManager::Init()
{
    auto& tempFilePath = FaultLoggerConfig::GetInstance().GetTempFileConfig().tempFilePath;
    if (tempFilePath.empty() || !CreateFileDir(tempFilePath)) {
        DFXLOGE("%{public}s :: invalid temp file path %{public}s", TEMP_FILE_MANAGER_TAG, tempFilePath.c_str());
        return false;
    }
    ScanTempFilesOnStart();
    return InitTempFileWatcher();
}

int32_t TempFileManager::CreateFileDescriptor(int32_t type, int32_t pid, int32_t tid, uint64_t time)
{
    const auto fileConfig = GetTargetFileConfig([type](const SingleFileConfig& fileConfig) {
        return fileConfig.type == type;
    });
    if (fileConfig == nullptr) {
        DFXLOGW("%{public}s :: failed to create fileDescriptor because of unknown file type for %{public}d",
            TEMP_FILE_MANAGER_TAG, type);
        return -1;
    }
    std::string ss = FaultLoggerConfig::GetInstance().GetTempFileConfig().tempFilePath + "/" +
        fileConfig->fileNamePrefix + "-" + std::to_string(pid);
    if (type == FaultLoggerType::JS_HEAP_SNAPSHOT || type == FaultLoggerType::JS_RAW_SNAPSHOT) {
        ss += "-" + std::to_string(tid);
    }
    ss += "-" + std::to_string(time == 0 ? std::chrono::duration_cast<std::chrono::milliseconds>\
(std::chrono::system_clock::now().time_since_epoch()).count() : time);
    if (type == FaultLoggerType::JS_RAW_SNAPSHOT) {
        ss += ".rawheap";
    }
    DFXLOGI("%{public}s :: create file for path(%{public}s).", TEMP_FILE_MANAGER_TAG, ss.c_str());
    int32_t fd = OHOS_TEMP_FAILURE_RETRY(open(ss.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP));
    if (fd < 0) {
        int openErrno = errno;
        const auto& dirPath = FaultLoggerConfig::GetInstance().GetTempFileConfig().tempFilePath;
        if (access(dirPath.c_str(), F_OK) != 0) {
            DFXLOGE("%{public}s :: Failed to create log file, errno(%{public}d). %{public}s does not exist!!!",
                    TEMP_FILE_MANAGER_TAG, openErrno, dirPath.c_str());
        } else {
            DFXLOGE("%{public}s :: Failed to create log file, errno(%{public}d)", TEMP_FILE_MANAGER_TAG, openErrno);
        }
    }
    return fd;
}

#ifndef is_ohos_lite
std::list<std::pair<int32_t, int64_t>> TempFileManager::crashFileRecords_{};

void TempFileManager::ClearTimeOutRecords()
{
#ifdef FAULTLOGGERD_TEST
    constexpr int validTime = 1;
#else
    constexpr int validTime = 8;
#endif
    auto currentTime = time(nullptr);
    crashFileRecords_.remove_if([currentTime](const std::pair<int32_t, int32_t>& pair) {
        return pair.second + validTime <= currentTime;
    });
}

bool TempFileManager::CheckCrashFileRecord(int32_t pid)
{
    DFX_TRACE_SCOPED("CheckCrashFileRecord");
    ClearTimeOutRecords();
    auto iter = std::find_if(crashFileRecords_.begin(), crashFileRecords_.end(),
        [pid](const auto& record) {
            return record.first == pid;
        });
    return iter != crashFileRecords_.end();
}

void TempFileManager::RecordFileCreation(int32_t type, int32_t pid)
{
    ClearTimeOutRecords();
    if (type != FaultLoggerType::CPP_CRASH) {
        return;
    }
    auto iter = std::find_if(crashFileRecords_.begin(), crashFileRecords_.end(),
        [pid](const auto& record) {
            return record.first == pid;
        });
    if (iter != crashFileRecords_.end()) {
        iter->second = time(nullptr);
    } else {
        crashFileRecords_.emplace_back(pid, time(nullptr));
    }
}
#endif

std::unique_ptr<TempFileManager::TempFileWatcher> TempFileManager::TempFileWatcher::CreateInstance(
    TempFileManager& tempFileManager)
{
    SmartFd watchFd{inotify_init()};
    if (!watchFd) {
        DFXLOGE("%{public}s :: failed to init inotify fd: %{public}d.", TEMP_FILE_MANAGER_TAG, watchFd.GetFd());
        return nullptr;
    }
    return std::unique_ptr<TempFileManager::TempFileWatcher>(new (std::nothrow)TempFileWatcher(tempFileManager,
        std::move(watchFd)));
}

TempFileManager::TempFileWatcher::TempFileWatcher(TempFileManager& tempFileManager, SmartFd fd)
    : EpollListener(std::move(fd), true), tempFileManager_(tempFileManager) {}

bool TempFileManager::TempFileWatcher::AddWatchEvent(const char* watchPath, uint32_t watchEvent)
{
    if (watchPath == nullptr) {
        return false;
    }
    if (inotify_add_watch(GetFd(), watchPath, watchEvent) < 0) {
        DFXLOGE("%{public}s :: failed to add watch for file: %{public}s.", TEMP_FILE_MANAGER_TAG, watchPath);
        return false;
    }
    return true;
}

void TempFileManager::TempFileWatcher::OnEventPoll()
{
    constexpr uint32_t eventLen = static_cast<uint32_t>(sizeof(inotify_event));
    constexpr uint32_t eventLenSize = 32;
    constexpr uint32_t buffLen = eventLenSize * eventLen;
    constexpr uint32_t bound = buffLen - eventLen;
    char eventBuf[buffLen] = {0};
    auto readLen = static_cast<size_t>(OHOS_TEMP_FAILURE_RETRY(read(GetFd(), eventBuf, sizeof(eventBuf))));
    size_t eventPos = 0;
    while (readLen >= eventLen && eventPos < bound) {
        auto *event = reinterpret_cast<inotify_event *>(eventBuf + eventPos);
#ifdef FAULTLOGGERD_TEST
        if (event->mask & tempFileManager_.eventMask_) {
            return;
        }
#endif
        if (event->mask & IN_DELETE_SELF) {
            HandleDirRemoved();
            return;
        }
        if (event->len > 0) {
            std::string fileName(event->name);
            auto fileConfig = GetTargetFileConfig([&fileName](const SingleFileConfig& fileConfig) {
                return fileName.find(fileConfig.fileNamePrefix) != std::string::npos;
            });
            std::string filePath = FaultLoggerConfig::GetInstance().GetTempFileConfig().tempFilePath + "/" + fileName;
            if (fileConfig == nullptr) {
                RemoveTempFile(filePath);
            } else {
                HandleEvent(event->mask, filePath, *fileConfig);
            }
        }
        auto eventSize = (eventLen + event->len);
        readLen -= eventSize;
        eventPos += eventSize;
    }
}

void TempFileManager::TempFileWatcher::HandleEvent(uint32_t eventMask, const std::string& filePath,
    const SingleFileConfig& fileConfig)
{
    if (eventMask & IN_CREATE) {
        HandleFileCreate(filePath, fileConfig);
    }
    if (eventMask & (IN_MOVED_FROM | IN_DELETE)) {
        HandleFileDeleteOrMove(filePath, fileConfig);
    }
    if (eventMask & IN_CLOSE_WRITE) {
        HandleFileWrite(filePath, fileConfig);
    }
    if (eventMask & IN_MOVED_TO) {
        HandleFileCreate(filePath, fileConfig);
        HandleFileWrite(filePath, fileConfig);
    }
}

void TempFileManager::TempFileWatcher::HandleFileCreate(const std::string& filePath, const SingleFileConfig& fileConfig)
{
    int32_t currentFileCount = ++(tempFileManager_.GetTargetFileCount(fileConfig.type));
    DFXLOGD("%{public}s :: file %{public}s is created, currentFileCount: %{public}d, keepFileCount: %{public}d, "
            "maxFileCount: %{public}d, existTime %{public}d, overTimeDeleteType %{public}d",
            TEMP_FILE_MANAGER_TAG, filePath.c_str(), currentFileCount, fileConfig.keepFileCount,
            fileConfig.maxFileCount, fileConfig.fileExistTime, fileConfig.overTimeFileDeleteType);
    if (fileConfig.overTimeFileDeleteType == OverTimeFileDeleteType::ACTIVE) {
        DelayTaskQueue::GetInstance().AddDelayTask(
            [filePath] {
                RemoveTempFile(filePath);
            }, fileConfig.fileExistTime);
    }
    if ((fileConfig.keepFileCount >= 0 && currentFileCount > fileConfig.keepFileCount) ||
        (fileConfig.maxFileCount >= 0 && currentFileCount > fileConfig.maxFileCount)) {
        std::list<std::string> files;
        GetTempFiles(fileConfig, files);
        RemoveTimeOutFileIfNeed(fileConfig, files);
        ForceRemoveFileIfNeed(fileConfig, files);
    }
}

void TempFileManager::TempFileWatcher::HandleFileDeleteOrMove(const std::string& filePath,
                                                              const SingleFileConfig& fileConfig)
{
    int32_t& currentFileCount = tempFileManager_.GetTargetFileCount(fileConfig.type);
    if (currentFileCount > 0) {
        currentFileCount--;
    }
    DFXLOGD("%{public}s :: file %{public}s is deleted or moved, currentFileCount: %{public}d",
            TEMP_FILE_MANAGER_TAG, filePath.c_str(), currentFileCount);
}

void TempFileManager::TempFileWatcher::HandleFileWrite(const std::string& filePath, const SingleFileConfig& fileConfig)
{
    if (access(filePath.c_str(), F_OK) == 0) {
        CheckTempFileSize(fileConfig, filePath);
    }
}

void TempFileManager::TempFileWatcher::HandleDirRemoved()
{
    std::string summary = "The temp file directory: " +
        FaultLoggerConfig::GetInstance().GetTempFileConfig().tempFilePath + " was removed unexpectedly";
    DFXLOGE("%{public}s :: %{public}s", TEMP_FILE_MANAGER_TAG, summary.c_str());
#ifndef HISYSEVENT_DISABLE
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::RELIABILITY, "CPP_CRASH_NO_LOG",
        OHOS::HiviewDFX::HiSysEvent::EventType::FAULT,
        "HAPPEN_TIME", timestamp,
        "SUMMARY", summary);
#endif
    EpollManager::GetInstance().RemoveListener(GetFd());
}
}
}
