/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include <filesystem>
#include <functional>
#include <gtest/gtest.h>
#include <securec.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include "dfx_exception.h"
#include "dfx_log.h"
#include "dfx_socket_request.h"
#include "dfx_util.h"
#include "fault_logger_daemon.h"
#include "fault_logger_pipe.h"
#include "faultloggerd_client.h"
#include "faultloggerd_socket.h"
#include "faultloggerd_test.h"
#include "smart_fd.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr int32_t SOCKET_TIMEOUT = 1;
constexpr int32_t FD_PAIR_NUM = 2;
constexpr const char* const FAULTLOGGERD_SERVER_TEST_TAG = "FAULTLOGGERD_SERVER_TEST";
void FillRequestHeadData(RequestDataHead& head, int8_t clientType)
{
    head.clientType = clientType;
    head.clientPid = getpid();
}

int32_t SendRequestToSocket(int32_t sockFd, const std::string& socketName, const void* requestData, size_t requestSize)
{
    if (!StartConnect(sockFd, socketName.c_str(), SOCKET_TIMEOUT)) {
        return ResponseCode::CONNECT_FAILED;
    }
    if (!SendMsgToSocket(sockFd, requestData, requestSize)) {
        return ResponseCode::SEND_DATA_FAILED;
    }
    int32_t responseCode{ResponseCode::RECEIVE_DATA_FAILED};
    GetMsgFromSocket(sockFd, &responseCode, sizeof (responseCode));
    return responseCode;
}

int32_t SendRequestToServer(const std::string& socketName, const void* requestData, size_t requestSize,
    std::function<void(int32_t, int32_t&)> callback = nullptr)
{
    SmartFd sockFd = CreateSocketFd();
    int responseCode = SendRequestToSocket(sockFd, socketName, requestData, requestSize);
    if (callback) {
        callback(sockFd, responseCode);
    }
    if (responseCode != ResponseCode::REQUEST_SUCCESS) {
        DFXLOGE("%{public}s :: failed to get filedescriptor from Server and response code %{public}d",
            FAULTLOGGERD_SERVER_TEST_TAG, responseCode);
    }
    return responseCode;
}

int32_t RequestFileDescriptorFromServer(const std::string& socketName, const void* requestData,
    size_t requestSize, int* fds, uint32_t nfds = 1)
{
    return SendRequestToServer(socketName, requestData, requestSize, [fds, nfds] (int32_t socketFd, int32_t& retCode) {
        if (retCode == ResponseCode::REQUEST_SUCCESS) {
            retCode = ReadFileDescriptorFromSocket(socketFd, fds, nfds) ?
                ResponseCode::REQUEST_SUCCESS : ResponseCode::RECEIVE_DATA_FAILED;
        }
    });
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

/**
 * @tc.name: FaultloggerdServerTest01
 * @tc.desc: test for invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, FaultloggerdServerTest01, TestSize.Level2)
{
    FaultLoggerdStatsRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    int32_t retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::INVALID_REQUEST_DATA);

    FillRequestHeadData(requestData.head, -1);
    retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::UNKNOWN_CLIENT_TYPE);
}

/**
 * @tc.name: LogFileDesClientTest01
 * @tc.desc: request a file descriptor for logging crash
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LogFileDesClientTest01, TestSize.Level2)
{
    FaultLoggerdRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    requestData.type = FaultLoggerType::CPP_CRASH;
    requestData.pid = requestData.head.clientPid;
    int32_t fd{-1};
    RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData), &fd);
    SmartFd sFd(fd);
    ASSERT_GE(sFd, 0);
}

/**
 * @tc.name: LogFileDesClientTest02
 * @tc.desc: request a file descriptor through a socket not matched.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LogFileDesClientTest02, TestSize.Level2)
{
    FaultLoggerdRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    requestData.type = FaultLoggerType::CPP_CRASH;
    requestData.pid = requestData.head.clientPid;
    int32_t retCode = SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
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
    int32_t retCode = SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
}

#ifndef is_ohos_lite
/**
 * @tc.name: FaultloggerdClientTest001
 * @tc.desc: request a file descriptor for logging crash
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpClientTest01, TestSize.Level2)
{
    constexpr int waitTime = 1500; // 1.5s;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    SdkDumpRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    requestData.pid = requestData.head.clientPid;
    int32_t fds[FD_PAIR_NUM] = {-1, -1};
    RequestFileDescriptorFromServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData), fds, FD_PAIR_NUM);
    SmartFd buffReadFd = fds[0];
    SmartFd resReadFd = fds[FD_PAIR_NUM - 1];
    ASSERT_GE(buffReadFd, 0);
    ASSERT_GE(resReadFd, 0);

    int32_t retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::SDK_DUMP_REPEAT);
    constexpr int pipeTimeOut = 11000;
    requestData.time = pipeTimeOut;
    retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_SUCCESS);
    RequestDelPipeFd(requestData.pid);
}

/**
 * @tc.name: SdkDumpClientTest02
 * @tc.desc: request sdk dump with invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpClientTest02, TestSize.Level2)
{
    SdkDumpRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    requestData.pid = 0;
    int32_t retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);

    requestData.pid = requestData.head.clientPid;
    retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
}
    /**
 * @tc.name: SdkDumpClientTest03
 * @tc.desc: request sdk dumpJson after request a fd for cppcrash.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpClientTest03, TestSize.Level2)
{
    RequestFileDescriptor(FaultLoggerType::CPP_CRASH);
    SdkDumpRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    requestData.pid = requestData.head.clientPid;
    int32_t retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::SDK_PROCESS_CRASHED);
}
/**
 * @tc.name: PipeFdClientTest01
 * @tc.desc: request a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, PipeFdClientTest01, TestSize.Level2)
{
    constexpr int waitTime = 1500; // 1.5s;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    SdkDumpRequestData sdkDumpRequestData;
    FillRequestHeadData(sdkDumpRequestData.head, FaultLoggerClientType::SDK_DUMP_CLIENT);
    sdkDumpRequestData.pid = sdkDumpRequestData.head.clientPid;
    int32_t readFds[FD_PAIR_NUM] = {-1, -1};
    RequestFileDescriptorFromServer(SERVER_SDKDUMP_SOCKET_NAME, &sdkDumpRequestData, sizeof(sdkDumpRequestData),
        readFds, FD_PAIR_NUM);
    SmartFd buffReadFd = readFds[0];
    SmartFd resReadFd = readFds[FD_PAIR_NUM - 1];
    ASSERT_GE(buffReadFd, 0);
    ASSERT_GE(resReadFd, 0);

    PipFdRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    requestData.pid = requestData.head.clientPid;

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE;
    int32_t writeFds[FD_PAIR_NUM] = {-1, -1};
    RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData), writeFds, FD_PAIR_NUM);
    int32_t retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::ABNORMAL_SERVICE);

    SmartFd buffWriteFd = writeFds[0];
    SmartFd resWriteFd = writeFds[FD_PAIR_NUM - 1];
    ASSERT_GE(buffWriteFd, 0);
    ASSERT_GE(resWriteFd, 0);

    int sendMsg = 10;
    int recvMsg = 0;
    ASSERT_TRUE(SendMsgToSocket(buffWriteFd, &sendMsg, sizeof (sendMsg)));
    ASSERT_TRUE(GetMsgFromSocket(buffReadFd, &recvMsg, sizeof (recvMsg)));
    ASSERT_EQ(sendMsg, recvMsg);

    ASSERT_TRUE(SendMsgToSocket(resWriteFd, &sendMsg, sizeof (sendMsg)));
    ASSERT_TRUE(GetMsgFromSocket(resReadFd, &recvMsg, sizeof (recvMsg)));
    ASSERT_EQ(sendMsg, recvMsg);
    RequestDelPipeFd(requestData.pid);
}

/**
 * @tc.name: PipeFdClientTest02
 * @tc.desc: request a pip fd with invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, PipeFdClientTest02, TestSize.Level2)
{
    PipFdRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_READ;
    requestData.pid = requestData.head.clientPid;
    int32_t retCode = SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::ABNORMAL_SERVICE);

    requestData.pipeType = -1;
    retCode = SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);

    requestData.pipeType = 3;
    retCode = SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: PipeFdClientTest03
 * @tc.desc: request to delete a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, PipeFdClientTest03, TestSize.Level2)
{
    PipFdRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    requestData.pid = requestData.head.clientPid;

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    int32_t retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_SUCCESS);
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
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    requestData.pid = requestData.head.clientPid;
    requestData.uid = getuid();
    int32_t retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
    if (strcpy_s(requestData.message, sizeof(requestData.message) / sizeof(requestData.message[0]), "Test")) {
        return;
    }
    requestData.uid = -1;
    retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: ReportExceptionClientTest02
 * @tc.desc: request to report exception.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, ReportExceptionClientTest02, TestSize.Level2)
{
    CrashDumpException requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    requestData.pid = requestData.head.clientPid;
    if (strcpy_s(requestData.message, sizeof(requestData.message), "Test")) {
        return;
    }
    requestData.uid = getuid();
    SmartFd socketFd = CreateSocketFd();
    StartConnect(socketFd, SERVER_SDKDUMP_SOCKET_NAME, SOCKET_TIMEOUT);
    ASSERT_TRUE(SendMsgToSocket(socketFd, &requestData, sizeof (requestData)));
}

/**
 * @tc.name: ReportExceptionClientTest03
 * @tc.desc: request to report exception with invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, ReportExceptionClientTest03, TestSize.Level2)
{
    CrashDumpException requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::REPORT_EXCEPTION_CLIENT);
    requestData.pid = requestData.head.clientPid;
    requestData.error = CrashExceptionCode::CRASH_DUMP_LOCAL_REPORT;
    int32_t retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);

    requestData.uid = -1;
    retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: StatsClientTest01
 * @tc.desc: request for stats client.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, StatsClientTest01, TestSize.Level2)
{
    FaultLoggerdStatsRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    requestData.type = FaultLoggerdStatType::DUMP_CATCHER;

    int32_t retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_SUCCESS);

    requestData.type = FaultLoggerdStatType::PROCESS_DUMP;
    retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_SUCCESS);

    retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_SUCCESS);

    requestData.type = FaultLoggerdStatType::DUMP_CATCHER;
    retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_SUCCESS);
}
#endif

/**
 * @tc.name: FaultloggerdSocketAbnormalTest
 * @tc.desc: test abnomarl case
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, FaultloggerdSocketAbnormalTest, TestSize.Level2)
{
    ASSERT_FALSE(StartConnect(-1, nullptr, 3));
    int32_t sockFd = -1;
    constexpr int32_t maxConnection = 30;
    ASSERT_FALSE(StartListen(sockFd, nullptr, maxConnection));
    ASSERT_TRUE(StartListen(sockFd, "FaultloggerdSocketAbnormalTest", maxConnection));
    ASSERT_FALSE(SendFileDescriptorToSocket(sockFd, nullptr, 0));
    ASSERT_FALSE(ReadFileDescriptorFromSocket(sockFd, nullptr, 0));
    int32_t fd = -1;
    ASSERT_FALSE(ReadFileDescriptorFromSocket(sockFd, &fd, 1));
    ASSERT_FALSE(SendMsgToSocket(sockFd, nullptr, 0));
    ASSERT_FALSE(GetMsgFromSocket(sockFd, nullptr, 0));
}

/**
 * @tc.name: AbnormalTest001
 * @tc.desc: Service exception scenario test
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest001, TestSize.Level2)
{
    bool cur = StartConnect(-1, nullptr, 0);
    ASSERT_FALSE(cur);
    cur = StartConnect(0, nullptr, 0);
    ASSERT_FALSE(cur);
    int32_t sockFd = 0;
    char* name = nullptr;
    cur = StartListen(sockFd, name, 0);
    ASSERT_FALSE(cur);
}

/**
 * @tc.name: AbnormalTest002
 * @tc.desc: Service exception scenario test
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest002, TestSize.Level2)
{
    bool cur = SendFileDescriptorToSocket(0, nullptr, 0);
    ASSERT_FALSE(cur);
    cur = SendFileDescriptorToSocket(0, nullptr, 1);
    ASSERT_FALSE(cur);
    int32_t fd = 0;
    int32_t* fds = &fd;
    cur = SendFileDescriptorToSocket(-1, fds, 1);
    ASSERT_FALSE(cur);
}

/**
 * @tc.name: AbnormalTest003
 * @tc.desc: Service exception scenario test
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest003, TestSize.Level2)
{
    bool cur = SendMsgToSocket(0, nullptr, 0);
    ASSERT_FALSE(cur);
    void* data = reinterpret_cast<void*>(1);
    cur = SendMsgToSocket(-1, data, 0);
    ASSERT_FALSE(cur);
    cur = SendMsgToSocket(0, data, 0);
    ASSERT_FALSE(cur);
}

/**
 * @tc.name: AbnormalTest004
 * @tc.desc: Service exception scenario test
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest004, TestSize.Level2)
{
    bool cur = GetMsgFromSocket(0, nullptr, 0);
    ASSERT_FALSE(cur);
    void* data = reinterpret_cast<void*>(1);
    cur = GetMsgFromSocket(-1, data, 0);
    ASSERT_FALSE(cur);
    cur = GetMsgFromSocket(0, data, 0);
    ASSERT_FALSE(cur);
}

/**
 * @tc.name: AbnormalTest005
 * @tc.desc: Service exception scenario test
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest005, TestSize.Level2)
{
    FaultLoggerDaemon::GetInstance().GetEpollManager(EpollManagerType::HELPER_SERVER);
    EpollManager* manager = FaultLoggerDaemon::GetInstance().GetEpollManager((EpollManagerType)(-1));
    ASSERT_EQ(manager, nullptr);
    int fd = -1;
    FaultLoggerPipe faultloggerPipe;
    faultloggerPipe.Close(fd);
}
} // namespace HiviewDFX
} // namespace OHOS

