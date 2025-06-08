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

#include <cstdint>
#include "smart_fd.h"
namespace OHOS {
namespace HiviewDFX {
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

SmartFd StartListen(const char* name, uint32_t listenCnt);
bool SendFileDescriptorToSocket(int32_t sockFd, const int32_t* fds, uint32_t nFds);
bool SendMsgToSocket(int32_t sockFd, const void* data, uint32_t dataLength);
bool GetMsgFromSocket(int32_t sockFd, void* data, uint32_t dataLength);

typedef struct SocketRequestData {
    /** request data **/
    const void* requestData;
    /** request data size */
    uint32_t requestSize;
} __attribute__((packed)) SocketRequestData;

typedef struct SocketFdData {
    /** socket fd data **/
    int* fds;
    /** socket fd size */
    uint32_t nFds;
} __attribute__((packed)) SocketFdData;

class FaultLoggerdSocket {
public:
    explicit FaultLoggerdSocket(bool signalSafely = false) : signalSafely_(signalSafely) {}
    explicit FaultLoggerdSocket(int32_t fd, bool signalSafely = false) : socketFd_(fd), signalSafely_(signalSafely) {}
    void CloseSocketFileDescriptor();
    bool CreateSocketFileDescriptor(uint32_t timeout);
    bool StartConnect(const char* socketName);
    bool SendFileDescriptorToSocket(const int32_t* fds, uint32_t nFds) const;
    bool ReadFileDescriptorFromSocket(int32_t* fds, uint32_t nFds) const;
    bool SendMsgToSocket(const void* data, uint32_t dataLength) const;
    bool GetMsgFromSocket(void* data, uint32_t dataLength) const;
    int32_t RequestServer(const SocketRequestData& socketRequestData) const;
    int32_t RequestFdsFromServer(const SocketRequestData& socketRequestData, SocketFdData& socketFdData) const;
private:
    bool ConcatenateSocketName(char* dst, uint32_t dstSize, const char* src, uint32_t srcSize) const;
    bool SetSocketTimeOut(uint32_t timeout, int optName);
    int32_t socketFd_ = -1;
    bool signalSafely_;
};

}
}

#endif