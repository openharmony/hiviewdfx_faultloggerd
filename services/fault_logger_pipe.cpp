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

#include "fault_logger_pipe.h"

#include <algorithm>
#include <cerrno>
#include <ctime>
#include <string>

#include <fcntl.h>
#include <securec.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "epoll_manager.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr size_t MAX_LITE_PERF_PIPE_SIZE = 100;
}

FaultLoggerPipe::FaultLoggerPipe()
{
    int fds[PIPE_NUM_SZ] = {-1, -1};
    if (pipe2(fds, O_NONBLOCK) != 0) {
        DFXLOGE("%{public}s :: Failed to create pipe, errno: %{public}d.", __func__, errno);
        return;
    }
    DFXLOGD("%{public}s :: create pipe.", __func__);
    readFd_ = SmartFd{fds[PIPE_READ]};
    writeFd_ = SmartFd{fds[PIPE_WRITE]};

    if (fcntl(readFd_.GetFd(), F_SETPIPE_SZ, MAX_PIPE_SIZE) < 0 ||
        fcntl(writeFd_.GetFd(), F_SETPIPE_SZ, MAX_PIPE_SIZE) < 0) {
        DFXLOGE("%{public}s :: Failed to set pipe size, errno: %{public}d.", __func__, errno);
    }
}

int FaultLoggerPipe::GetReadFd() const
{
    DFXLOGD("%{public}s :: pipe read fd: %{public}d", __func__, readFd_.GetFd());
    return readFd_.GetFd();
}

int FaultLoggerPipe::GetWriteFd()
{
    DFXLOGD("%{public}s :: pipe write fd: %{public}d", __func__, writeFd_.GetFd());
    if (!write_) {
        write_ = true;
        return writeFd_.GetFd();
    }
    return -1;
}

std::list<FaultLoggerPipePair> FaultLoggerPipePair::sdkDumpPipes_{};
std::list<LitePerfPipePair> LitePerfPipePair::pipes_{};
std::list<LimitedPipePair> LimitedPipePair::pipes_{};

FaultLoggerPipePair::FaultLoggerPipePair(int32_t pid, uint64_t requestTime) : pid_(pid), requestTime_(requestTime) {}

bool FaultLoggerPipePair::IsValid(uint64_t checkTime) const
{
    constexpr int pipeTimeOut = 10000; // 10s
    return checkTime <= requestTime_ + pipeTimeOut;
}

int32_t FaultLoggerPipePair::GetPipeFd(PipeFdUsage usage, FaultLoggerPipeType pipeType)
{
    FaultLoggerPipe& targetPipe = (usage == PipeFdUsage::BUFFER_FD) ? faultLoggerPipeBuf_ : faultLoggerPipeRes_;
    if (pipeType == FaultLoggerPipeType::PIPE_FD_READ) {
        return targetPipe.GetReadFd();
    }
    if (pipeType == FaultLoggerPipeType::PIPE_FD_WRITE) {
        return targetPipe.GetWriteFd();
    }
    return -1;
}

FaultLoggerPipePair& FaultLoggerPipePair::CreateSdkDumpPipePair(int pid, uint64_t requestTime)
{
    auto pipePair = GetSdkDumpPipePair(pid);
    if (pipePair != nullptr) {
        return *pipePair;
    }
    return sdkDumpPipes_.emplace_back(pid, requestTime);
}

bool FaultLoggerPipePair::CheckSdkDumpRecord(int pid, uint64_t checkTime)
{
    auto iter = std::find_if(sdkDumpPipes_.begin(), sdkDumpPipes_.end(),
        [pid](const FaultLoggerPipePair& faultLoggerPipePair) {
            return faultLoggerPipePair.pid_ == pid;
        });
    if (iter != sdkDumpPipes_.end()) {
        if (iter->IsValid(checkTime)) {
            return true;
        }
        sdkDumpPipes_.erase(iter);
    }
    return false;
}

FaultLoggerPipePair* FaultLoggerPipePair::GetSdkDumpPipePair(int pid)
{
    auto iter = std::find_if(sdkDumpPipes_.begin(), sdkDumpPipes_.end(),
        [pid](const FaultLoggerPipePair& faultLoggerPipePair) {
            return faultLoggerPipePair.pid_ == pid;
        });
    return iter == sdkDumpPipes_.end() ? nullptr : &(*iter);
}

void FaultLoggerPipePair::DelSdkDumpPipePair(int pid)
{
    sdkDumpPipes_.remove_if([pid](const FaultLoggerPipePair& faultLoggerPipePair) {
        return faultLoggerPipePair.pid_ == pid;
    });
}

LitePerfPipePair::LitePerfPipePair(int32_t uid, uint64_t timeOutTime) : uid_(uid), timeOutTime_(timeOutTime) {}

int32_t LitePerfPipePair::GetPipeFd(PipeFdUsage usage, FaultLoggerPipeType pipeType)
{
    FaultLoggerPipe& targetPipe = (usage == PipeFdUsage::BUFFER_FD) ? faultLoggerPipeBuf_ : faultLoggerPipeRes_;
    if (pipeType == FaultLoggerPipeType::PIPE_FD_READ) {
        return targetPipe.GetReadFd();
    }
    if (pipeType == FaultLoggerPipeType::PIPE_FD_WRITE) {
        return targetPipe.GetWriteFd();
    }
    return -1;
}

LitePerfPipePair& LitePerfPipePair::CreatePipePair(int uid, uint64_t timeOutTime)
{
    auto pipePair = GetPipePair(uid);
    if (pipePair != nullptr) {
        return *pipePair;
    }
    return pipes_.emplace_back(uid, timeOutTime);
}

bool LitePerfPipePair::CheckDumpMax()
{
    return pipes_.size() > MAX_LITE_PERF_PIPE_SIZE;
}

bool LitePerfPipePair::CheckDumpRecord(int uid)
{
    auto iter = std::find_if(pipes_.begin(), pipes_.end(),
        [uid](const LitePerfPipePair& pipePair) {
            return pipePair.uid_ == uid;
        });
    return iter != pipes_.end();
}

LitePerfPipePair* LitePerfPipePair::GetPipePair(int uid)
{
    auto iter = std::find_if(pipes_.begin(), pipes_.end(),
        [uid](const LitePerfPipePair& pipePair) {
            return pipePair.uid_ == uid;
        });
    return iter == pipes_.end() ? nullptr : &(*iter);
}

void LitePerfPipePair::DelPipePair(int uid)
{
    pipes_.remove_if([uid](const LitePerfPipePair& pipePair) {
        return pipePair.uid_ == uid;
    });
}

void LitePerfPipePair::ClearTimeOutPairs()
{
    auto currentTime = GetMicroSecondsSinceBoot();
    pipes_.remove_if([currentTime](const LitePerfPipePair& pipePair) {
        return pipePair.timeOutTime_ <= currentTime;
    });
}

LimitedPipePair::LimitedPipePair(int32_t uid) : uid_(uid) {}

int32_t LimitedPipePair::GetPipeFd(FaultLoggerPipeType pipeType)
{
    if (pipeType == FaultLoggerPipeType::PIPE_FD_READ) {
        return faultLoggerPipeBuf_.GetReadFd();
    }
    if (pipeType == FaultLoggerPipeType::PIPE_FD_WRITE) {
        return faultLoggerPipeBuf_.GetWriteFd();
    }
    return -1;
}

LimitedPipePair& LimitedPipePair::CreatePipePair(int uid)
{
    auto pipePair = GetPipePair(uid);
    if (pipePair != nullptr) {
        return *pipePair;
    }
    return pipes_.emplace_back(uid);
}

bool LimitedPipePair::CheckDumpRecord(int uid)
{
    auto iter = std::find_if(pipes_.begin(), pipes_.end(),
        [uid](const LimitedPipePair& pipePair) {
            return pipePair.uid_ == uid;
        });
    return iter != pipes_.end();
}

LimitedPipePair* LimitedPipePair::GetPipePair(int uid)
{
    auto iter = std::find_if(pipes_.begin(), pipes_.end(),
        [uid](const LimitedPipePair& pipePair) {
            return pipePair.uid_ == uid;
        });
    return iter == pipes_.end() ? nullptr : &(*iter);
}

void LimitedPipePair::DelPipePair(int uid)
{
    pipes_.remove_if([uid](const LimitedPipePair& pipePair) {
        return pipePair.uid_ == uid;
    });
}
} // namespace HiviewDfx
} // namespace OHOS
