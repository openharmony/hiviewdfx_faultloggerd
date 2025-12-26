/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "faultlog_client.h"

#include <errno.h>
#include <fcntl.h>
#include <securec.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxFaultlogClient"
#endif

static const int32_t CRASHDUMP_SOCKET_TIMEOUT = 3;
#define SOCKET_BUFFER_SIZE 32

static bool ConcatenateSocketName(char* dst, uint32_t dstSize, const char* src, const uint32_t srcSize)
{
    if (dstSize <= srcSize) {
        return false;
    }
    int err = strncpy_s(dst, dstSize, src, srcSize);
    if (err != EOK) {
        DFXLOGE("%{public}s :: strncpy failed, err = %{public}d.", __func__, err);
        return false;
    }
    return true;
}

static bool StartConnect(const char* socketName, const int timeout, int* sockfd)
{
    if (socketName == NULL) {
        DFXLOGE("socketName null");
        return false;
    }

    if ((*sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        DFXLOGE("%{public}s :: Failed to socket, errno(%{public}d)", __func__, errno);
        return false;
    }

    if (timeout > 0) {
        struct timeval timev = { timeout, 0 };
        if (OHOS_TEMP_FAILURE_RETRY(setsockopt(*sockfd, SOL_SOCKET, SO_RCVTIMEO, &timev, sizeof(timev))) != 0) {
            DFXLOGE("setsockopt(%{public}d) SO_RCVTIMEO error, errno(%{public}d).", *sockfd, errno);
        }
        if (OHOS_TEMP_FAILURE_RETRY(setsockopt(*sockfd, SOL_SOCKET, SO_SNDTIMEO, &timev, sizeof(timev))) != 0) {
            DFXLOGE("setsockopt(%{public}d) SO_SNDTIMEO error, errno(%{public}d).", *sockfd, errno);
        }
    }

    struct sockaddr_un server;
    server.sun_family = AF_LOCAL;
    uint32_t baseLen = (uint32_t)strlen(FAULTLOGGERD_SOCK_BASE_PATH);
    uint32_t dstSize = (uint32_t)sizeof(server.sun_path);
    if (!ConcatenateSocketName(server.sun_path, dstSize, FAULTLOGGERD_SOCK_BASE_PATH, baseLen)) {
        return false;
    }
    if (!ConcatenateSocketName(server.sun_path + baseLen, dstSize - baseLen, socketName, strlen(socketName))) {
        return false;
    }

    int len = (int)(offsetof(struct sockaddr_un, sun_path) + strlen(server.sun_path) + 1);
    int connected = OHOS_TEMP_FAILURE_RETRY(connect(*sockfd, (struct sockaddr *)(&server), len));
    if (connected < 0) {
        DFXLOGE("%{public}s :: connect failed, errno = %{public}d.", __func__, errno);
        return false;
    }
    return true;
}

static void CloseSocket(int sockfd)
{
    if (sockfd >= 0) {
        close(sockfd);
    }
}

static bool RecvMsgFromSocket(const int sockfd, unsigned char* data, unsigned int dataSize, size_t* len)
{
    if ((sockfd < 0) || (data == NULL)) {
        return false;
    }

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
        DFXLOGE("%{public}s :: Failed to recv message, errno(%{public}d)", __func__, errno);
        return false;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
    if (cmsg == NULL) {
        DFXLOGE("%{public}s :: Invalid message", __func__);
        return false;
    }

    unsigned char* src = CMSG_DATA(cmsg);
    if (src == NULL) {
        DFXLOGE("%{public}s :: Invalid cmsg data", __func__);
        return false;
    }

    *len = cmsg->cmsg_len - sizeof(struct cmsghdr);
    if (memcpy_s(data, dataSize, src, *len) != 0) {
        DFXLOGE("%{public}s :: memcpy error", __func__);
        return false;
    }
    return true;
}

static bool ReadFileDescriptorFromSocket(int sockfd, int* fd)
{
    size_t dataLen = sizeof(int);
    unsigned char data[dataLen + 1];
    if (!RecvMsgFromSocket(sockfd, data, sizeof(data), &dataLen)) {
        DFXLOGE("%{public}s :: Failed to recv message", __func__);
        return false;
    }

    if (dataLen != sizeof(int)) {
        DFXLOGE("%{public}s :: data is null or len is error", __func__);
        return false;
    }
    *fd = *((int *)data);
    DFXLOGD("%{public}s :: fd: %{public}d", __func__, *fd);
    return true;
}

static int32_t RequestServer(const void *buf, int len, int* fd)
{
    int32_t retCode = DEFAULT_ERROR_CODE;
    if (buf == NULL || len <= 0) {
        DFXLOGE("request null");
        return retCode;
    }

    int sockfd = -1;
    if (!StartConnect(SERVER_SOCKET_NAME, CRASHDUMP_SOCKET_TIMEOUT, &sockfd)) {
        DFXLOGE("StartConnect(%{public}d) failed", sockfd);
        CloseSocket(sockfd);
        return CONNECT_FAILED;
    }

    if (OHOS_TEMP_FAILURE_RETRY(write(sockfd, buf, len)) != (ssize_t)len) {
        DFXLOGE("Failed to write request message to socket, errno(%{public}d).", errno);
        CloseSocket(sockfd);
        return SEND_DATA_FAILED;
    }

    retCode = RECEIVE_DATA_FAILED;
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(sockfd, &retCode, sizeof(retCode)));
    if (nread <= 0) {
        DFXLOGE("Failed to get message from socket, %{public}zd, errno(%{public}d). ", nread, errno);
    }

    if (retCode != REQUEST_SUCCESS || fd == NULL) {
        CloseSocket(sockfd);
        return retCode;
    }

    if (!ReadFileDescriptorFromSocket(sockfd, fd)) {
        retCode = RECEIVE_DATA_FAILED;
    } else {
        retCode = REQUEST_SUCCESS;
    }
    CloseSocket(sockfd);
    return retCode;
}

int32_t RequestLimitedPipeFd(const int32_t pipeType, int* pipeFd, int pid, const char* procName)
{
#ifndef is_ohos_lite
    if (pipeType < PIPE_FD_READ || pipeType > PIPE_FD_DELETE) {
        DFXLOGE("%{public}s :: pipeType: %{public}d failed.", __func__, pipeType);
        return DEFAULT_ERROR_CODE;
    }
    DFXLOGI("%{public}s :: pipeType: %{public}d.", __func__, pipeType);
    LiteDumpFdRequestData request;
    request.head.clientType = PIPE_FD_LIMITED_CLIENT;
    request.head.clientPid = getpid();
    request.pipeType = (int8_t)pipeType;
    request.pid = pid;
    if (procName == NULL) {
        request.processName[0] = '\0';
    } else {
        memcpy_s(request.processName, sizeof(request.processName), procName, NAME_BUF_LEN);
        request.processName[sizeof(request.processName) - 1] = '\0';
    }
    int requestLen = (int)sizeof(struct LiteDumpFdRequestData);
    return RequestServer(&request, requestLen, pipeFd);
#else
    return DEFAULT_ERROR_CODE;
#endif
}

int32_t RequestLimitedDelPipeFd(int pid)
{
#ifndef is_ohos_lite
    LiteDumpFdRequestData  request;
    request.head.clientType = PIPE_FD_LIMITED_CLIENT;
    request.head.clientPid = getpid();
    request.pipeType = PIPE_FD_DELETE;
    request.pid = pid;
    int requestLen = (int)sizeof(struct LiteDumpFdRequestData);
    return RequestServer(&request, requestLen, NULL);
#else
    return DEFAULT_ERROR_CODE;
#endif
}

int32_t RequestLimitedProcessDump(int pid)
{
#ifndef is_ohos_lite
    FaultLoggerdRequest request;
    request.head.clientType = LIMITED_PROCESS_DUMP_CLIENT;
    request.head.clientPid = getpid();
    request.pid = pid;
    int requestLen = (int)sizeof(struct FaultLoggerdRequest);
    return RequestServer(&request, requestLen, NULL);
#else
    return DEFAULT_ERROR_CODE;
#endif
}
