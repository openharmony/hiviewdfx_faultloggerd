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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <securec.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
const std::string FAULTLOGGER_PIPE_TAG = "FaultLoggerPipe";
const int PIPE_TIMEOUT = 10000; // 10 seconds
}

FaultLoggerPipe::FaultLoggerPipe()
{
    init_ = false;
    write_ = false;
    Init();
}

FaultLoggerPipe::~FaultLoggerPipe()
{
    Destroy();
}

int FaultLoggerPipe::GetReadFd(void)
{
    LOGDEBUG("%{public}s :: pipe read fd: %{public}d", __func__, fds_[PIPE_READ]);
    return fds_[PIPE_READ];
}

int FaultLoggerPipe::GetWriteFd(void)
{
    LOGDEBUG("%{public}s :: pipe write fd: %{public}d", __func__, fds_[PIPE_WRITE]);
    if (!write_) {
        write_ = true;
        return fds_[PIPE_WRITE];
    }
    return -1;
}

bool FaultLoggerPipe::Init(void)
{
    if (!init_) {
        if (pipe2(fds_, O_NONBLOCK) != 0) {
            LOGERROR("%{public}s :: Failed to create pipe.", __func__);
            return false;
        }
        LOGDEBUG("%{public}s :: create pipe.", __func__);
    }
    init_ = true;
    if (!SetSize(MAX_PIPE_SIZE)) {
        LOGERROR("%{public}s :: Failed to set pipe size.", __func__);
    }
    return true;
}

bool FaultLoggerPipe::SetSize(long sz)
{
    if (!init_) {
        return false;
    }
    if (fcntl(fds_[PIPE_READ], F_SETPIPE_SZ, sz) < 0) {
        return false;
    }
    if (fcntl(fds_[PIPE_WRITE], F_SETPIPE_SZ, sz) < 0) {
        return false;
    }
    return true;
}

void FaultLoggerPipe::Destroy(void)
{
    if (init_) {
        LOGDEBUG("%{public}s :: close pipe.", __func__);
        Close(fds_[PIPE_READ]);
        Close(fds_[PIPE_WRITE]);
    }
    init_ = false;
}

void FaultLoggerPipe::Close(int fd) const
{
    if (fd > 0) {
        syscall(SYS_close, fd);
    }
}

FaultLoggerPipe2::FaultLoggerPipe2(uint64_t time, bool isJson)
{
    if (isJson) {
        faultLoggerJsonPipeBuf_ = std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe());
        faultLoggerJsonPipeRes_ = std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe());
    } else {
        faultLoggerPipeBuf_ = std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe());
        faultLoggerPipeRes_ = std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe());
    }
    time_ = time;
}

FaultLoggerPipe2::~FaultLoggerPipe2()
{
    faultLoggerPipeBuf_.reset();
    faultLoggerPipeRes_.reset();
    faultLoggerJsonPipeBuf_.reset();
    faultLoggerJsonPipeRes_.reset();
    time_ = 0;
}

FaultLoggerPipeMap::FaultLoggerPipeMap()
{
    std::lock_guard<std::mutex> lck(pipeMapsMutex_);
    faultLoggerPipes_.clear();
}

FaultLoggerPipeMap::~FaultLoggerPipeMap()
{
    std::lock_guard<std::mutex> lck(pipeMapsMutex_);
    std::map<int, std::unique_ptr<FaultLoggerPipe2> >::iterator iter = faultLoggerPipes_.begin();
    while (iter != faultLoggerPipes_.end()) {
        faultLoggerPipes_.erase(iter++);
    }
}

void FaultLoggerPipeMap::Set(int pid, uint64_t time, bool isJson)
{
    std::lock_guard<std::mutex> lck(pipeMapsMutex_);
    if (!Find(pid)) {
        std::unique_ptr<FaultLoggerPipe2> ptr = std::unique_ptr<FaultLoggerPipe2>(new FaultLoggerPipe2(time, isJson));
        faultLoggerPipes_.emplace(pid, std::move(ptr));
    }
}

bool FaultLoggerPipeMap::Check(int pid, uint64_t time)
{
    std::lock_guard<std::mutex> lck(pipeMapsMutex_);
    std::map<int, std::unique_ptr<FaultLoggerPipe2> >::const_iterator iter = faultLoggerPipes_.find(pid);
    if (iter != faultLoggerPipes_.end()) {
        if ((time > faultLoggerPipes_[pid]->time_) && (time - faultLoggerPipes_[pid]->time_) > PIPE_TIMEOUT) {
            faultLoggerPipes_.erase(iter);
            return false;
        }
        return true;
    }
    return false;
}

FaultLoggerPipe2* FaultLoggerPipeMap::Get(int pid)
{
    std::lock_guard<std::mutex> lck(pipeMapsMutex_);
    if (!Find(pid)) {
        return nullptr;
    }
    return faultLoggerPipes_[pid].get();
}

void FaultLoggerPipeMap::Del(int pid)
{
    std::lock_guard<std::mutex> lck(pipeMapsMutex_);
    std::map<int, std::unique_ptr<FaultLoggerPipe2> >::const_iterator iter = faultLoggerPipes_.find(pid);
    if (iter != faultLoggerPipes_.end()) {
        faultLoggerPipes_.erase(iter);
    }
}

bool FaultLoggerPipeMap::Find(int pid) const
{
    std::map<int, std::unique_ptr<FaultLoggerPipe2> >::const_iterator iter = faultLoggerPipes_.find(pid);
    if (iter != faultLoggerPipes_.end()) {
        return true;
    }
    return false;
}
} // namespace HiviewDfx
} // namespace OHOS
