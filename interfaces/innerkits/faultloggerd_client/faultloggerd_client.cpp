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
#include "dfx_socket_request.h"

namespace {
constexpr int32_t SOCKET_TIMEOUT = 5;

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

template<typename REQ>
int32_t SendRequestToSocket(const SmartFd& sockFd, const REQ& requestData)
{
    if (!SendMsgToSocket(sockFd, &requestData, sizeof (REQ))) {
        return ResponseCode::SEND_DATA_FAILED;
    }
    int32_t responseCode{ResponseCode::RECEIVE_DATA_FAILED};
    if (!GetMsgFromSocket(sockFd, &responseCode, sizeof (responseCode))) {
        return ResponseCode::RECEIVE_DATA_FAILED;
    }
    if (responseCode != ResponseCode::REQUEST_SUCCESS) {
        DFXLOGE("[%{public}d]: failed to request for clientType %{public}d and response %{public}d",
                __LINE__, requestData.head.clientType, responseCode);
    }
    return responseCode;
}

template<typename REQ>
int32_t SendRequestToServer(const std::string& socketName, const REQ& requestData, int timeout)
{
    SmartFd sockFd(GetConnectSocketFd(socketName.c_str(), timeout));
    if (sockFd < 0) {
        return ResponseCode::CONNECT_FAILED;
    }
    return SendRequestToSocket(sockFd, requestData);
}

template<typename REQ>
int32_t RequestFileDescriptorFromServer(const std::string& socketName, const REQ& requestData, const int timeout)
{
    SmartFd sockFd(GetConnectSocketFd(socketName.c_str(), timeout));
    if (SendRequestToSocket(sockFd, requestData) == ResponseCode::REQUEST_SUCCESS) {
        return ReadFileDescriptorFromSocket(sockFd);
    }
    return -1;
}

int32_t RequestFileDescriptor(const struct FaultLoggerdRequest &request)
{
    switch (request.type) {
        case CPP_CRASH:
        case CPP_STACKTRACE:
        case LEAK_STACKTRACE:
        case JIT_CODE_LOG:
            return RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, request, SOCKET_TIMEOUT);
        default:
            return RequestFileDescriptorFromServer(SERVER_SOCKET_NAME, request, SOCKET_TIMEOUT);
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

int32_t RequestFileDescriptorEx(struct FaultLoggerdRequest *request)
{
    if (request == nullptr) {
        DFXLOGE("[%{public}d]: nullptr request", __LINE__);
        return -1;
    }
    FillRequestHeadData(request->head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    return RequestFileDescriptor(*request);
}

int RequestSdkDump(int32_t pid, int32_t tid, int (&pipeReadFd)[2], bool isJson, int timeout)
{
    DFXLOGI("RequestSdkDump :: pid(%{public}d), tid(%{public}d), isJson(%{public}s).",
        pid, tid, isJson ? "true" : "false");
#ifndef is_ohos_lite
    if (pid <= 0 || tid < 0) {
        return -1;
    }

    struct SdkDumpRequestData request{};
    FillRequestHeadData(request.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    request.isJson = isJson;
    request.sigCode = DUMP_TYPE_REMOTE;
    request.pid = pid;
    request.tid = tid;
    request.callerTid = gettid();
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    request.endTime = GetAbsTimeMilliSeconds() + static_cast<uint64_t>(timeout);
    return SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, request, SOCKET_TIMEOUT);
#else
    return -1;
#endif
}

int RequestPipeFd(int32_t pid, int32_t pipeType, int (&pipeFd)[2])
{
#ifndef is_ohos_lite
    if (pipeType < FaultLoggerPipeType::PIPE_FD_READ_BUF || pipeType > FaultLoggerPipeType::PIPE_FD_DELETE) {
        DFXLOGE("%{public}s :: pipeType(%{public}d) failed.", __func__, pipeType);
        return -1;
    }
    PipFdRequestData request{};
    FillRequestHeadData(request.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    request.pipeType = static_cast<int8_t>(pipeType);
    request.pid = pid;
    return RequestFileDescriptorFromServer(GetSocketName(), request, SOCKET_TIMEOUT);
#else
    return -1;
#endif
}

int32_t RequestDelPipeFd(int32_t pid)
{
#ifndef is_ohos_lite
    PipFdRequestData request{};
    FillRequestHeadData(request.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    request.pid = pid;
    return SendRequestToServer(GetSocketName(), request, SOCKET_TIMEOUT);
#else
    return -1;
#endif
}

int ReportDumpStats(struct FaultLoggerdStatsRequest *request)
{
#ifndef HISYSEVENT_DISABLE
    if (request == nullptr) {
        return -1;
    }
    FillRequestHeadData(request->head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    SmartFd sockFd(GetConnectSocketFd(GetSocketName().c_str(), SOCKET_TIMEOUT));
    if (!SendMsgToSocket(sockFd, request, sizeof (FaultLoggerdStatsRequest))) {
        DFXLOGE("ReportDumpCatcherStats: failed to write stats.");
        return -1;
    }
#endif
    return 0;
}