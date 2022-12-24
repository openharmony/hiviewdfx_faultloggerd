/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains faultlog secure module. */

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
static const std::string FAULTLOGGER_PIPE_TAG = "FaultLoggerPipe";

static const int PIPE_READ = 0;
static const int PIPE_WRITE = 1;
}

FaultLoggerPipe::FaultLoggerPipe()
{
    bInit_ = false;
    Init();
}

FaultLoggerPipe::~FaultLoggerPipe()
{
    Destroy();
}

int FaultLoggerPipe::GetReadFd()
{
    DfxLogDebug("%s :: pipe read fd: %d", __func__, pfds_[PIPE_READ]);
    return pfds_[PIPE_READ];
}

int FaultLoggerPipe::GetWriteFd()
{
    DfxLogDebug("%s :: pipe write fd: %d", __func__, pfds_[PIPE_WRITE]);
    return pfds_[PIPE_WRITE];
}

bool FaultLoggerPipe::Init()
{
    if (!bInit_) {
        if (pipe(pfds_) != 0) {
            DfxLogError("%s :: Failed to create pipe.", __func__);
            return false;
        }
        DfxLogDebug("%s :: create pipe.", __func__);
    }
    bInit_ = true;
    return true;
}

void FaultLoggerPipe::Destroy()
{
    if (bInit_) {
        DfxLogDebug("%s :: close pipe.", __func__);
        Close(pfds_[PIPE_READ]);
        Close(pfds_[PIPE_WRITE]);
    }
    bInit_ = false;
}

void FaultLoggerPipe::Close(int fd) const
{
    syscall(SYS_close, fd);
}

FaultLoggerPipe2::FaultLoggerPipe2(std::unique_ptr<FaultLoggerPipe> pipeBuf, std::unique_ptr<FaultLoggerPipe> pipeRes)
    : faultLoggerPipeBuf_(std::move(pipeBuf)), faultLoggerPipeRes_(std::move(pipeRes))
{
}

FaultLoggerPipe2::FaultLoggerPipe2(): FaultLoggerPipe2(std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe()),
    std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe()))
{
}

FaultLoggerPipe2::~FaultLoggerPipe2()
{
    faultLoggerPipeBuf_.reset();
    faultLoggerPipeRes_.reset();
}

FaultLoggerPipeMap::FaultLoggerPipeMap()
{
    faultLoggerPipes_.clear();
}

FaultLoggerPipeMap::~FaultLoggerPipeMap()
{
    std::map<int, std::unique_ptr<FaultLoggerPipe2> >::iterator iter = faultLoggerPipes_.begin();
    while (iter != faultLoggerPipes_.end()) {
        faultLoggerPipes_.erase(iter++);
    }
}

void FaultLoggerPipeMap::Set(int pid)
{
    if (!Find(pid)) {
        std::unique_ptr<FaultLoggerPipe2> ptr = std::unique_ptr<FaultLoggerPipe2>(new FaultLoggerPipe2());
        faultLoggerPipes_.insert(make_pair(pid, std::move(ptr)));
    }
}

FaultLoggerPipe2* FaultLoggerPipeMap::Get(int pid)
{
    if (!Find(pid)) {
        return nullptr;
    }
    return faultLoggerPipes_[pid].get();
}

void FaultLoggerPipeMap::Del(int pid)
{
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
