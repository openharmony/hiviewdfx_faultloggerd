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
#include "init_socket.h"

namespace {
constexpr int SOCKET_BUFFER_SIZE = 32;
}

bool StartConnect(int32_t sockFd, const char* socketName, uint32_t timeout)
{
    if (UNLIKELY(sockFd < 0 || socketName == nullptr)) {
        return false;
    }
    if (timeout > 0) {
        struct timeval timev = { timeout, 0 };
        if (OHOS_TEMP_FAILURE_RETRY(setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, &timev, sizeof(timev))) != 0) {
            DFXLOGE("setsockopt(%{public}d) SO_RCVTIMEO error, errno(%{public}d).", sockFd, errno);
        }
        if (OHOS_TEMP_FAILURE_RETRY(setsockopt(sockFd, SOL_SOCKET, SO_SNDTIMEO, &timev, sizeof(timev))) != 0) {
            DFXLOGE("setsockopt(%{public}d) SO_SNDTIMEO error, errno(%{public}d).", sockFd, errno);
        }
    }

    struct sockaddr_un server{0};
    server.sun_family = AF_LOCAL;
    std::string fullPath = std::string(FAULTLOGGERD_SOCK_BASE_PATH) + std::string(socketName);
    errno_t err = strncpy_s(server.sun_path, sizeof(server.sun_path), fullPath.c_str(), sizeof(server.sun_path) - 1);
    if (err != EOK) {
        DFXLOGE("%{public}s :: strncpy failed, err = %{public}d.", __func__, (int)err);
        return false;
    }

    int len = static_cast<int>(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
    int connected = OHOS_TEMP_FAILURE_RETRY(connect(sockFd, reinterpret_cast<struct sockaddr *>(&server), len));
    if (connected < 0) {
        DFXLOGE("%{public}s :: connect failed, errno = %{public}d.", __func__, errno);
        return false;
    }
    return true;
}

int32_t CreateSocketFd()
{
    int32_t socketFd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (socketFd < 0) {
        DFXLOGE("%{public}s :: Failed to create socket, errno(%{public}d)", __func__, errno);
        return -1;
    }
    return socketFd;
}

static bool GetServerSocket(int32_t& sockFd, const char* name)
{
    sockFd = OHOS_TEMP_FAILURE_RETRY(socket(AF_LOCAL, SOCK_STREAM, 0));
    if (sockFd < 0) {
        DFXLOGE("%{public}s :: Failed to create socket, errno(%{public}d)", __func__, errno);
        return false;
    }

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
        close(sockFd);
        sockFd = -1;
        return false;
    }

    DFXLOGI("%{public}s :: success to listen socket %{public}s", __func__, name);
    return true;
}

static bool RecvMsgFromSocket(int sockFd, void* data, const size_t dataLength)
{
    if (sockFd < 0 || data == nullptr) {
        return false;
    }

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

    if (OHOS_TEMP_FAILURE_RETRY(recvmsg(sockFd, &msgh, 0)) < 0) {
        DFXLOGE("%{public}s :: Failed to recv message, errno(%{public}d)", __func__, errno);
        return false;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
    if (cmsg == nullptr) {
        DFXLOGE("%{public}s :: Invalid message", __func__);
        return false;
    }
    if (dataLength != cmsg->cmsg_len - sizeof(struct cmsghdr)) {
        DFXLOGE("%{public}s :: msg length is not matched", __func__);
        return false;
    }
    if (memcpy_s(data, dataLength, CMSG_DATA(cmsg), dataLength) != 0) {
        DFXLOGE("%{public}s :: memcpy error", __func__);
        return false;
    }
    return  true;
}

static bool SendMsgCtlToSocket(int sockFd, const void *cmsg, uint32_t cmsgLen)
{
    if ((sockFd < 0) || (cmsg == nullptr) || (cmsgLen == 0)) {
        return false;
    }

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
        if (memcpy_s(CMSG_DATA(cmsgh), cmsgLen, cmsg, cmsgLen) != 0) {
            DFXLOGE("%{public}s :: memcpy error", __func__);
        }
    }

    if (OHOS_TEMP_FAILURE_RETRY(sendmsg(sockFd, &msgh, 0)) < 0) {
        DFXLOGE("%{public}s :: Failed to send message, errno(%{public}d)", __func__, errno);
        return false;
    }
    return true;
}

bool SendFileDescriptorToSocket(int32_t sockFd, const int32_t* fds, uint32_t nFds)
{
    if (nFds == 0 || fds == nullptr) {
        return false;
    }
    return SendMsgCtlToSocket(sockFd, fds, sizeof(int32_t) * nFds);
}

bool ReadFileDescriptorFromSocket(int32_t sockFd, int32_t* fds, uint32_t nFds)
{
    if (nFds == 0 || fds == nullptr) {
        return false;
    }
    if (!RecvMsgFromSocket(sockFd, fds, sizeof(int32_t) * nFds)) {
        DFXLOGE("%{public}s :: Failed to recv message", __func__);
        return false;
    }
    return true;
}

bool SendMsgToSocket(int32_t sockFd, const void* data, uint32_t dataLength)
{
    if (data == nullptr || sockFd < 0 || dataLength == 0) {
        DFXLOGE("Failed to write request message to socket, invalid data or socket fd %{public}d.", sockFd);
        return false;
    }
    if (OHOS_TEMP_FAILURE_RETRY(write(sockFd, data, dataLength)) != dataLength) {
        DFXLOGE("Failed to write request message to socket, errno(%{public}d).", errno);
        return false;
    }
    return true;
}

bool GetMsgFromSocket(int32_t sockFd, void* data, uint32_t dataLength)
{
    if (data == nullptr || sockFd < 0 || dataLength == 0) {
        DFXLOGE("Failed to read message from socket, invalid data or socket fd %{public}d.", sockFd);
        return false;
    }
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(sockFd, data, dataLength));
    if (nread <= 0) {
        DFXLOGE("Failed to get message from socket, %{public}zd, errno(%{public}d). ", nread, errno);
        return false;
    }
    return true;
}