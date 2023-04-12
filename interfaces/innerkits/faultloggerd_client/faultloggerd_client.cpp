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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "faultloggerd_socket.h"

static const int32_t SOCKET_TIMEOUT = 5;

static void FillRequest(int32_t type, FaultLoggerdRequest *request)
{
    if (request == nullptr) {
        DFXLOG_ERROR("nullptr request");
        return;
    }

    request->type = type;
    request->pid = getpid();
    request->tid = gettid();
    request->uid = getuid();
    request->time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    OHOS::HiviewDFX::ReadStringFromFile("/proc/self/cmdline", request->module, sizeof(request->module));
}

int32_t RequestFileDescriptor(int32_t type)
{
    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
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
    if (request == nullptr) {
        DFXLOG_ERROR("nullptr request");
        return -1;
    }

    int sockfd;
    if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, SOCKET_TIMEOUT)) {
        DFXLOG_ERROR("StartConnect failed");
        return -1;
    }

    write(sockfd, request, sizeof(struct FaultLoggerdRequest));
    int fd = ReadFileDescriptorFromSocket(sockfd);
    DFXLOG_DEBUG("RequestFileDescriptorEx(%d).\n", fd);
    close(sockfd);
    return fd;
}

static bool CheckReadResp(int sockfd)
{
    char ControlBuffer[SOCKET_BUFFER_SIZE] = {0};
    (void)memset_s(&ControlBuffer, sizeof(ControlBuffer), 0, SOCKET_BUFFER_SIZE);

    ssize_t nread = read(sockfd, ControlBuffer, sizeof(ControlBuffer) - 1);
    if (nread != static_cast<ssize_t>(strlen(FAULTLOGGER_DAEMON_RESP))) {
        DFXLOG_ERROR("nread: %d.", nread);
        return false;
    }
    return true;
}

static int32_t RequestFileDescriptorByCheck(const struct FaultLoggerdRequest *request)
{
    int32_t fd = -1;
    if (request == nullptr) {
        DFXLOG_ERROR("nullptr request");
        return -1;
    }

    int sockfd = -1;
    do {
        if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, SOCKET_TIMEOUT)) {
            DFXLOG_ERROR("StartConnect failed");
            break;
        }

        write(sockfd, request, sizeof(struct FaultLoggerdRequest));

        if (!CheckReadResp(sockfd)) {
            break;
        }

        int data = 12345;
        if (!SendMsgIovToSocket(sockfd, reinterpret_cast<void *>(&data), sizeof(data))) {
            DFXLOG_ERROR("%s :: Failed to sendmsg.", __func__);
            break;
        }

        fd = ReadFileDescriptorFromSocket(sockfd);
        DFXLOG_DEBUG("RequestFileDescriptorByCheck(%d).\n", fd);
    } while (false);
    close(sockfd);
    return fd;
}

static int SendUidToServer(int sockfd)
{
    int mRsp = (int)FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;

    int data = 12345;
    if (!SendMsgIovToSocket(sockfd, reinterpret_cast<void *>(&data), sizeof(data))) {
        DFXLOG_ERROR("%s :: Failed to sendmsg.", __func__);
        return mRsp;
    }

    char recvbuf[SOCKET_BUFFER_SIZE] = {'\0'};
    ssize_t count = recv(sockfd, recvbuf, sizeof(recvbuf), 0);
    if (count < 0) {
        DFXLOG_ERROR("%s :: Failed to recv.", __func__);
        return mRsp;
    }

    mRsp = atoi(recvbuf);
    return mRsp;
}

bool CheckConnectStatus()
{
    int sockfd = -1;
    if (StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, -1)) {
        close(sockfd);
        return true;
    }
    return false;
}

static int SendRequestToServer(const FaultLoggerdRequest &request)
{
    int sockfd = -1;
    int resRsp = (int)FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;
    do {
        if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, -1)) {
            DFXLOG_ERROR("StartConnect failed.");
            break;
        }

        if (write(sockfd, &request, sizeof(struct FaultLoggerdRequest)) != static_cast<long>(sizeof(request))) {
            DFXLOG_ERROR("write failed.");
            break;
        }

        if (!CheckReadResp(sockfd)) {
            break;
        }
        resRsp = SendUidToServer(sockfd);
    } while (false);

    close(sockfd);
    DFXLOG_INFO("SendRequestToServer :: resRsp(%d).", resRsp);
    return resRsp;
}

bool RequestCheckPermission(int32_t pid)
{
    DFXLOG_INFO("RequestCheckPermission :: %d.", pid);
    if (pid <= 0) {
        return false;
    }

    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));

    request.pid = pid;
    request.clientType = (int32_t)FaultLoggerClientType::PERMISSION_CLIENT;

    bool ret = false;
    if (SendRequestToServer(request) == (int)FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS) {
        ret = true;
    }
    return ret;
}

int RequestSdkDump(int32_t type, int32_t pid, int32_t tid)
{
    DFXLOG_INFO("RequestSdkDump :: type(%d), pid(%d), tid(%d).", type, pid, tid);
    if (pid <= 0 || tid < 0) {
        return -1;
    }

    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.sigCode = type;
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
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.clientType = (int32_t)FaultLoggerClientType::PRINT_T_HILOG_CLIENT;

    int sockfd = -1;
    do {
        if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, -1)) {
            DFXLOG_ERROR("StartConnect failed");
            break;
        }

        if (write(sockfd, &request, sizeof(struct FaultLoggerdRequest)) != static_cast<long>(sizeof(request))) {
            break;
        }

        if (!CheckReadResp(sockfd)) {
            break;
        }

        int nwrite = write(sockfd, msg, strlen(msg));
        if (nwrite != static_cast<long>(strlen(msg))) {
            DFXLOG_ERROR("nwrite: %d.", nwrite);
            break;
        }
    } while (false);
    close(sockfd);
}

int32_t RequestPipeFd(int32_t pid, int32_t pipeType)
{
    if (pipeType < static_cast<int32_t>(FaultLoggerPipeType::PIPE_FD_READ_BUF) ||
        pipeType > static_cast<int32_t>(FaultLoggerPipeType::PIPE_FD_WRITE_RES)) {
        DFXLOG_ERROR("%s :: pipeType(%d) failed.", __func__, pipeType);
        return -1;
    }
    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(struct FaultLoggerdRequest));
    request.pipeType = pipeType;
    request.pid = pid;
    request.callerPid = getpid();
    request.callerTid = syscall(SYS_gettid);
    request.clientType = (int32_t)FaultLoggerClientType::PIPE_FD_CLIENT;
    if ((pipeType == static_cast<int32_t>(FaultLoggerPipeType::PIPE_FD_READ_BUF)) ||
        (pipeType == static_cast<int32_t>(FaultLoggerPipeType::PIPE_FD_READ_RES))) {
        return RequestFileDescriptorByCheck(&request);
    }
    return RequestFileDescriptorEx(&request);
}

int32_t RequestDelPipeFd(int32_t pid)
{
    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(struct FaultLoggerdRequest));
    request.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    request.pid = pid;
    request.clientType = (int32_t)FaultLoggerClientType::PIPE_FD_CLIENT;

    int sockfd;
    if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, SOCKET_TIMEOUT)) {
        DFXLOG_ERROR("StartConnect failed");
        return -1;
    }

    write(sockfd, &request, sizeof(struct FaultLoggerdRequest));
    close(sockfd);
    return 0;
}