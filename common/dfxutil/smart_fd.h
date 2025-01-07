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
#include <unistd.h>
namespace OHOS {
namespace HiviewDFX {
class SmartFd {
public:
    SmartFd(int32_t fd) : fd_(fd) {}

    ~SmartFd()
    {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    SmartFd(const SmartFd&) = delete;

    SmartFd &operator=(const SmartFd&) = delete;

    SmartFd(SmartFd&& rhs) noexcept : fd_(rhs.fd_)
    {
        rhs.fd_ = -1;
    }

    SmartFd& operator=(SmartFd&& rhs) noexcept
    {
        if (this != &rhs) {
            if (fd_ >= 0) {
                close(fd_);
            }
            fd_ = rhs.fd_;
            rhs.fd_ = -1;
        }
        return *this;
    }

    operator int32_t() const
    {
        return fd_;
    }
private:
    int32_t fd_;
};
}
}
#endif // SMART_FD_H
