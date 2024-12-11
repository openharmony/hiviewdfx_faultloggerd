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
#include <unistd.h>
#include <sys/socket.h>

#include "dfx_exception.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_test.h"
#include "dfx_socket_request.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
int32_t GetRandomSocketFd()
{
    std::string socketNames[] = { SERVER_SOCKET_NAME, SERVER_SDKDUMP_SOCKET_NAME, SERVER_CRASH_SOCKET_NAME };
    srand(static_cast<unsigned>(time(nullptr)));
    int randomSocketIndex = rand() % 3;
    return GetConnectSocketFd(socketNames[randomSocketIndex].c_str(), 0);
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
    RequestSdkDumpJson(request.pid, request.tid);
    RequestPipeFd(request.pid, request.type);
    RequestDelPipeFd(request.pid);
    RequestFileDescriptorEx(&request);
}

void FaultLoggerdServerTest(const uint8_t* data, size_t size)
{
    if (size < sizeof(FaultLoggerdRequest)) {
        return;
    }
    SmartFd socketFd(GetRandomSocketFd());
    SendMsgToSocket(socketFd, data, size);
}

void FileDesServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (FaultLoggerdRequest)) {
        return;
    }
    FaultLoggerdRequest requestData = *reinterpret_cast<const FaultLoggerdRequest*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    SmartFd socketFd(GetRandomSocketFd());
    SendMsgToSocket(socketFd, &requestData, sizeof(FaultLoggerdRequest));
}

void ExceptionReportServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (CrashDumpException)) {
        return;
    }
    CrashDumpException requestData = *reinterpret_cast<const CrashDumpException*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    SmartFd socketFd(GetRandomSocketFd());
    SendMsgToSocket(socketFd, &requestData, sizeof(CrashDumpException));
}

void DumpStatsServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (FaultLoggerdStatsRequest)) {
        return;
    }
    FaultLoggerdStatsRequest requestData = *reinterpret_cast<const FaultLoggerdStatsRequest*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    SmartFd socketFd(GetRandomSocketFd());
    SendMsgToSocket(socketFd, &requestData, sizeof(FaultLoggerdStatsRequest));
}

void SdkDumpServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (SdkDumpRequestData)) {
        return;
    }
    SdkDumpRequestData requestData = *reinterpret_cast<const SdkDumpRequestData*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    SmartFd socketFd(GetRandomSocketFd());
    SendMsgToSocket(socketFd, &requestData, sizeof(SdkDumpRequestData));
}

void PipServiceTest(const uint8_t* data, size_t size)
{
    if (size < sizeof (PipFdRequestData)) {
        return;
    }
    PipFdRequestData requestData = *reinterpret_cast<const PipFdRequestData*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    SmartFd socketFd(GetRandomSocketFd());
    SendMsgToSocket(socketFd, &requestData, sizeof(PipFdRequestData));
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
