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
    DFXLOGD("%{public}s :: pipe read fd: %{public}d", __func__, fds_[PIPE_READ]);
    return fds_[PIPE_READ];
}

int FaultLoggerPipe::GetWriteFd(void)
{
    DFXLOGD("%{public}s :: pipe write fd: %{public}d", __func__, fds_[PIPE_WRITE]);
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
            DFXLOGE("%{public}s :: Failed to create pipe.", __func__);
            return false;
        }
        DFXLOGD("%{public}s :: create pipe.", __func__);
    }
    init_ = true;
    if (!SetSize(MAX_PIPE_SIZE)) {
        DFXLOGE("%{public}s :: Failed to set pipe size.", __func__);
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
        DFXLOGD("%{public}s :: close pipe.", __func__);
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

FaultLoggerPipePair::FaultLoggerPipePair(uint64_t time, bool isJson) : isJson_(isJson), time_(time) {}

int FaultLoggerPipePair::GetPipFd(bool isWritePip, bool isResPip, bool isJson)
{
    if (isJson_ != isJson) {
        return -1;
    }
    FaultLoggerPipe& targetPip = isResPip ? faultLoggerPipeRes_ : faultLoggerPipeBuf_;
    return isWritePip ? targetPip.GetWriteFd() : targetPip.GetReadFd();
}
} // namespace HiviewDfx
} // namespace OHOS
