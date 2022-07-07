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
#include <string>
#include <vector>

#include <fcntl.h>
#include <securec.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static const std::string FaultLoggerPipe_TAG = "FaultLoggerPipe";

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

void FaultLoggerPipe::Close(int fd)
{
    syscall(SYS_close, fd);
}

FaultLoggerPipe2::FaultLoggerPipe2()
{
    faultLoggerPipeBuf_ = std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe());
    faultLoggerPipeRes_ = std::unique_ptr<FaultLoggerPipe>(new FaultLoggerPipe());
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
    std::map<int, std::shared_ptr<FaultLoggerPipe2> >::iterator iter = faultLoggerPipes_.begin();
    while (iter != faultLoggerPipes_.end()) {
        faultLoggerPipes_.erase(iter++);
    }
}

void FaultLoggerPipeMap::Set(int pid, std::shared_ptr<FaultLoggerPipe2> faultLoggerPipe)
{
    if (!Find(pid)) {
        DfxLogDebug("%s :: Set ptr(%p)", __func__, faultLoggerPipe.get());
        faultLoggerPipes_.insert(make_pair(pid, faultLoggerPipe));
    }
}

FaultLoggerPipe2* FaultLoggerPipeMap::Get(int pid)
{
    if (!Find(pid)) {
        return nullptr;
    }
    std::shared_ptr<FaultLoggerPipe2> faultLoggerPipe = faultLoggerPipes_[pid];
    return faultLoggerPipe.get();
}

void FaultLoggerPipeMap::Del(int pid)
{
    std::map<int, std::shared_ptr<FaultLoggerPipe2> >::iterator iter = faultLoggerPipes_.find(pid);
    if (iter != faultLoggerPipes_.end()) {
        faultLoggerPipes_.erase(iter);
    }
}

bool FaultLoggerPipeMap::Find(int pid)
{
    std::map<int, std::shared_ptr<FaultLoggerPipe2> >::iterator iter = faultLoggerPipes_.find(pid);
    if (iter != faultLoggerPipes_.end()) {
        return true;
    }
    return false;
}
} // namespace HiviewDfx
} // namespace OHOS
