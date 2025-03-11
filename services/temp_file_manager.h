/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef TEMP_FILE_MANAGER_H_
#define TEMP_FILE_MANAGER_H_

#include <filesystem>
#include <list>
#include <string>
#include <utility>

#include "epoll_manager.h"
#include "fault_logger_config.h"

namespace OHOS {
namespace HiviewDFX {

class TempFileManager {
public:
    explicit TempFileManager(EpollManager& epollManager);

    bool Init();

    static int32_t CreateFileDescriptor(int32_t type, int32_t pid, int32_t tid, uint64_t time);
#ifndef is_ohos_lite
    static bool CheckCrashFileRecord(int32_t pid);
    static void RecordFileCreation(int32_t type, int32_t pid);
#endif

private:
    class TempFileWatcher : public EpollListener {
    public:
        static std::unique_ptr<TempFileWatcher> CreateInstance(TempFileManager& tempFileManager);
        bool AddWatchEvent(const char* watchPath, uint32_t watchEvent);
        void OnEventPoll() override;
    private:
        TempFileWatcher(TempFileManager& tempFileManager, int32_t fd);
        void HandleEvent(uint32_t eventMask, const std::string& filePath, const SingleFileConfig& fileConfig);
        void HandleFileCreate(const std::string& filePath, const SingleFileConfig& fileConfig);
        void HandleFileDeleteOrMove(const std::string& filePath, const SingleFileConfig& fileConfig);
        void HandleDirRemoved();
        static void HandleFileWrite(const std::string& filePath, const SingleFileConfig& fileConfig);
        TempFileManager &tempFileManager_;
    };
    bool InitTempFileWatcher();
    void ScanTempFilesOnStart();
    void ClearBigFilesOnStart(bool isSizeOverLimit, std::list<std::string>& files);

#ifndef is_ohos_lite
    static void ClearTimeOutRecords();
#endif

    EpollManager& epollManager_;
    int32_t& GetTargetFileCount(int32_t type);
    std::vector<std::pair<int32_t, int32_t>> fileCounts_;
#ifndef is_ohos_lite
    static std::list<std::pair<int32_t, int64_t>> crashFileRecords_;
#endif
};
}
}
#endif // TEMP_FILE_MANAGER_H_
