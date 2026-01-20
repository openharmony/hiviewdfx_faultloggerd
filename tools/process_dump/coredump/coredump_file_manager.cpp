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
#include "coredump_file_manager.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "coredump_config_manager.h"
#include "coredump_controller.h"
#include "coredump_mapping_manager.h"
#include "dfx_log.h"
#include "dump_utils.h"
#include "faultloggerd_client.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
char g_coredumpFilePath[256] = {0}; // 256 : coredump file path

void HandleSigterm(int sig)
{
    unlink(g_coredumpFilePath);
    _exit(0);
}

int UnBlockSIGTERM()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, DUMPCATCHER_TIMEOUT);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    return 0;
}
}

void CoredumpFileManager::Init(int32_t targetPid, uint32_t uid)
{
    targetPid_ = targetPid;
    uid_ = uid;
}

void CoredumpFileManager::WriteNativeCoredump()
{
    if (!isWriteNativeCoredump_) {
        DFXLOGI("is write coredump lite is false");
        return;
    }
    if (write(fd_, mappedMemory_, coreFileSize_) < 0) {
        DFXLOGE("write coredump lite fail, errno:%{public}d", errno);
    } else {
        DFXLOGI("write coredump lite succ");
    }
}

CoredumpFileManager::~CoredumpFileManager()
{
    if (mappedMemory_ != nullptr) {
        munmap(static_cast<void*>(mappedMemory_), coreFileSize_);
        mappedMemory_ = nullptr;
    }
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

bool CoredumpFileManager::CreateFile()
{
    if (targetPid_ <= 0) {
        DFXLOGE("the targetPid is invalid!");
        return false;
    }

    auto& mappingManager = CoredumpMappingManager::GetInstance();
    mappingManager.Parse(targetPid_);

    if (coreFileSize_ = mappingManager.EstimateFileSize(); coreFileSize_ == 0) {
        DFXLOGE("corefile size is 0");
        return false;
    } else {
        DFXLOGE("Estimate corefile size is %{public}lu", coreFileSize_);
    }

    if (!CreateFileForCoreDump()) {
        DFXLOGE("create file fail");
        return false;
    }
    RegisterCancelCoredump(GetCoredumpFilePath());
    return true;
}

bool CoredumpFileManager::MmapForFd()
{
    if (fd_ <= 0) {
        DFXLOGE("The fd is invalid, not to mmap");
        return false;
    }
    if (coreFileSize_ == 0) {
        DFXLOGE("The coreFileSize is 0, not to mmap");
        return false;
    }
    if (!AdjustFileSize(coreFileSize_)) {
        return false;
    }
    if (CoredumpMappingManager::isNativeProcess_) {
        mappedMemory_ = static_cast<char *>(mmap(nullptr, coreFileSize_, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        isWriteNativeCoredump_ = true;
    } else {
        mappedMemory_ = static_cast<char *>(mmap(nullptr, coreFileSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
    }
    if (mappedMemory_ == MAP_FAILED) {
        DFXLOGE("mmap fail %{public}d", errno);
        mappedMemory_ = nullptr;
        return false;
    }
    return true;
}

bool CoredumpFileManager::CreateFileForCoreDump()
{
    constexpr long minUid = 10000; // 10000 : minimum uid for hap
    if (uid_ < minUid) {
        if (CoredumpConfigManager::GetInstance().GetConfig().coredumpSwitch) {
            bundleName_ = "native";
            fd_ = RequestFileDescriptor(COREDUMP_LITE);
            CoredumpMappingManager::isNativeProcess_ = true;
        }
    } else {
        bundleName_ = DumpUtils::GetSelfBundleName();
        if (bundleName_.empty()) {
            DFXLOGE("query bundleName fail");
            return false;
        }
        if (!CoredumpController::VerifyProcess()) {
            return false;
        }
        std::string filePath = GetCoredumpFilePath();
        fd_ = OHOS_TEMP_FAILURE_RETRY(open(filePath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR));
    }
    if (fd_ == INVALID_FD) {
        DFXLOGE("create coredump file fail, errno = %{public}d", errno);
        return false;
    }

    DFXLOGI("create corefile succ, curUid:%{public}d", getuid());
    return true;
}

bool CoredumpFileManager::AdjustFileSize(uint64_t fileSize)
{
    if (fd_ == -1) {
        DFXLOGE("fd is invalid, not to adjust file size");
        return false;
    }
    if (fileSize == 0) {
        DFXLOGE("filesize is 0, not to adjust file size");
        return false;
    }
    if (ftruncate(fd_, fileSize) == -1) {
        DFXLOGE("ftruncate fail, errno:%{public}d", errno);
        return false;
    }
    return true;
}

bool CoredumpFileManager::UnlinkFile(const std::string &logPath)
{
    if (logPath.empty()) {
        return false;
    }
    if (unlink(logPath.c_str()) != 0) {
        DFXLOGE("unlink file(%{public}s) fail, errno:%{public}d", logPath.c_str(), errno);
        return false;
    }
    DFXLOGI("unlink file(%{public}s) success", logPath.c_str());
    return true;
}

std::string CoredumpFileManager::GetCoredumpFileName()
{
    if (bundleName_.empty()) {
        return "";
    }
    return bundleName_ + ".dmp";
}

std::string CoredumpFileManager::GetCoredumpFilePath()
{
    if (bundleName_.empty()) {
        return "";
    }
    auto configManager = CoredumpConfigManager::GetInstance();
    return configManager.GetConfig().outputPath + "/" + GetCoredumpFileName();
}

bool CoredumpFileManager::RegisterCancelCoredump(const std::string& dumpFilePath)
{
    auto ret = strncpy_s(g_coredumpFilePath, sizeof(g_coredumpFilePath), dumpFilePath.c_str(), dumpFilePath.length());
    if (ret != 0) {
        DFXLOGE("strncpy_s fail, err:%{public}d", ret);
        return false;
    }

    UnBlockSIGTERM();
    if (signal(DUMPCATCHER_TIMEOUT, HandleSigterm) == SIG_ERR) {
        DFXLOGE("Failed to register handler for DUMPCATCHER_TIMEOUT");
        return false;
    }
    return true;
}
}
}
