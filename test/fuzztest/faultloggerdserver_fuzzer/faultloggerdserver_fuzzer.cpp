/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "faultloggerdserver_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>

#include "dfx_exception.h"
#include "dfx_socket_request.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_socket.h"
#include "faultloggerd_test.h"
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
void SendMsgToServer(int32_t socketFd, const void* data, size_t msgLen)
{
    std::string socketNames[] = { SERVER_SOCKET_NAME, SERVER_SDKDUMP_SOCKET_NAME, SERVER_CRASH_SOCKET_NAME };
    srand(static_cast<unsigned>(time(nullptr)));
    int randomSocketIndex = rand() % 3;
    if (StartConnect(socketFd, socketNames[randomSocketIndex].c_str(), 0)) {
        SendMsgToSocket(socketFd, data, msgLen);
    }
}

bool GetFileDescriptorFromServer(int32_t socketFd, int32_t* fds, uint32_t nFds)
{
    int32_t retCode{ResponseCode::RECEIVE_DATA_FAILED};
    GetMsgFromSocket(socketFd, &retCode, sizeof (retCode));
    if (retCode == ResponseCode::REQUEST_SUCCESS) {
        return ReadFileDescriptorFromSocket(socketFd, fds, nFds);
    }
    return false;
}

void FillRequestHeadData(RequestDataHead& head, FaultLoggerClientType clientType)
{
    head.clientType = clientType;
    head.clientPid = getpid();
}
}

void FaultLoggerdClientTest(const uint8_t* data, size_t size)
{
    if (size < sizeof(FaultLoggerdRequest)) {
        return;
    }
    FaultLoggerdRequest request = *reinterpret_cast<const FaultLoggerdRequest*>(data);
    RequestFileDescriptor(request.type);
    int pipeReadFd[PIPE_NUM_SZ] = {-1, -1};
    RequestSdkDump(request.pid, request.tid, pipeReadFd);
    CloseFd(pipeReadFd[PIPE_BUF_INDEX]);
    CloseFd(pipeReadFd[PIPE_RES_INDEX]);
    RequestPipeFd(request.pid, request.type, pipeReadFd);
    CloseFd(pipeReadFd[PIPE_BUF_INDEX]);
    CloseFd(pipeReadFd[PIPE_RES_INDEX]);
    RequestDelPipeFd(request.pid);
    RequestFileDescriptorEx(&request);
}

void FaultLoggerdServerTest(const uint8_t* data, size_t size)
{
    if (size < sizeof(FaultLoggerdRequest)) {
        return;
    }
    SmartFd socketFd(CreateSocketFd());
    SendMsgToServer(socketFd, data, size);
}

void FileDesServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (FaultLoggerdRequest)) {
        return;
    }
    FaultLoggerdRequest requestData = *reinterpret_cast<const FaultLoggerdRequest*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    SmartFd socketFd(CreateSocketFd());
    SendMsgToServer(socketFd, &requestData, sizeof(FaultLoggerdRequest));
    int32_t fd = -1;
    if (GetFileDescriptorFromServer(socketFd, &fd, 1)) {
        CloseFd(fd);
    }
}

void ExceptionReportServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (CrashDumpException)) {
        return;
    }
    CrashDumpException requestData = *reinterpret_cast<const CrashDumpException*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    SmartFd socketFd(CreateSocketFd());
    SendMsgToServer(socketFd, &requestData, sizeof(CrashDumpException));
}

void DumpStatsServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (FaultLoggerdStatsRequest)) {
        return;
    }
    FaultLoggerdStatsRequest requestData = *reinterpret_cast<const FaultLoggerdStatsRequest*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    SmartFd socketFd(CreateSocketFd());
    SendMsgToServer(socketFd, &requestData, sizeof(FaultLoggerdStatsRequest));
}

void SdkDumpServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (SdkDumpRequestData)) {
        return;
    }
    SdkDumpRequestData requestData = *reinterpret_cast<const SdkDumpRequestData*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    SmartFd socketFd(CreateSocketFd());
    SendMsgToServer(socketFd, &requestData, sizeof(SdkDumpRequestData));
    int32_t fds[PIPE_NUM_SZ] = {-1, -1};
    if (GetFileDescriptorFromServer(socketFd, fds, PIPE_NUM_SZ)) {
        CloseFd(fds[PIPE_BUF_INDEX]);
        CloseFd(fds[PIPE_RES_INDEX]);
        RequestDelPipeFd(requestData.pid);
    }
}

void PipServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (PipFdRequestData)) {
        return;
    }
    PipFdRequestData requestData = *reinterpret_cast<const PipFdRequestData*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    SmartFd socketFd(CreateSocketFd());
    SendMsgToServer(socketFd, &requestData, sizeof(PipFdRequestData));
    int32_t fds[PIPE_NUM_SZ] = {-1, -1};
    if (GetFileDescriptorFromServer(socketFd, fds, PIPE_NUM_SZ)) {
        CloseFd(fds[PIPE_BUF_INDEX]);
        CloseFd(fds[PIPE_RES_INDEX]);
    }
}

void TempFileManagerTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (FaultLoggerdRequest)) {
        return;
    }
    const FaultLoggerdRequest& requestData = *reinterpret_cast<const FaultLoggerdRequest*>(data);
    if (requestData.head.clientType & 1) {
        SmartFd socketFd(OHOS::HiviewDFX::TempFileManager::CreateFileDescriptor(
            FaultLoggerType::CPP_CRASH, requestData.pid, requestData.tid, requestData.time));
    } else {
        SmartFd socketFd(OHOS::HiviewDFX::TempFileManager::CreateFileDescriptor(
            FaultLoggerType::JS_RAW_SNAPSHOT, requestData.pid, requestData.tid, requestData.time));
    }
}
} // namespace HiviewDFX
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FaultLoggerdTestServer::GetInstance();
    if (data == nullptr || size == 0) {
        return 0;
    }
    OHOS::HiviewDFX::FaultLoggerdClientTest(data, size);
    OHOS::HiviewDFX::FaultLoggerdServerTest(data, size);
    OHOS::HiviewDFX::FileDesServiceTest(data, size);
    OHOS::HiviewDFX::ExceptionReportServiceTest(data, size);
    OHOS::HiviewDFX::DumpStatsServiceTest(data, size);
    OHOS::HiviewDFX::SdkDumpServiceTest(data, size);
    OHOS::HiviewDFX::PipServiceTest(data, size);
    OHOS::HiviewDFX::TempFileManagerTest(data, size);
    return 0;
}
