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
#include "faultloggerd_client.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <securec.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"

namespace {
static const int32_t SOCKET_BUFFER_SIZE = 256;
static const int32_t SOCKET_TIMEOUT = 5;
static const char FAULTLOGGERD_SOCK_PATH[] = "/dev/unix/socket/faultloggerd.server";
}

static int ReadFileDescriptorFromSocket(int socket)
{
    struct msghdr msg = { 0 };
    char messageBuffer[SOCKET_BUFFER_SIZE];
    struct iovec io = {
        .iov_base = messageBuffer,
        .iov_len = sizeof(messageBuffer)
    };
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char controlBuffer[SOCKET_BUFFER_SIZE];
    msg.msg_control = controlBuffer;
    msg.msg_controllen = sizeof(controlBuffer);

    if (recvmsg(socket, &msg, 0) < 0) {
        DfxLogError("Failed to receive message\n");
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr || cmsg->cmsg_len != CMSG_LEN(sizeof(int))) {
        DfxLogError("Invalid message\n");
        return -1;
    }

    unsigned char *data = CMSG_DATA(cmsg);
    if (data == nullptr) {
        return -1;
    }
    return *(reinterpret_cast<int *>(data));
}

bool ReadStringFromFile(const char *path, char *buf, size_t len)
{
    if ((len <= 1) || (buf == nullptr) || (path == nullptr)) {
        return false;
    }

    char realPath[PATH_MAX];
    if (realpath(path, realPath) == nullptr) {
        return false;
    }

    FILE *fp = fopen(realPath, "r");
    if (fp == nullptr) {
        // log failure
        return false;
    }

    char *ptr = buf;
    for (size_t i = 0; i < len; i++) {
        int c = getc(fp);
        if (c == EOF) {
            *ptr++ = 0x00;
            break;
        } else {
            *ptr++ = c;
        }
    }
    fclose(fp);
    return false;
}

void FillRequest(int32_t type, FaultLoggerdRequest *request)
{
    if (request == nullptr) {
        DfxLogError("nullptr request");
        return;
    }

    request->type = type;
    request->pid = getpid();
    request->tid = gettid();
    request->uid = (int32_t)getuid();
    ReadStringFromFile("/proc/self/cmdline", request->module, sizeof(request->module));
}

int32_t RequestFileDescriptor(int32_t type)
{
    struct FaultLoggerdRequest request;
    errno_t err = memset_s(&request, sizeof(request), 0, sizeof(request));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s request failed..", __func__);
    }
    FillRequest(type, &request);
    return RequestFileDescriptorEx(&request);
}

int32_t RequestLogFileDescriptor(struct FaultLoggerdRequest *request)
{
    request->clientType = (int32_t)FaultLoggerClientType::LOG_FILE_DES_CLIENT;
    return RequestFileDescriptorEx(request);
}

int32_t RequestFileDescriptorEx(const struct FaultLoggerdRequest *request)
{
    int sockfd;
    struct sockaddr_un server;
    struct timeval timeout = {
        SOCKET_TIMEOUT,
        0
    };
    void* pTimeout = &timeout;
    if (request == nullptr) {
        DfxLogError("nullptr request");
        return -1;
    }

    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        DfxLogError("client socket error");
        return -1;
    }
    int setSocketOptRet = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, \
        static_cast<const char*>(pTimeout), sizeof(timeout));
    if (setSocketOptRet != 0) {
        DfxLogError("setSocketOptRet error");
    }
    errno_t err = memset_s(&server, sizeof(server), 0, sizeof(server));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s server failed..", __func__);
    }
    server.sun_family = AF_LOCAL;
    if (strncpy_s(server.sun_path, sizeof(server.sun_path),
        FAULTLOGGERD_SOCK_PATH, strlen(FAULTLOGGERD_SOCK_PATH)) != 0) {
        DfxLogError("Failed to set sock path.");
        close(sockfd);
        return -1;
    }

    int len = (int)(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
    if (connect(sockfd, reinterpret_cast<struct sockaddr *>(&server), len) < 0) {
        DfxLogError("RequestFileDescriptorEx :: connect error");
        close(sockfd);
        return -1;
    }

    write(sockfd, request, sizeof(struct FaultLoggerdRequest));
    int fd = ReadFileDescriptorFromSocket(sockfd);
    DfxLogDebug("RequestFileDescriptorEx(%d).\n", fd);
    close(sockfd);
    return fd;
}

static FaultLoggerCheckPermissionResp SendUidToServer(int sockfd)
{
    FaultLoggerCheckPermissionResp mRsp = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;

    do {
        struct msghdr msgh;
        msgh.msg_name = nullptr;
        msgh.msg_namelen = 0;

        /* On Linux, we must transmit at least 1 byte of real data in
           order to send ancillary data */

        int data = 12345;
        /* Data is optionally taken from command line */
        struct iovec iov;
        msgh.msg_iov = &iov;
        msgh.msg_iovlen = 1;
        iov.iov_base = &data;
        iov.iov_len = sizeof(data);

        /* Don't construct an explicit credentials structure. (It is not
           necessary to do so, if we just want the receiver to receive
           our real credentials.) */
        msgh.msg_control = nullptr;
        msgh.msg_controllen = 0;

        if (sendmsg(sockfd, &msgh, 0) < 0) {
            DfxLogError("Failed to send uid to server.");
            break;
        }

        char recvbuf[SOCKET_BUFFER_SIZE] = {'\0'};
        ssize_t count = recv(sockfd, recvbuf, sizeof(recvbuf), 0);
        if (count < 0) {
            DfxLogError("Failed to recv uid check result from server.");
            break;
        }

        mRsp = (FaultLoggerCheckPermissionResp)atoi(recvbuf);
    } while (false);

    return mRsp;
}

bool CheckConnectStatus()
{
    int sockfd = -1;
    bool check_status = false;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        return false;
    }
    do {
        struct sockaddr_un server;
        errno_t ret = memset_s(&server, sizeof(server), 0, sizeof(server));
        if (ret != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)ret);
            break;
        }
        server.sun_family = AF_LOCAL;
        if (strncpy_s(server.sun_path, sizeof(server.sun_path),
            FAULTLOGGERD_SOCK_PATH, strlen(FAULTLOGGERD_SOCK_PATH)) != 0) {
            break;
        }

        int len = (int)(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
        int connect_status = connect(sockfd, reinterpret_cast<struct sockaddr *>(&server), len);
        if (connect_status == 0) {
            check_status = true;
        }
    } while (false);
    close(sockfd);
    return check_status;
}

static bool SendRequestToServer(const FaultLoggerdRequest &request)
{
    int sockfd = -1;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        return false;
    }

    do {
        struct sockaddr_un server;
        char ControlBuffer[SOCKET_BUFFER_SIZE];

        errno_t ret = memset_s(&server, sizeof(server), 0, sizeof(server));
        if (ret != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)ret);
            break;
        }
        server.sun_family = AF_LOCAL;
        if (strncpy_s(server.sun_path, sizeof(server.sun_path),
            FAULTLOGGERD_SOCK_PATH, strlen(FAULTLOGGERD_SOCK_PATH)) != 0) {
            break;
        }

        int len = (int)(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
        if (connect(sockfd, reinterpret_cast<struct sockaddr *>(&server), len) < 0) {
            break;
        }

        if (write(sockfd, &request, sizeof(struct FaultLoggerdRequest)) != static_cast<long>(sizeof(request))) {
            break;
        }

        ret = memset_s(&ControlBuffer, sizeof(ControlBuffer), 0, SOCKET_BUFFER_SIZE);
        if (ret != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)ret);
            break;
        }
        if (read(sockfd, ControlBuffer, sizeof(ControlBuffer) - 1) != strlen(FAULTLOGGER_DAEMON_RESP)) {
            break;
        }

        FaultLoggerCheckPermissionResp mRsp = SendUidToServer(sockfd);
        close(sockfd);

        DfxLogInfo("SendRequestToServer :: mRsp(%d).", (int)mRsp);

        if ((FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS == mRsp)
                || (FaultLoggerSdkDumpResp::SDK_DUMP_PASS == (FaultLoggerSdkDumpResp)mRsp)) {
            return true;
        } else {
            return false;
        }
    } while (false);

    close(sockfd);
    return false;
}

bool RequestCheckPermission(int32_t pid)
{
    DfxLogInfo("RequestCheckPermission :: %d.", pid);

    if (pid <= 0) {
        return false;
    }

    struct FaultLoggerdRequest request;
    errno_t err = memset_s(&request, sizeof(request), 0, sizeof(request));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s request failed..", __func__);
    }

    request.pid = pid;
    request.clientType = (int32_t)FaultLoggerClientType::PERMISSION_CLIENT;

    return SendRequestToServer(request);
}

bool RequestSdkDump(int32_t pid, int32_t tid)
{
    DfxLogInfo("RequestSdkDump :: pid(%d), tid(%d).", pid, tid);

    if (pid <= 0 || tid < 0) {
        return false;
    }

    struct FaultLoggerdRequest request;
    errno_t err = memset_s(&request, sizeof(request), 0, sizeof(request));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s request failed..", __func__);
    }
    request.pid = pid;
    request.tid = tid;
    request.callerPid = getpid();
    request.callerTid = syscall(SYS_gettid);
    request.clientType = (int32_t)FaultLoggerClientType::SDK_DUMP_CLIENT;

    return SendRequestToServer(request);
}

void RequestPrintTHilog(const char *msg, int length)
{
    if (length >= LOG_BUF_LEN) {
        return;
    }

    struct FaultLoggerdRequest request;
    errno_t err = memset_s(&request, sizeof(request), 0, sizeof(request));
    if (err != EOK) {
        DfxLogError("%s :: msmset_s request failed..", __func__);
    }
    request.clientType = (int32_t)FaultLoggerClientType::PRINT_T_HILOG_CLIENT;

    int sockfd = -1;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        return;
    }

    do {
        struct sockaddr_un server;
        char ControlBuffer[SOCKET_BUFFER_SIZE];

        errno_t ret = memset_s(&server, sizeof(server), 0, sizeof(server));
        if(ret != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)ret);
            break;
        }
        server.sun_family = AF_LOCAL;
        if (strncpy_s(server.sun_path, sizeof(server.sun_path),
            FAULTLOGGERD_SOCK_PATH, strlen(FAULTLOGGERD_SOCK_PATH)) != 0) {
            break;
        }

        int len = (int)(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
        if (connect(sockfd, reinterpret_cast<struct sockaddr *>(&server), len) < 0) {
            break;
        }

        if (write(sockfd, &request, sizeof(struct FaultLoggerdRequest)) != static_cast<long>(sizeof(request))) {
            break;
        }

        ret = memset_s(&ControlBuffer, sizeof(ControlBuffer), 0, SOCKET_BUFFER_SIZE);
        if (ret != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)ret);
            break;
        }
        if (read(sockfd, ControlBuffer, sizeof(ControlBuffer) - 1) != \
            static_cast<long>(strlen(FAULTLOGGER_DAEMON_RESP))) {
            break;
        }
        if (write(sockfd, msg, strlen(msg)) != static_cast<long>(strlen(msg))) {
            break;
        }
    } while (false);
    close(sockfd);
}

