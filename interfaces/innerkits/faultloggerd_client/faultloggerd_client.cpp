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
#include "faultloggerd_client.h"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <securec.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "faultloggerd_socket.h"

static const int32_t SOCKET_TIMEOUT = 5;

static std::string GetSocketConnectionName()
{
    char content[NAME_BUF_LEN];
    GetProcessName(content, sizeof(content));
    if (std::string(content).find("processdump") != std::string::npos) {
        return std::string(SERVER_CRASH_SOCKET_NAME);
    }
    return std::string(SERVER_SOCKET_NAME);
}

int32_t RequestFileDescriptor(int32_t type)
{
    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.type = type;
    request.pid = getpid();
    request.tid = gettid();
    request.uid = getuid();
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    return RequestFileDescriptorEx(&request);
}

int32_t RequestLogFileDescriptor(struct FaultLoggerdRequest *request)
{
    if (request == nullptr) {
        DFXLOGE("[%{public}d]: nullptr request", __LINE__);
        return -1;
    }
    request->clientType = (int32_t)FaultLoggerClientType::LOG_FILE_DES_CLIENT;
    return RequestFileDescriptorEx(request);
}

int32_t RequestFileDescriptorEx(const struct FaultLoggerdRequest *request)
{
    if (request == nullptr) {
        DFXLOGE("[%{public}d]: nullptr request", __LINE__);
        return -1;
    }

    int sockfd;
    std::string name = GetSocketConnectionName();
    if (!StartConnect(sockfd, name.c_str(), SOCKET_TIMEOUT)) {
        DFXLOGE("[%{public}d]: StartConnect(%{public}d) failed", __LINE__, sockfd);
        return -1;
    }

    OHOS_TEMP_FAILURE_RETRY(write(sockfd, request, sizeof(struct FaultLoggerdRequest)));
    int fd = ReadFileDescriptorFromSocket(sockfd);
    DFXLOGD("RequestFileDescriptorEx(%{public}d).", fd);
    close(sockfd);
    return fd;
}

static bool CheckReadResp(int sockfd)
{
    char ControlBuffer[SOCKET_BUFFER_SIZE] = {0};
    if (OHOS_TEMP_FAILURE_RETRY(read(sockfd, ControlBuffer, sizeof(ControlBuffer) - 1)) !=
        strlen(FAULTLOGGER_DAEMON_RESP)) {
        DFXLOGE("read count not equal expect count, %{public}d", errno);
        return false;
    }
    if (std::string(ControlBuffer) != FAULTLOGGER_DAEMON_RESP) {
        DFXLOGE("response is not complete");
        return false;
    }
    return true;
}

static void RequestPipeFdEx(const struct FaultLoggerdRequest *request, int (&pipeFd)[2], bool needCheck)
{
    if (request == nullptr) {
        DFXLOGE("[%{public}d]: nullptr request", __LINE__);
        return;
    }
    int sockfd = -1;
    do {
        std::string name = GetSocketConnectionName();
        if (!StartConnect(sockfd, name.c_str(), SOCKET_TIMEOUT)) {
            DFXLOGE("[%{public}d]: StartConnect(%{public}d) failed", __LINE__, sockfd);
            return;
        }
        OHOS_TEMP_FAILURE_RETRY(write(sockfd, request, sizeof(struct FaultLoggerdRequest)));
        if (needCheck) {
            if (!CheckReadResp(sockfd)) {
                break;
            }
            int data = 12345;
            if (!SendMsgIovToSocket(sockfd, reinterpret_cast<void *>(&data), sizeof(data))) {
                DFXLOGE("%{public}s :: Failed to sendmsg.", __func__);
                break;
            }
        }
        size_t len = sizeof(pipeFd);
        int replyCode = 0;
        if (!RecvMsgFromSocket(sockfd, reinterpret_cast<void *>(pipeFd), len, replyCode) || len != sizeof(pipeFd)) {
            DFXLOGE("RecvMsgFromSocket failed, errno(%{public}d)", errno);
        }
    } while (false);
    OHOS::HiviewDFX::CloseFd(sockfd);
}

static int RecvPipeReadFd(int sockfd, int (&pipeReadFd)[2])
{
    int32_t mRsp = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;

    int data = 12345;
    if (!SendMsgIovToSocket(sockfd, reinterpret_cast<void *>(&data), sizeof(data))) {
        DFXLOGE("%{public}s :: Failed to sendmsg.", __func__);
        return mRsp;
    }
    size_t len = sizeof(pipeReadFd);
    if (!RecvMsgFromSocket(sockfd, reinterpret_cast<void *>(pipeReadFd), len, mRsp)) {
        DFXLOGE("%{public}s :: Failed to recv message", __func__);
        return FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;
    }
    if (len != sizeof(pipeReadFd)) {
        DFXLOGE("data is null or len is %{public}zu", len);
        return FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;
    }
    return mRsp;
}

bool CheckConnectStatus()
{
    int sockfd = -1;
    std::string name = GetSocketConnectionName();
    if (StartConnect(sockfd, name.c_str(), SOCKET_TIMEOUT)) {
        close(sockfd);
        return true;
    }
    return false;
}

static int SendSdkDumpRequestToServer(const FaultLoggerdRequest &request, int (&pipeReadFd)[2])
{
    int sockfd = -1;
    int resRsp = FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT;
    do {
        std::string name = GetSocketConnectionName();
        if (request.clientType == FaultLoggerClientType::SDK_DUMP_CLIENT) {
            name = std::string(SERVER_SDKDUMP_SOCKET_NAME);
        }

        if (!StartConnect(sockfd, name.c_str(), SOCKET_TIMEOUT)) {
            DFXLOGE("[%{public}d]: StartConnect(%{public}d) failed", __LINE__, sockfd);
            resRsp = static_cast<int>(SDK_CONNECT_FAIL);
            return resRsp;
        }
        if (OHOS_TEMP_FAILURE_RETRY(write(sockfd, &request,
            sizeof(struct FaultLoggerdRequest))) != static_cast<long>(sizeof(request))) {
            DFXLOGE("write failed.");
            resRsp = static_cast<int>(SDK_WRITE_FAIL);
            break;
        }

        if (!CheckReadResp(sockfd)) {
            break;
        }
        resRsp = RecvPipeReadFd(sockfd, pipeReadFd);
    } while (false);
    if (resRsp != FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS) {
        OHOS::HiviewDFX::CloseFd(pipeReadFd[PIPE_BUF_INDEX]);
        OHOS::HiviewDFX::CloseFd(pipeReadFd[PIPE_RES_INDEX]);
    }
    OHOS::HiviewDFX::CloseFd(sockfd);
    DFXLOGI("SendSdkDumpRequestToServer :: resRsp(%{public}d).", resRsp);
    return resRsp;
}

int RequestSdkDump(int32_t pid, int32_t tid, int (&pipeReadFd)[2], bool isJson, int timeout)
{
    DFXLOGI("RequestSdkDump :: pid(%{public}d), tid(%{public}d), isJson(%{public}s).",
        pid, tid, isJson ? "true" : "false");
    if (pid <= 0 || tid < 0) {
        return -1;
    }

    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.sigCode = isJson ? DUMP_TYPE_REMOTE_JSON : DUMP_TYPE_REMOTE;
    request.pid = pid;
    request.tid = tid;
    request.callerPid = getpid();
    request.callerTid = gettid();
    request.clientType = (int32_t)FaultLoggerClientType::SDK_DUMP_CLIENT;
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    request.endTime = GetAbsTimeMilliSeconds() + static_cast<uint64_t>(timeout);
    pipeReadFd[PIPE_BUF_INDEX] = -1;
    pipeReadFd[PIPE_RES_INDEX] = -1;
    return SendSdkDumpRequestToServer(request, pipeReadFd);
}

int RequestPrintTHilog(const char *msg, int length)
{
    if (length >= LINE_BUF_SIZE) {
        return -1;
    }

    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.clientType = (int32_t)FaultLoggerClientType::PRINT_T_HILOG_CLIENT;
    request.pid = getpid();
    request.uid = getuid();
    int sockfd = -1;
    do {
        std::string name = GetSocketConnectionName();
        if (!StartConnect(sockfd, name.c_str(), SOCKET_TIMEOUT)) {
            DFXLOGE("[%{public}d]: StartConnect(%{public}d) failed", __LINE__, sockfd);
            return -1;
        }

        if (OHOS_TEMP_FAILURE_RETRY(write(sockfd, &request,
            sizeof(struct FaultLoggerdRequest))) != static_cast<long>(sizeof(request))) {
            break;
        }

        if (!CheckReadResp(sockfd)) {
            break;
        }

        int nwrite = OHOS_TEMP_FAILURE_RETRY(write(sockfd, msg, strlen(msg)));
        if (nwrite != static_cast<long>(strlen(msg))) {
            DFXLOGE("nwrite: %{public}d.", nwrite);
            break;
        }
        close(sockfd);
        return 0;
    } while (false);
    close(sockfd);
    return -1;
}

int RequestPipeFd(int32_t pid, int32_t pipeType, int (&pipeFd)[2])
{
    if (pipeType < static_cast<int32_t>(FaultLoggerPipeType::PIPE_FD_READ) ||
        pipeType > static_cast<int32_t>(FaultLoggerPipeType::PIPE_FD_WRITE)) {
        DFXLOGE("%{public}s :: pipeType(%{public}d) failed.", __func__, pipeType);
        return -1;
    }
    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(struct FaultLoggerdRequest));

    request.pipeType = pipeType;
    request.pid = pid;
    request.callerPid = getpid();
    request.callerTid = gettid();
    request.clientType = (int32_t)FaultLoggerClientType::PIPE_FD_CLIENT;
    bool needCheck = pipeType == static_cast<int32_t>(FaultLoggerPipeType::PIPE_FD_READ);
    pipeFd[PIPE_BUF_INDEX] = -1;
    pipeFd[PIPE_RES_INDEX] = -1;
    RequestPipeFdEx(&request, pipeFd, needCheck);
    if (pipeFd[PIPE_BUF_INDEX] > 0 && pipeFd[PIPE_RES_INDEX] > 0) {
        return 0;
    }
    OHOS::HiviewDFX::CloseFd(pipeFd[PIPE_BUF_INDEX]);
    OHOS::HiviewDFX::CloseFd(pipeFd[PIPE_RES_INDEX]);
    return -1;
}

int32_t RequestDelPipeFd(int32_t pid)
{
    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(struct FaultLoggerdRequest));
    request.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    request.pid = pid;
    request.clientType = (int32_t)FaultLoggerClientType::PIPE_FD_CLIENT;

    int sockfd;
    std::string name = GetSocketConnectionName();
    if (!StartConnect(sockfd, name.c_str(), SOCKET_TIMEOUT)) {
        DFXLOGE("[%{public}d]: StartConnect(%{public}d) failed", __LINE__, sockfd);
        return -1;
    }

    OHOS_TEMP_FAILURE_RETRY(write(sockfd, &request, sizeof(struct FaultLoggerdRequest)));
    close(sockfd);
    return 0;
}

int ReportDumpStats(const struct FaultLoggerdStatsRequest *request)
{
    int sockfd = -1;
    std::string name = GetSocketConnectionName();
    if (!StartConnect(sockfd, name.c_str(), SOCKET_TIMEOUT)) {
        DFXLOGE("[%{public}d]: StartConnect(%{public}d) failed", __LINE__, sockfd);
        return -1;
    }

    if (OHOS_TEMP_FAILURE_RETRY(write(sockfd, request,
        sizeof(struct FaultLoggerdStatsRequest))) != static_cast<long int>(sizeof(struct FaultLoggerdStatsRequest))) {
        DFXLOGE("ReportDumpCatcherStats: failed to write stats.");
        close(sockfd);
        return -1;
    }
    close(sockfd);
    return 0;
}
