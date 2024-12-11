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

#ifndef DFX_FAULTLOGGERD_SOCKET_H
#define DFX_FAULTLOGGERD_SOCKET_H

#include <cinttypes>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif
const char* const FAULTLOGGERD_SOCK_BASE_PATH = "/dev/unix/socket/";

#ifdef FAULTLOGGERD_TEST
const char* const SERVER_SOCKET_NAME = "test.faultloggerd.server";
const char* const SERVER_CRASH_SOCKET_NAME = "test.faultloggerd.crash.server";
const char* const SERVER_SDKDUMP_SOCKET_NAME = "test.faultloggerd.sdkdump.server";
#else
const char* const SERVER_SOCKET_NAME = "faultloggerd.server";
const char* const SERVER_CRASH_SOCKET_NAME = "faultloggerd.crash.server";
const char* const SERVER_SDKDUMP_SOCKET_NAME = "faultloggerd.sdkdump.server";
#endif

bool StartListen(int& sockfd, const char* name, const int listenCnt);
int32_t GetConnectSocketFd(const char* socketName, const int timeout);
bool SendFileDescriptorToSocket(int sockfd, int fd);
int ReadFileDescriptorFromSocket(int sockfd);

bool SendMsgToSocket(int sockfd, const void* data, const unsigned int dataLength);
bool GetMsgFromSocket(int sockfd, void* data, const unsigned int dataLength);
#ifdef __cplusplus
}
class SmartFd {
public:
    SmartFd(int32_t fd) : fd_(fd) {};
    ~SmartFd();
    SmartFd(const SmartFd& smartSocket) = delete;
    SmartFd(SmartFd&& smartSocket) noexcept ;
    const SmartFd &operator=(const SmartFd &) = delete;
    SmartFd& operator=(SmartFd &&rhs) noexcept ;
    operator int32_t() const;
private:
    int32_t fd_;
};
#endif
#endif
