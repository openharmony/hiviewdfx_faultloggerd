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

    struct timeval time;
    (void)gettimeofday(&time, nullptr);

    request->type = type;
    request->pid = getpid();
    request->tid = gettid();
    request->uid = getuid();
    request->time = (static_cast<uint64_t>(time.tv_sec) * 1000) + // 1000 : second to millsecond convert ratio
        (static_cast<uint64_t>(time.tv_usec) / 1000); // 1000 : microsecond to millsecond convert ratio
    ReadStringFromFile("/proc/self/cmdline", request->module, sizeof(request->module));
}

int32_t RequestFileDescriptor(int32_t type)
{
    struct FaultLoggerdRequest request;
    errno_t err = memset_s(&request, sizeof(request), 0, sizeof(request));
    if (err != EOK) {
        DfxLogError("%s :: memset_s request failed..", __func__);
    }
    FillRequest(type, &request);
    return RequestFileDescriptorEx(&request);
}

int32_t RequestLogFileDescriptor(struct FaultLoggerdRequest *request)
{
    request->clientType = (int32_t)FaultLoggerClientType::LOG_FILE_DES_CLIENT;
    return RequestFileDescriptorEx(request);
}

int32_t RequestPipeFd(int32_t pid, int32_t pipeType)
{
    struct FaultLoggerdRequest request;
    if (memset_s(&request, sizeof(request), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        DfxLogError("%s :: memset_s request failed..", __func__);
    }
    request.pipeType = pipeType;
    request.pid = pid;

    request.clientType = (int32_t)FaultLoggerClientType::PIPE_FD_CLIENT;
    return RequestFileDescriptorEx(&request);
}

int32_t RequestFileDescriptorEx(const struct FaultLoggerdRequest *request)
{
    if (request == nullptr) {
        DfxLogError("nullptr request");
        return -1;
    }

    int sockfd;
    const int32_t SOCKET_TIMEOUT = 5;
    if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, SOCKET_TIMEOUT)) {
        DfxLogError("StartConnect failed");
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

    int data = 12345;
    if (!SendMsgIovToSocket(sockfd, reinterpret_cast<void *>(&data), sizeof(data))) {
        DfxLogError("%s :: Failed to sendmsg.", __func__);
        return mRsp;
    }

    char recvbuf[SOCKET_BUFFER_SIZE] = {'\0'};
    ssize_t count = recv(sockfd, recvbuf, sizeof(recvbuf), 0);
    if (count < 0) {
        DfxLogError("%s :: Failed to recv.", __func__);
        return mRsp;
    }
    
    mRsp = (FaultLoggerCheckPermissionResp)atoi(recvbuf);
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

static bool SendRequestToServer(const FaultLoggerdRequest &request)
{
    int sockfd = -1;
    bool resRsp = false;
    do {
        if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, -1)) {
            DfxLogError("StartConnect failed.");
            break;
        }

        if (write(sockfd, &request, sizeof(struct FaultLoggerdRequest)) != static_cast<long>(sizeof(request))) {
            break;
        }

        char ControlBuffer[SOCKET_BUFFER_SIZE];
        errno_t err = memset_s(&ControlBuffer, sizeof(ControlBuffer), 0, SOCKET_BUFFER_SIZE);
        if (err != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)err);
            break;
        }
		
        int nread = read(sockfd, ControlBuffer, sizeof(ControlBuffer) - 1);
        if (nread != (ssize_t)strlen(FAULTLOGGER_DAEMON_RESP)) {
            DfxLogError("nread: %d.", nread);
            break;
        }

        FaultLoggerCheckPermissionResp mRsp = SendUidToServer(sockfd);
        if ((FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS == mRsp)
                || (FaultLoggerSdkDumpResp::SDK_DUMP_PASS == (FaultLoggerSdkDumpResp)mRsp)) {
            resRsp = true;
        }
    } while (false);

    close(sockfd);
    DfxLogInfo("SendRequestToServer :: resRsp(%d).", resRsp);
    return resRsp;
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
        DfxLogError("%s :: memset_s request failed..", __func__);
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
        DfxLogError("%s :: memset_s request failed..", __func__);
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
        DfxLogError("%s :: memset_s request failed..", __func__);
    }
    request.clientType = (int32_t)FaultLoggerClientType::PRINT_T_HILOG_CLIENT;

    int sockfd = -1;
    do {
        if (!StartConnect(sockfd, FAULTLOGGERD_SOCK_PATH, -1)) {
            DfxLogError("StartConnect failed");
            break;
        }

        if (write(sockfd, &request, sizeof(struct FaultLoggerdRequest)) != static_cast<long>(sizeof(request))) {
            break;
        }

        char ControlBuffer[SOCKET_BUFFER_SIZE];
        errno_t err = memset_s(&ControlBuffer, sizeof(ControlBuffer), 0, SOCKET_BUFFER_SIZE);
        if (err != EOK) {
            DfxLogError("memset_s failed, err = %d.", (int)err);
            break;
        }
        if (read(sockfd, ControlBuffer, sizeof(ControlBuffer) - 1) != \
            static_cast<long>(strlen(FAULTLOGGER_DAEMON_RESP))) {
            break;
        }
		
        int nwrite = write(sockfd, msg, strlen(msg));
        if (nwrite != static_cast<long>(strlen(msg))) {
            DfxLogError("nwrite: %d.", nwrite);
            break;
        }
    } while (false);
    close(sockfd);
}

