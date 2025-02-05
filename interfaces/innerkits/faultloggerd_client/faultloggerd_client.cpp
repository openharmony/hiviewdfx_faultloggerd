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

#include <cstdint>
#include <cstdlib>
#include <functional>

#include <securec.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "dfx_util.h"
#include "faultloggerd_socket.h"
#include "smart_fd.h"

namespace {
constexpr const char* const FAULTLOGGERD_CLIENT_TAG = "FAULT_LOGGERD_CLIENT";
constexpr int32_t SOCKET_TIMEOUT = 1;
std::string GetSocketName()
{
    char content[NAME_BUF_LEN];
    GetProcessName(content, sizeof(content));
    return strcmp(content, "processdump") == 0 ? SERVER_CRASH_SOCKET_NAME : SERVER_SOCKET_NAME;
}

void FillRequestHeadData(RequestDataHead& head, FaultLoggerClientType clientType)
{
    head.clientType = clientType;
    head.clientPid = getpid();
}

int32_t SendRequestToSocket(int32_t sockFd, const std::string& socketName, const void* data, size_t dataSize)
{
    if (!StartConnect(sockFd, socketName.c_str(), SOCKET_TIMEOUT)) {
        return ResponseCode::CONNECT_FAILED;
    }
    if (!SendMsgToSocket(sockFd, data, dataSize)) {
        return ResponseCode::SEND_DATA_FAILED;
    }
    int32_t retCode{ResponseCode::RECEIVE_DATA_FAILED};
    GetMsgFromSocket(sockFd, &retCode, sizeof (retCode));
    return retCode;
}

int32_t SendRequestToServer(const std::string& socketName, const void* data, size_t dataSize,
    const std::function<int32_t(int32_t, int32_t)>& resultHandler = nullptr)
{
    OHOS::HiviewDFX::SmartFd sockFd = CreateSocketFd();
    int32_t retCode = SendRequestToSocket(sockFd, socketName, data, dataSize);
    if (resultHandler) {
        retCode = resultHandler(sockFd, retCode);
    }
    if (retCode != ResponseCode::REQUEST_SUCCESS) {
        DFXLOGE("%{public}s.%{public}s :: request failed and retCode %{public}d", FAULTLOGGERD_CLIENT_TAG,
            __func__, retCode);
    }
    return retCode;
}

int32_t RequestFileDescriptorFromServer(const std::string& socketName, const void* requestData,
    size_t requestSize, int* fds, uint32_t nFds)
{
    return SendRequestToServer(socketName, requestData, requestSize, [fds, nFds] (int32_t socketFd, int32_t retCode) {
        if (retCode != ResponseCode::REQUEST_SUCCESS) {
            return retCode;
        }
        return ReadFileDescriptorFromSocket(socketFd, fds, nFds) ? retCode : ResponseCode::RECEIVE_DATA_FAILED;
    });
}

int32_t RequestFileDescriptor(const struct FaultLoggerdRequest &request)
{
    int32_t fd{-1};
    switch (request.type) {
        case CPP_CRASH:
        case CPP_STACKTRACE:
        case LEAK_STACKTRACE:
        case JIT_CODE_LOG:
            RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, &request, sizeof(request), &fd, 1);
            return fd;
        default:
            RequestFileDescriptorFromServer(SERVER_SOCKET_NAME, &request, sizeof(request), &fd, 1);
            return fd;
    }
}
}

int32_t RequestFileDescriptor(int32_t type)
{
    struct FaultLoggerdRequest request{};
    FillRequestHeadData(request.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    request.type = type;
    request.pid = request.head.clientPid;
    request.tid = gettid();
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    return RequestFileDescriptor(request);
}

int32_t RequestFileDescriptorEx(struct FaultLoggerdRequest* request)
{
    if (request == nullptr) {
        DFXLOGE("%{public}s.%{public}s :: RequestFileDescriptorEx nullptr.", FAULTLOGGERD_CLIENT_TAG, __func__);
        return -1;
    }
    FillRequestHeadData(request->head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    return RequestFileDescriptor(*request);
}

int32_t RequestSdkDump(int32_t pid, int32_t tid, int (&pipeReadFd)[2], bool isJson, int timeout)
{
#ifndef is_ohos_lite
    DFXLOGI("%{public}s.%{public}s :: pid: %{public}d, tid: %{public}d, isJson: %{public}d.",
        FAULTLOGGERD_CLIENT_TAG, __func__, pid, tid, isJson);
    if (pid <= 0 || tid < 0) {
        return ResponseCode::DEFAULT_ERROR_CODE;
    }
    struct SdkDumpRequestData request{};
    FillRequestHeadData(request.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    request.sigCode = isJson ? DUMP_TYPE_REMOTE_JSON : DUMP_TYPE_REMOTE;
    request.pid = pid;
    request.tid = tid;
    request.callerTid = gettid();
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    request.endTime = GetAbsTimeMilliSeconds() + static_cast<uint64_t>(timeout);
    return RequestFileDescriptorFromServer(SERVER_SDKDUMP_SOCKET_NAME, &request,
        sizeof(request), pipeReadFd, PIPE_NUM_SZ);
#else
    return ResponseCode::DEFAULT_ERROR_CODE;
#endif
}

int32_t RequestPipeFd(int32_t pid, int32_t pipeType, int (&pipeFd)[2])
{
#ifndef is_ohos_lite
    if (pipeType < FaultLoggerPipeType::PIPE_FD_READ || pipeType > FaultLoggerPipeType::PIPE_FD_DELETE) {
        DFXLOGE("%{public}s.%{public}s :: pipeType: %{public}d failed.", FAULTLOGGERD_CLIENT_TAG, __func__, pipeType);
        return ResponseCode::DEFAULT_ERROR_CODE;
    }
    PipFdRequestData request{};
    FillRequestHeadData(request.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    request.pipeType = static_cast<int8_t>(pipeType);
    request.pid = pid;
    return RequestFileDescriptorFromServer(GetSocketName(), &request, sizeof(request), pipeFd, PIPE_NUM_SZ);
#else
    return ResponseCode::DEFAULT_ERROR_CODE;
#endif
}

int32_t RequestDelPipeFd(int32_t pid)
{
#ifndef is_ohos_lite
    PipFdRequestData request{};
    FillRequestHeadData(request.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    request.pid = pid;
    return SendRequestToServer(GetSocketName(), &request, sizeof(request));
#else
    return ResponseCode::DEFAULT_ERROR_CODE;
#endif
}

int32_t ReportDumpStats(struct FaultLoggerdStatsRequest *request)
{
#ifndef HISYSEVENT_DISABLE
    if (request == nullptr) {
        return ResponseCode::DEFAULT_ERROR_CODE;
    }
    FillRequestHeadData(request->head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    OHOS::HiviewDFX::SmartFd socketFd = CreateSocketFd();
    if (!StartConnect(socketFd, GetSocketName().c_str(), SOCKET_TIMEOUT)) {
        DFXLOGE("%{public}s.%{public}s :: failed to connect server.", FAULTLOGGERD_CLIENT_TAG, __func__);
        return ResponseCode::CONNECT_FAILED;
    }
    if (!SendMsgToSocket(socketFd, request, sizeof (FaultLoggerdStatsRequest))) {
        DFXLOGE("%{public}s.%{public}s :: failed to send msg to server.", FAULTLOGGERD_CLIENT_TAG, __func__);
        return ResponseCode::SEND_DATA_FAILED;
    }
#endif
    return ResponseCode::REQUEST_SUCCESS;
}