/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files is writer log to file module on process dump module. */

#include "faultloggerd_socket.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <securec.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "init_socket.h"

bool StartConnect(int& sockfd, const char* path, const int timeout)
{
    bool ret = false;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        DfxLogError("%s :: Failed to socket\n", __func__);
        return ret;
    }

    do {
        if (timeout > 0) {
            struct timeval timev = {
                timeout,
                0
            };
            void* pTimev = &timev;
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, \
                static_cast<const char*>(pTimev), sizeof(timev)) != 0) {
                    DfxLogError("setsockopt SO_RCVTIMEO error");
            }
        }

        struct sockaddr_un server;
        errno_t err = memset_s(&server, sizeof(server), 0, sizeof(server));
        if (err != EOK) {
            DfxLogError("%s :: memset_s failed, err = %d.", __func__, (int)err);
            break;
        }
        server.sun_family = AF_LOCAL;
        if (strncpy_s(server.sun_path, sizeof(server.sun_path), path, sizeof(server.sun_path) - 1) != 0) {
            DfxLogError("%s :: strncpy failed.", __func__);
            break;
        }

        int len = (int)(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
        int connected = connect(sockfd, reinterpret_cast<struct sockaddr *>(&server), len);
        if (connected < 0) {
            DfxLogError("%s :: strncpy failed.", __func__);
            break;
        }

        ret = true;
    } while (false);

    if (!ret) {
        close(sockfd);
    }
    return ret;
}

static bool startListenInitFd(int& sockfd, const char* name, const int listenCnt)
{
    if (sockfd = GetControlSocket(name) < 0)
    {
        DfxLogError("%s :: Failed to get init socket fd", __func__);
        return false;
    }

    if (listen(sockfd, listenCnt) < 0)
    {
        DfxLogError("%s :: Failed to listen socket", __func__);
        close(sockfd);
        sockfd = -1;
        return false;
    }
    DfxLogInfo("%s :: success to listen socket", __func__);
    return true;
}

bool StartListen(int& sockfd, const char* name, const char* path, const int listenCnt)
{
    if (startListenInitFd(sockfd, name, listenCnt))
    {
        return true;
    }

    bool ret = false;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        DfxLogError("%s :: Failed to create socket", __func__);
        return ret;
    }

    do {
        struct sockaddr_un server;
        errno_t err = memset_s(&server, sizeof(server), 0, sizeof(server));
        if (err != EOK) {
            DfxLogError("%s :: memset_s failed, err = %d.", __func__, (int)err);
            break;
        }
        server.sun_family = AF_LOCAL;
        if (strncpy_s(server.sun_path, sizeof(server.sun_path), path, sizeof(server.sun_path) - 1) != 0) {
            DfxLogError("%s :: strncpy failed.", __func__);
            break;
        }

        unlink(path);
        if (bind(sockfd, (struct sockaddr *)&server,
            offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path)) < 0) {
            DfxLogError("%s :: Failed to bind socket", __func__);
            break;
        }

        if (listen(sockfd, listenCnt) < 0) {
            DfxLogError("%s :: Failed to listen socket", __func__);
            break;
        }
        ret = true;
    } while (false);

    if (!ret) {
        close(sockfd);
    }
    return ret;
}

static bool RecvMsgFromSocket(int sockfd, unsigned char* data, size_t& len)
{
    bool ret = false;
    if ((sockfd < 0) || (data == nullptr)) {
        return ret;
    }

    do {
        struct msghdr msgh = { 0 };
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

        if (recvmsg(sockfd, &msgh, 0) < 0) {
            DfxLogError("%s :: Failed to recv message\n", __func__);
            break;
        }

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
        if (cmsg == nullptr) {
            DfxLogError("%s :: Invalid message\n", __func__);
            break;
        }

        len = cmsg->cmsg_len - sizeof(struct cmsghdr);
        if (memcpy_s(data, len, CMSG_DATA(cmsg), len) != 0) {
            DfxLogError("%s :: memcpy error\n", __func__);
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
        struct msghdr msgh = { 0 };
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

        if (recvmsg(sockfd, &msgh, 0) < 0) {
            DfxLogError("%s :: Failed to recv message\n", __func__);
            break;
        }

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
        if (cmsg == nullptr) {
            DfxLogError("%s :: Invalid message\n", __func__);
            break;
        }

        if (memcpy_s(pucred, sizeof(struct ucred), CMSG_DATA(cmsg), sizeof(struct ucred)) != 0) {
            DfxLogError("%s :: memcpy error\n", __func__);
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
    
    struct msghdr msgh = { 0 };
    msgh.msg_name = nullptr;
    msgh.msg_namelen = 0;

    struct iovec iov;
    iov.iov_base = iovBase;
    iov.iov_len = iovLen;
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    msgh.msg_control = nullptr;
    msgh.msg_controllen = 0;

    if (sendmsg(sockfd, &msgh, 0) < 0) {
        DfxLogError("%s :: Failed to send message.", __func__);
        return false;
    }
    return true;
}

static bool SendMsgCtlToSocket(int sockfd, const void *cmsg, const int cmsgLen)
{
    if ((sockfd < 0) || (cmsg == nullptr) || (cmsgLen == 0)) {
        return false;
    }

    struct msghdr msgh = { 0 };
    char iovBase[] = "";
    struct iovec iov = {
        .iov_base = reinterpret_cast<void *>(iovBase),
        .iov_len = 1
    };
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
    }
    if (memcpy_s(CMSG_DATA(cmsgh), cmsgLen, cmsg, cmsgLen) != 0) {
        DfxLogError("%s :: memcpy error\n", __func__);
    }

    if (sendmsg(sockfd, &msgh, 0) < 0) {
        DfxLogError("%s :: Failed to send message", __func__);
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
        DfxLogError("%s :: Failed to recv message", __func__);
        return -1;
    }

    if (len != sizeof(int)) {
        DfxLogError("%s :: data is null or len is %d", __func__, len);
        return -1;
    }
    int fd = *(reinterpret_cast<int *>(data));
    DfxLogDebug("%s :: fd: %d", __func__, fd);
    return fd;
}