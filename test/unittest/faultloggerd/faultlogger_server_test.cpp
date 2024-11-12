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

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <filesystem>
#include <securec.h>

#include "dfx_exception.h"
#include "dfx_util.h"
#include "dfx_define.h"
#include "dfx_socket_request.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_socket.h"
#include "faultloggerd_test.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr int32_t SOCKET_TIMEOUT = 5;
void FillRequestHeadData(RequestDataHead& head, int8_t clientType)
{
    head.clientType = clientType;
    head.clientPid = getpid();
}

template<typename REQ>
void SendRequestToServer(const std::string& socketName, const REQ& requestData, int32_t& responseData)
{
    SmartFd sockFd(GetConnectSocketFd(socketName.c_str(), SOCKET_TIMEOUT));
    SendMsgToSocket(sockFd, &requestData, sizeof (requestData));
    GetMsgFromSocket(sockFd, &responseData, sizeof (responseData));
}

template<typename REQ>
int32_t RequestFileDescriptorFromServer(const std::string& socketName, const REQ& requestData, int32_t& responseData)
{
    SmartFd sockFd(GetConnectSocketFd(socketName.c_str(), SOCKET_TIMEOUT));
    SendMsgToSocket(sockFd, &requestData, sizeof (requestData));
    GetMsgFromSocket(sockFd, &responseData, sizeof (responseData));
    return responseData == 0 ? ReadFileDescriptorFromSocket(sockFd) : -1;
}
}

class FaultLoggerdServiceTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
};

void FaultLoggerdServiceTest::TearDownTestCase()
{
    ClearTempFiles();
}

void FaultLoggerdServiceTest::SetUpTestCase()
{
    FaultLoggerdTestServer::GetInstance();
}

#ifndef is_ohos_lite
/**
 * @tc.name: FaultloggerdClientTest001
 * @tc.desc: request a file descriptor for logging crash
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpClientTest01, TestSize.Level2)
{
    SdkDumpRequestData requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    requestData.pid = requestData.head.clientPid;
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode ::REQUEST_SUCCESS);
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::SDK_DUMP_REPEAT);
}

/**
 * @tc.name: SdkDumpClientTest02
 * @tc.desc: request sdk dump with invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpClientTest02, TestSize.Level2)
{
    SdkDumpRequestData requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    requestData.pid = 0;
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);

    requestData.pid = requestData.head.clientPid;
    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);
}
#endif

/**
 * @tc.name: LogFileDesClientTest01
 * @tc.desc: request a file descriptor for logging crash
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LogFileDesClientTest01, TestSize.Level2)
{
    FaultLoggerdRequest requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    requestData.type = FaultLoggerType::CPP_CRASH;
    requestData.pid = requestData.head.clientPid;
    SmartFd fileFd(RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData));
    ASSERT_GE(fileFd, 0);
}

/**
 * @tc.name: LogFileDesClientTest02
 * @tc.desc: request a file descriptor through a socket not matched.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LogFileDesClientTest02, TestSize.Level2)
{
    FaultLoggerdRequest requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    requestData.type = FaultLoggerType::CPP_CRASH;
    requestData.pid = requestData.head.clientPid;
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: LogFileDesClientTest03
 * @tc.desc: request a file descriptor with wrong type.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LogFileDesClientTest03, TestSize.Level2)
{
    FaultLoggerdRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    requestData.type = FaultLoggerType::JIT_CODE_LOG;
    requestData.pid = requestData.head.clientPid;
    int32_t responseData(-1);
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);
}

#ifndef is_ohos_lite
/**
 * @tc.name: SdkDumpClientTest04
 * @tc.desc: request sdk dumpJson after request a fd for cppcrash.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpClientTest04, TestSize.Level2)
{
    SdkDumpRequestData requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    requestData.pid = requestData.head.clientPid;
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::SDK_PROCESS_CRASHED);
}

/**
 * @tc.name: PipeFdClientTest01
 * @tc.desc: request a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, PipeFdClientTest01, TestSize.Level2)
{
    PipFdRequestData requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    requestData.pid = requestData.head.clientPid;

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_READ_BUF;
    SmartFd readBuff(RequestFileDescriptorFromServer(SERVER_SOCKET_NAME, requestData, responseData));
    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE_BUF;
    SmartFd writeBuff(RequestFileDescriptorFromServer(SERVER_SOCKET_NAME, requestData, responseData));
    int sendMsg = 10;
    ASSERT_TRUE(SendMsgToSocket(writeBuff, &sendMsg, sizeof (sendMsg)));
    int recvMsg = 0;
    ASSERT_TRUE(GetMsgFromSocket(readBuff, &recvMsg, sizeof (recvMsg)));
    ASSERT_EQ(sendMsg, recvMsg);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_READ_RES;
    SmartFd readResult(RequestFileDescriptorFromServer(SERVER_SOCKET_NAME, requestData, responseData));

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE_RES;
    SmartFd writeResult(RequestFileDescriptorFromServer(SERVER_SOCKET_NAME, requestData, responseData));

    ASSERT_TRUE(SendMsgToSocket(writeBuff, &sendMsg, sizeof (sendMsg)));
    ASSERT_TRUE(GetMsgFromSocket(readBuff, &recvMsg, sizeof (recvMsg)));
    ASSERT_EQ(sendMsg, recvMsg);
}

/**
 * @tc.name: PipeFdClientTest02
 * @tc.desc: request a pip fd with invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, PipeFdClientTest02, TestSize.Level2)
{
    PipFdRequestData requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_READ_BUF;
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::ABNORMAL_SERVICE);

    requestData.pid = requestData.head.clientPid;
    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_READ_BUF;
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::ABNORMAL_SERVICE);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_WRITE_BUF;
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::ABNORMAL_SERVICE);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_READ_RES;
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::ABNORMAL_SERVICE);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES;
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::ABNORMAL_SERVICE);

    requestData.pipeType = -1;
    SendRequestToServer(SERVER_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: PipeFdClientTest03
 * @tc.desc: request to delete a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, PipeFdClientTest03, TestSize.Level2)
{
    PipFdRequestData requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    requestData.pid = requestData.head.clientPid;

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_SUCCESS);
}
#endif

#ifndef HISYSEVENT_DISABLE
/**
 * @tc.name: ReportExceptionClientTest01
 * @tc.desc: request to report exception for CRASH_DUMP_LOCAL_REPORT.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, ReportExceptionClientTest01, TestSize.Level2)
{
    CrashDumpException requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    requestData.pid = requestData.head.clientPid;
    requestData.uid = getuid();
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);
    if (strcpy_s(requestData.message, sizeof(requestData.message) / sizeof(requestData.message[0]), "Test")) {
        return;
    }
    requestData.uid = -1;
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: ReportExceptionClientTest02
 * @tc.desc: request to report exception.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, ReportExceptionClientTest02, TestSize.Level2)
{
    CrashDumpException requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    requestData.pid = requestData.head.clientPid;
    if (strcpy_s(requestData.message, sizeof(requestData.message), "Test")) {
        return;
    }
    requestData.uid = getuid();
    SmartFd sockFd(GetConnectSocketFd(SERVER_SDKDUMP_SOCKET_NAME, SOCKET_TIMEOUT));
    ASSERT_TRUE(SendMsgToSocket(sockFd, &requestData, sizeof (requestData)));
}

/**
 * @tc.name: ReportExceptionClientTest03
 * @tc.desc: request to report exception with invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, ReportExceptionClientTest03, TestSize.Level2)
{
    CrashDumpException requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    requestData.pid = requestData.head.clientPid;
    requestData.error = CrashExceptionCode::CRASH_DUMP_LOCAL_REPORT;
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);

    requestData.uid = -1;
    SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: StatsClientTest01
 * @tc.desc: request for stats client.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, StatsClientTest01, TestSize.Level2)
{
    FaultLoggerdStatsRequest requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    requestData.type = FaultLoggerdStatType::DUMP_CATCHER;

    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_SUCCESS);

    requestData.type = FaultLoggerdStatType::PROCESS_DUMP;
    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_SUCCESS);

    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_SUCCESS);

    requestData.type = FaultLoggerdStatType::DUMP_CATCHER;
    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::REQUEST_SUCCESS);
}
#endif

/**
 * @tc.name: FaultloggerdServerTest01
 * @tc.desc: test for invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, FaultloggerdServerTest01, TestSize.Level2)
{
    FaultLoggerdStatsRequest requestData;
    int32_t responseData(-1);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);

    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::INVALID_REQUEST_DATA);

    FillRequestHeadData(requestData.head, -1);
    SendRequestToServer(SERVER_CRASH_SOCKET_NAME, requestData, responseData);
    ASSERT_EQ(responseData, ResponseCode::UNKNOWN_CLIENT_TYPE);
}

/**
 * @tc.name: FaultloggerdDaemonTest01
 * @tc.desc: request a file descriptor for logging crash
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, FaultloggerdDaemonTest01, TestSize.Level2)
{
    SmartFd crashFd(TempFileManager::CreateFileDescriptor(FaultLoggerType::CPP_CRASH, getpid(), gettid(), 0));
    ASSERT_GE(crashFd, 0);
}
} // namespace HiviewDFX
} // namespace OHOS

