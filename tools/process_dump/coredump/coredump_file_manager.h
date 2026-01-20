/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#ifndef COREDUMP_FILE_MANAGER_H
#define COREDUMP_FILE_MANAGER_H
#include <string>

#include "coredump_common.h"

namespace OHOS {
namespace HiviewDFX {

class CoredumpFileManager {
public:
    CoredumpFileManager() = default;
    ~CoredumpFileManager();
    bool CreateFile();
    bool MmapForFd();
    bool CreateFileForCoreDump();
    bool AdjustFileSize(uint64_t fileSize);
    static bool UnlinkFile(const std::string &logPath);
    std::string GetCoredumpFileName();
    std::string GetCoredumpFilePath();
    void Init(int32_t targetPid, uint32_t uid);
    char* GetMappedMemory() const { return mappedMemory_; }
    uint64_t GetCoreFileSize() const { return coreFileSize_; }
    bool RegisterCancelCoredump(const std::string& dumpFilePath);
    void WriteNativeCoredump();
    CoredumpFileManager(const CoredumpFileManager&) = delete;
    CoredumpFileManager& operator=(const CoredumpFileManager&) = delete;
private:
    int32_t targetPid_ {0};
    uint32_t uid_ {0};
    int fd_ {-1};
    uint64_t coreFileSize_ {0};
    std::string bundleName_;
    char *mappedMemory_ {nullptr};
    bool isWriteNativeCoredump_ {false};
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
