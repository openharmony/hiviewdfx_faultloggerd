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

#include "faultloggerd_socket.h"

#include <cstddef>
#include <cstdio>
#include <string>

#include <securec.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "init_socket.h"

namespace {
constexpr int SOCKET_BUFFER_SIZE = 32;
}

#define LOGE(isSigalSafe, fmt, ...) \
    if (!isSigalSafe) \
        DFXLOGE(fmt, ##__VA_ARGS__)

static bool GetServerSocket(int32_t& sockFd, const char* name)
{
    sockFd = OHOS_TEMP_FAILURE_RETRY(socket(AF_LOCAL, SOCK_STREAM, 0));
    if (sockFd < 0) {
        DFXLOGE("%{public}s :: Failed to create socket, errno(%{public}d)", __func__, errno);
        return false;
    }
    fdsan_exchange_owner_tag(sockFd, 0, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, LOG_DOMAIN));

    std::string path = std::string(FAULTLOGGERD_SOCK_BASE_PATH) + std::string(name);
    struct sockaddr_un server{0};
    server.sun_family = AF_LOCAL;
    if (strncpy_s(server.sun_path, sizeof(server.sun_path), path.c_str(), sizeof(server.sun_path) - 1) != 0) {
        DFXLOGE("%{public}s :: strncpy failed.", __func__);
        return false;
    }

    chmod(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH);
    unlink(path.c_str());

    int optval = 1;
    int ret = OHOS_TEMP_FAILURE_RETRY(setsockopt(sockFd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)));
    if (ret < 0) {
        DFXLOGE("%{public}s :: Failed to set socket option, errno(%{public}d)", __func__, errno);
        return false;
    }

    if (bind(sockFd, reinterpret_cast<struct sockaddr *>(&server),
        offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path)) < 0) {
        DFXLOGE("%{public}s :: Failed to bind socket, errno(%{public}d)", __func__, errno);
        return false;
    }

    return true;
}

bool StartListen(int32_t& sockFd, const char* name, uint32_t listenCnt)
{
    if (name == nullptr) {
        return false;
    }
    sockFd = GetControlSocket(name);
    if (sockFd < 0) {
        DFXLOGW("%{public}s :: Failed to get socket fd by cfg", __func__);
        if (!GetServerSocket(sockFd, name)) {
            DFXLOGE("%{public}s :: Failed to get socket fd by path", __func__);
            return false;
        }
    }

    if (listen(sockFd, listenCnt) < 0) {
        DFXLOGE("%{public}s :: Failed to listen socket, errno(%{public}d)", __func__, errno);
        fdsan_close_with_tag(sockFd, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, LOG_DOMAIN));
        sockFd = -1;
        return false;
    }

    DFXLOGI("%{public}s :: success to listen socket %{public}s", __func__, name);
    return true;
}

bool SendFileDescriptorToSocket(int32_t sockFd, const int32_t* fds, uint32_t nFds)
{
    FaultLoggerdSocket faultLoggerd(sockFd);
    return faultLoggerd.SendFileDescriptorToSocket(fds, nFds);
}

bool SendMsgToSocket(int32_t sockFd, const void* data, uint32_t dataLength)
{
    FaultLoggerdSocket faultLoggerd(sockFd);
    return faultLoggerd.SendMsgToSocket(data, dataLength);
}

bool GetMsgFromSocket(int32_t sockFd, void* data, uint32_t dataLength)
{
    FaultLoggerdSocket faultLoggerd(sockFd);
    return faultLoggerd.GetMsgFromSocket(data, dataLength);
}

bool FaultLoggerdSocket::CreateSocketFileDescriptor(uint32_t timeout)
{
    if (socketFd_ >= 0) {
        LOGE(signalSafely_, "%{public}s :: Create the socket repeatedly", __func__);
        return false;
    }
    socketFd_ = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (socketFd_ < 0) {
        LOGE(signalSafely_, "%{public}s :: Failed to create socket, errno(%{public}d)", __func__, errno);
        return false;
    }
    if (timeout > 0) {
        SetSocketTimeOut(timeout, SO_RCVTIMEO);
        SetSocketTimeOut(timeout, SO_SNDTIMEO);
    }
    return true;
}

void FaultLoggerdSocket::CloseSocketFileDescriptor()
{
    if (socketFd_ >= 0) {
        close(socketFd_);
        socketFd_ = -1;
    }
}

bool FaultLoggerdSocket::SetSocketTimeOut(uint32_t timeout, int optName)
{
    struct timeval timev = { timeout, 0 };
    if (OHOS_TEMP_FAILURE_RETRY(setsockopt(socketFd_, SOL_SOCKET, optName, &timev, sizeof(timev))) != 0) {
        LOGE(signalSafely_, "setsockopt(%{public}d) SO_RCVTIMEO error, errno(%{public}d).", socketFd_, errno);
        return false;
    }
    return true;
}

bool FaultLoggerdSocket::ConcatenateSocketName(char* dst, uint32_t dstSize, const char* src, uint32_t srcSize) const
{
    if (dstSize <= srcSize) {
        return false;
    }
    auto err = strncpy_s(dst, dstSize, src, srcSize);
    if (err != EOK) {
        LOGE(signalSafely_, "%{public}s :: strncpy failed, err = %{public}d.", __func__, static_cast<int>(err));
        return false;
    }
    return true;
}

bool FaultLoggerdSocket::StartConnect(const char* socketName)
{
    if (UNLIKELY(socketFd_ < 0 || socketName == nullptr)) {
        return false;
    }
    struct sockaddr_un server{0};
    server.sun_family = AF_LOCAL;
    auto baseLen = strlen(FAULTLOGGERD_SOCK_BASE_PATH);
    auto dstSize = sizeof(server.sun_path);
    if (!ConcatenateSocketName(server.sun_path, dstSize, FAULTLOGGERD_SOCK_BASE_PATH, baseLen)) {
        return false;
    }
    if (!ConcatenateSocketName(server.sun_path + baseLen, dstSize - baseLen, socketName, strlen(socketName))) {
        return false;
    }
    int len = static_cast<int>(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
    int connected = OHOS_TEMP_FAILURE_RETRY(connect(socketFd_, reinterpret_cast<struct sockaddr*>(&server), len));
    if (connected < 0) {
        LOGE(signalSafely_, "%{public}s :: connect failed, errno = %{public}d.", __func__, errno);
        return false;
    }
    return true;
}

bool FaultLoggerdSocket::SendFileDescriptorToSocket(const int32_t* fds, uint32_t nFds) const
{
    if ((socketFd_ < 0) || (fds == nullptr) || (nFds == 0)) {
        return false;
    }
    auto cmsgLen = sizeof(uint32_t) * nFds;
    struct msghdr msgh{0};
    char iovBase[] = "";
    struct iovec iov = {.iov_base = iovBase, .iov_len = 1};
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    int controlBufLen = CMSG_SPACE(cmsgLen);
    char controlBuf[controlBufLen];
    msgh.msg_control = controlBuf;
    msgh.msg_controllen = sizeof(controlBuf);

    struct cmsghdr *cmsgh = CMSG_FIRSTHDR(&msgh);
    if (cmsgh != nullptr) {
        cmsgh->cmsg_level = SOL_SOCKET;
        cmsgh->cmsg_type = SCM_RIGHTS;
        cmsgh->cmsg_len = CMSG_LEN(cmsgLen);
        if (memcpy_s(CMSG_DATA(cmsgh), cmsgLen, fds, cmsgLen) != 0) {
            LOGE(signalSafely_, "%{public}s :: memcpy error", __func__);
        }
    }
    if (OHOS_TEMP_FAILURE_RETRY(sendmsg(socketFd_, &msgh, 0)) < 0) {
        LOGE(signalSafely_, "%{public}s :: Failed to send message, errno(%{public}d)", __func__, errno);
        return false;
    }
    return true;
}

bool FaultLoggerdSocket::ReadFileDescriptorFromSocket(int32_t *fds, uint32_t nFds) const
{
    if (socketFd_ < 0 || fds == nullptr || nFds == 0) {
        return false;
    }
    auto dataLength = sizeof(int32_t) * nFds;
    struct msghdr msgh{0};
    char msgBuffer[SOCKET_BUFFER_SIZE] = { 0 };
    struct iovec iov = {
        .iov_base = msgBuffer,
        .iov_len = sizeof(msgBuffer)
    };
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    char ctlBuffer[SOCKET_BUFFER_SIZE] = { 0 };
    msgh.msg_control = ctlBuffer;
    msgh.msg_controllen = sizeof(ctlBuffer);

    if (OHOS_TEMP_FAILURE_RETRY(recvmsg(socketFd_, &msgh, 0)) < 0) {
        LOGE(signalSafely_, "%{public}s :: Failed to recv message, errno(%{public}d)", __func__, errno);
        return false;
    }
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
    if (cmsg == nullptr) {
        LOGE(signalSafely_, "%{public}s :: Invalid message", __func__);
        return false;
    }
    if (dataLength != cmsg->cmsg_len - sizeof(struct cmsghdr)) {
        LOGE(signalSafely_, "%{public}s :: msg length is not matched", __func__);
        return false;
    }
    if (memcpy_s(fds, dataLength, CMSG_DATA(cmsg), dataLength) != 0) {
        LOGE(signalSafely_, "%{public}s :: memcpy error", __func__);
        return false;
    }
    return  true;
}

bool FaultLoggerdSocket::SendMsgToSocket(const void *data, uint32_t dataLength) const
{
    if (socketFd_ < 0 || data == nullptr || dataLength == 0) {
        return false;
    }
    if (OHOS_TEMP_FAILURE_RETRY(write(socketFd_, data, dataLength)) != dataLength) {
        LOGE(signalSafely_, "Failed to write request message to socket, errno(%{public}d).", errno);
        return false;
    }
    return true;
}

bool FaultLoggerdSocket::GetMsgFromSocket(void* data, uint32_t dataLength) const
{
    if (socketFd_ < 0 || data == nullptr || dataLength == 0) {
        return false;
    }
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(socketFd_, data, dataLength));
    if (nread <= 0) {
        LOGE(signalSafely_, "Failed to get message from socket, %{public}zd, errno(%{public}d). ", nread, errno);
        return false;
    }
    return true;
}

int32_t FaultLoggerdSocket::RequestServer(const SocketRequestData& socketRequestData) const
{
    if (!SendMsgToSocket(socketRequestData.requestData, socketRequestData.requestSize)) {
        return ResponseCode::SEND_DATA_FAILED;
    }
    int32_t retCode{ResponseCode::RECEIVE_DATA_FAILED};
    GetMsgFromSocket(&retCode, sizeof (retCode));
    return retCode;
}

int32_t FaultLoggerdSocket::RequestFdsFromServer(const SocketRequestData &socketRequestData,
    SocketFdData &socketFdData) const
{
    int32_t retCode = RequestServer(socketRequestData);
    if (retCode != ResponseCode::REQUEST_SUCCESS) {
        return retCode;
    }
    return ReadFileDescriptorFromSocket(socketFdData.fds, socketFdData.nFds) ? ResponseCode::REQUEST_SUCCESS :
        ResponseCode::RECEIVE_DATA_FAILED;
}