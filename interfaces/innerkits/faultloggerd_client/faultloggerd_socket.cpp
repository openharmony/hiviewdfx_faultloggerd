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
#include <securec.h>
#include <string>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "init_socket.h"

bool StartConnect(int& sockfd, const char* path, const int timeout)
{
    bool ret = false;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        DFXLOGE("%{public}s :: Failed to socket, errno(%{public}d)", __func__, errno);
        return ret;
    }

    do {
        if (timeout > 0) {
            struct timeval timev = {
                timeout,
                0
            };
            void* pTimev = &timev;
            if (OHOS_TEMP_FAILURE_RETRY(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, \
                static_cast<const char*>(pTimev), sizeof(timev))) != 0) {
                    DFXLOGE("setsockopt(%{public}d) SO_RCVTIMEO error, errno(%{public}d).", sockfd, errno);
            }
            if (OHOS_TEMP_FAILURE_RETRY(setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, \
                static_cast<const char*>(pTimev), sizeof(timev))) != 0) {
                    DFXLOGE("setsockopt(%{public}d) SO_SNDTIMEO error, errno(%{public}d).", sockfd, errno);
            }
        }

        std::string fullPath = std::string(FAULTLOGGERD_SOCK_BASE_PATH) + std::string(path);
        struct sockaddr_un server;
        (void)memset_s(&server, sizeof(server), 0, sizeof(server));
        server.sun_family = AF_LOCAL;
        errno_t err = strncpy_s(server.sun_path, sizeof(server.sun_path), fullPath.c_str(),
            sizeof(server.sun_path) - 1);
        if (err != EOK) {
            DFXLOGE("%{public}s :: strncpy failed, err = %{public}d.", __func__, (int)err);
            break;
        }

        int len = static_cast<int>(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
        int connected = OHOS_TEMP_FAILURE_RETRY(connect(sockfd, reinterpret_cast<struct sockaddr *>(&server), len));
        if (connected < 0) {
            DFXLOGE("%{public}s :: connect failed, errno = %{public}d.", __func__, errno);
            break;
        }

        ret = true;
    } while (false);

    if (!ret) {
        close(sockfd);
    }
    return ret;
}

static bool GetServerSocket(int& sockfd, const char* name)
{
    sockfd = OHOS_TEMP_FAILURE_RETRY(socket(AF_LOCAL, SOCK_STREAM, 0));
    if (sockfd < 0) {
        DFXLOGE("%{public}s :: Failed to create socket, errno(%{public}d)", __func__, errno);
        return false;
    }

    std::string path = std::string(FAULTLOGGERD_SOCK_BASE_PATH) + std::string(name);
    struct sockaddr_un server;
    (void)memset_s(&server, sizeof(server), 0, sizeof(server));
    server.sun_family = AF_LOCAL;
    if (strncpy_s(server.sun_path, sizeof(server.sun_path), path.c_str(), sizeof(server.sun_path) - 1) != 0) {
        DFXLOGE("%{public}s :: strncpy failed.", __func__);
        return false;
    }

    chmod(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH);
    unlink(path.c_str());

    int optval = 1;
    int ret = OHOS_TEMP_FAILURE_RETRY(setsockopt(sockfd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)));
    if (ret < 0) {
        DFXLOGE("%{public}s :: Failed to set socket option, errno(%{public}d)", __func__, errno);
        return false;
    }

    if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&server),
        offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path)) < 0) {
        DFXLOGE("%{public}s :: Failed to bind socket, errno(%{public}d)", __func__, errno);
        return false;
    }

    return true;
}

bool StartListen(int& sockfd, const char* name, const int listenCnt)
{
    if (name == nullptr) {
        return false;
    }
    sockfd = GetControlSocket(name);
    if (sockfd < 0) {
        DFXLOGW("%{public}s :: Failed to get socket fd by cfg", __func__);
        if (GetServerSocket(sockfd, name) == false) {
            DFXLOGE("%{public}s :: Failed to get socket fd by path", __func__);
            return false;
        }
    }

    if (listen(sockfd, listenCnt) < 0) {
        DFXLOGE("%{public}s :: Failed to listen socket, errno(%{public}d)", __func__, errno);
        close(sockfd);
        sockfd = -1;
        return false;
    }

    DFXLOGI("%{public}s :: success to listen socket", __func__);
    return true;
}

static bool RecvMsgFromSocket(int sockfd, unsigned char* data, size_t& len)
{
    bool ret = false;
    if ((sockfd < 0) || (data == nullptr)) {
        return ret;
    }

    do {
        struct msghdr msgh;
        (void)memset_s(&msgh, sizeof(msgh), 0, sizeof(msgh));
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

        if (OHOS_TEMP_FAILURE_RETRY(recvmsg(sockfd, &msgh, 0)) < 0) {
            DFXLOGE("%{public}s :: Failed to recv message, errno(%{public}d)\n", __func__, errno);
            break;
        }

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
        if (cmsg == nullptr) {
            DFXLOGE("%{public}s :: Invalid message\n", __func__);
            break;
        }

        len = cmsg->cmsg_len - sizeof(struct cmsghdr);
        if (memcpy_s(data, len, CMSG_DATA(cmsg), len) != 0) {
            DFXLOGE("%{public}s :: memcpy error\n", __func__);
            break;
        }

        ret = true;
    } while (false);
    return ret;
}

bool RecvMsgCredFromSocket(int sockfd, struct ucred* pucred)
{
    bool ret = false;
    if ((sockfd < 0) || (pucred == nullptr)) {
        return ret;
    }

    do {
        struct msghdr msgh;
        (void)memset_s(&msgh, sizeof(msgh), 0, sizeof(msgh));
        union {
            char buf[CMSG_SPACE(sizeof(struct ucred))];

            /* Space large enough to hold a 'ucred' structure */
            struct cmsghdr align;
        } controlMsg;

        msgh.msg_name = nullptr;
        msgh.msg_namelen = 0;

        int data;
        struct iovec iov = {
            .iov_base = &data,
            .iov_len = sizeof(data)
        };
        msgh.msg_iov = &iov;
        msgh.msg_iovlen = 1;

        msgh.msg_control = controlMsg.buf;
        msgh.msg_controllen = sizeof(controlMsg.buf);

        if (OHOS_TEMP_FAILURE_RETRY(recvmsg(sockfd, &msgh, 0)) < 0) {
            DFXLOGE("%{public}s :: Failed to recv message, errno(%{public}d)\n", __func__, errno);
            break;
        }

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
        if (cmsg == nullptr) {
            DFXLOGE("%{public}s :: Invalid message\n", __func__);
            break;
        }

        if (memcpy_s(pucred, sizeof(struct ucred), CMSG_DATA(cmsg), sizeof(struct ucred)) != 0) {
            DFXLOGE("%{public}s :: memcpy error\n", __func__);
            break;
        }

        ret = true;
    } while (false);
    return ret;
}

bool SendMsgIovToSocket(int sockfd, void *iovBase, const int iovLen)
{
    if ((sockfd < 0) || (iovBase == nullptr) || (iovLen == 0)) {
        return false;
    }

    struct msghdr msgh;
    (void)memset_s(&msgh, sizeof(msgh), 0, sizeof(msgh));
    msgh.msg_name = nullptr;
    msgh.msg_namelen = 0;

    struct iovec iov;
    iov.iov_base = iovBase;
    iov.iov_len = iovLen;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    msgh.msg_control = nullptr;
    msgh.msg_controllen = 0;

    if (OHOS_TEMP_FAILURE_RETRY(sendmsg(sockfd, &msgh, 0)) < 0) {
        DFXLOGE("%{public}s :: Failed to send message, errno(%{public}d).", __func__, errno);
        return false;
    }
    return true;
}

static bool SendMsgCtlToSocket(int sockfd, const void *cmsg, const int cmsgLen)
{
    if ((sockfd < 0) || (cmsg == nullptr) || (cmsgLen == 0)) {
        return false;
    }

    struct msghdr msgh;
    (void)memset_s(&msgh, sizeof(msgh), 0, sizeof(msgh));
    char iovBase[] = "";
    struct iovec iov = {
        .iov_base = reinterpret_cast<void *>(iovBase),
        .iov_len = 1
    };
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    int controlBufLen = CMSG_SPACE(static_cast<unsigned int>(cmsgLen));
    char controlBuf[controlBufLen];
    msgh.msg_control = controlBuf;
    msgh.msg_controllen = sizeof(controlBuf);

    struct cmsghdr *cmsgh = CMSG_FIRSTHDR(&msgh);
    if (cmsgh != nullptr) {
        cmsgh->cmsg_level = SOL_SOCKET;
        cmsgh->cmsg_type = SCM_RIGHTS;
        cmsgh->cmsg_len = CMSG_LEN(cmsgLen);
    }
    if (memcpy_s(CMSG_DATA(cmsgh), cmsgLen, cmsg, cmsgLen) != 0) {
        DFXLOGE("%{public}s :: memcpy error\n", __func__);
    }

    if (OHOS_TEMP_FAILURE_RETRY(sendmsg(sockfd, &msgh, 0)) < 0) {
        DFXLOGE("%{public}s :: Failed to send message, errno(%{public}d)", __func__, errno);
        return false;
    }
    return true;
}

bool SendFileDescriptorToSocket(int sockfd, int fd)
{
    return SendMsgCtlToSocket(sockfd, reinterpret_cast<void *>(&fd), sizeof(fd));
}

int ReadFileDescriptorFromSocket(int sockfd)
{
    size_t len = sizeof(int);
    unsigned char data[len + 1];
    if (!RecvMsgFromSocket(sockfd, data, len)) {
        DFXLOGE("%{public}s :: Failed to recv message", __func__);
        return -1;
    }

    if (len != sizeof(int)) {
        DFXLOGE("%{public}s :: data is null or len is %{public}zu", __func__, len);
        return -1;
    }
    int fd = *(reinterpret_cast<int *>(data));
    DFXLOGD("%{public}s :: fd: %{public}d", __func__, fd);
    return fd;
}