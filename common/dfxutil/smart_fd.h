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

#ifndef SMART_FD_H
#define SMART_FD_H

#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include "dfx_log_define.h"

namespace OHOS {
namespace HiviewDFX {
class SmartFd {
public:
    SmartFd() = default;
    explicit SmartFd(int fd, bool fdsan = true) : fd_(fd), fdsan_(fdsan)
    {
        if (fd_ >= 0 && fdsan_) {
            fdsan_exchange_owner_tag(fd_, 0, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, LOG_DOMAIN));
        }
    }

    ~SmartFd()
    {
        Reset();
    }

    SmartFd(const SmartFd&) = delete;

    SmartFd &operator=(const SmartFd&) = delete;

    SmartFd(SmartFd&& rhs) noexcept : fd_(rhs.fd_), fdsan_(rhs.fdsan_)
    {
        // reset
        rhs.fd_ = -1;
        rhs.fdsan_ = false;
    }

    SmartFd& operator=(SmartFd&& rhs) noexcept
    {
        if (this != &rhs) {
            CloseFd();
            fd_ = rhs.fd_;
            fdsan_ = rhs.fdsan_;
            // reset
            rhs.fd_ = -1;
            rhs.fdsan_ = false;
        }
        return *this;
    }

    explicit operator bool() const
    {
        return fd_ >= 0;
    }

    int GetFd() const
    {
        return fd_;
    }

    void Reset()
    {
        CloseFd();
        fd_ = -1;
        fdsan_ = false;
    }

private:
    void CloseFd() const
    {
        if (fd_ < 0) {
            return;
        }
        if (fdsan_) {
            fdsan_close_with_tag(fd_, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, LOG_DOMAIN));
            return;
        }
        close(fd_);
    }

    int fd_{-1};
    bool fdsan_{false};
};
}
}
#endif // SMART_FD_H
