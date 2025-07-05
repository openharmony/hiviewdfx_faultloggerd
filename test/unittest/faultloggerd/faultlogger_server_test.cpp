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
#include "dfx_test_util.h"
#include "fault_logger_daemon.h"
#include "fault_logger_pipe.h"
#include "faultloggerd_client.h"
#include "faultloggerd_socket.h"
#include "faultloggerd_test.h"
#include "fault_common_util.h"
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

int32_t SendRequestToServer(const std::string& socketName, const void* requestData, uint32_t requestSize,
    SocketFdData* socketFdData = nullptr)
{
    FaultLoggerdSocket faultLoggerdSocket;
    if (!faultLoggerdSocket.CreateSocketFileDescriptor(SOCKET_TIMEOUT)) {
        return ResponseCode::CONNECT_FAILED;
    }
    int32_t retCode{ResponseCode::DEFAULT_ERROR_CODE};
    if (!faultLoggerdSocket.StartConnect(socketName.c_str())) {
        retCode =  ResponseCode::CONNECT_FAILED;
    } else {
        retCode = socketFdData ? faultLoggerdSocket.RequestFdsFromServer({requestData, requestSize},
            *socketFdData) :  faultLoggerdSocket.RequestServer({requestData, requestSize});
    }
    faultLoggerdSocket.CloseSocketFileDescriptor();
    return retCode;
}

int32_t RequestFileDescriptorFromServer(const std::string& socketName, const void* requestData,
    size_t requestSize, int* fds, uint32_t nfds = 1)
{
    SocketFdData socketFdData{fds, nfds};
    return SendRequestToServer(socketName, requestData, requestSize, &socketFdData);
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
HWTEST_F(FaultLoggerdServiceTest, FaultloggerdServerTest01, TestSize.Level0)
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
    ASSERT_GE(sFd.GetFd(), 0);
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
    SmartFd buffReadFd{fds[0]};
    SmartFd resReadFd{fds[FD_PAIR_NUM - 1]};
    ASSERT_GE(buffReadFd.GetFd(), 0);
    ASSERT_GE(resReadFd.GetFd(), 0);

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
 * @tc.name: SdkDumpServiceTest001
 * @tc.desc: request sdk dump to hap watchdog thread.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpServiceTest001, TestSize.Level2)
{
    pid_t pid;
    pid = getpid();
    siginfo_t si;
    auto ret = FaultCommonUtil::SendSignalToHapWatchdogThread(pid, si);
    EXPECT_EQ(ret, ResponseCode::DEFAULT_ERROR_CODE);
#if defined(__aarch64__)
    std::string appName = "com.ohos.sceneboard";
    pid = GetProcessPid(appName);
    if (pid > 0) {
        ret = FaultCommonUtil::SendSignalToHapWatchdogThread(pid, si); // sceneboard is mask
        EXPECT_EQ(ret, ResponseCode::DEFAULT_ERROR_CODE);
    } else {
        FAIL() << "SdkDumpServiceTest001: " << appName << " not running.";
    }
#endif
}
/**
 * @tc.name: SdkDumpServiceTest002
 * @tc.desc: request sdk dump .
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, SdkDumpServiceTest002, TestSize.Level2)
{
    pid_t pid = getpid();
    siginfo_t si;
    auto ret = FaultCommonUtil::SendSignalToProcess(pid, si);
    EXPECT_EQ(ret, ResponseCode::REQUEST_SUCCESS);
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
    SmartFd buffReadFd{readFds[0]};
    SmartFd resReadFd{readFds[FD_PAIR_NUM - 1]};
    ASSERT_GE(buffReadFd.GetFd(), 0);
    ASSERT_GE(resReadFd.GetFd(), 0);

    PipFdRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    requestData.pid = requestData.head.clientPid;

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE;
    int32_t writeFds[FD_PAIR_NUM] = {-1, -1};
    RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData), writeFds, FD_PAIR_NUM);
    int32_t retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::ABNORMAL_SERVICE);

    SmartFd buffWriteFd{writeFds[0]};
    SmartFd resWriteFd{writeFds[FD_PAIR_NUM - 1]};
    ASSERT_GE(buffWriteFd.GetFd(), 0);
    ASSERT_GE(resWriteFd.GetFd(), 0);

    int sendMsg = 10;
    int recvMsg = 0;
    ASSERT_TRUE(SendMsgToSocket(buffWriteFd.GetFd(), &sendMsg, sizeof (sendMsg)));
    ASSERT_TRUE(GetMsgFromSocket(buffReadFd.GetFd(), &recvMsg, sizeof (recvMsg)));
    ASSERT_EQ(sendMsg, recvMsg);

    ASSERT_TRUE(SendMsgToSocket(resWriteFd.GetFd(), &sendMsg, sizeof (sendMsg)));
    ASSERT_TRUE(GetMsgFromSocket(resReadFd.GetFd(), &recvMsg, sizeof (recvMsg)));
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

/**
 * @tc.name: LitePerfPipeFdClientTest01
 * @tc.desc: request a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LitePerfPipeFdClientTest01, TestSize.Level2)
{
    LitePerfFdRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_LITEPERF_CLIENT);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_READ;
    requestData.pid = requestData.head.clientPid;
    int32_t readFds[FD_PAIR_NUM] = {-1, -1};
    RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData), readFds, FD_PAIR_NUM);
    SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));

    SmartFd buffReadFd{readFds[0]};
    SmartFd resReadFd{readFds[FD_PAIR_NUM - 1]};
    ASSERT_GE(buffReadFd.GetFd(), 0);
    ASSERT_GE(resReadFd.GetFd(), 0);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE;
    int32_t writeFds[FD_PAIR_NUM] = {-1, -1};
    RequestFileDescriptorFromServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData), writeFds, FD_PAIR_NUM);
    int retCode = SendRequestToServer(SERVER_CRASH_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::ABNORMAL_SERVICE);

    SmartFd buffWriteFd{writeFds[0]};
    SmartFd resWriteFd{writeFds[FD_PAIR_NUM - 1]};
    ASSERT_GE(buffWriteFd.GetFd(), 0);
    ASSERT_GE(resWriteFd.GetFd(), 0);

    int sendMsg = 10;
    int recvMsg = 0;
    ASSERT_TRUE(SendMsgToSocket(buffWriteFd.GetFd(), &sendMsg, sizeof (sendMsg)));
    ASSERT_TRUE(GetMsgFromSocket(buffReadFd.GetFd(), &recvMsg, sizeof (recvMsg)));
    ASSERT_EQ(sendMsg, recvMsg);

    ASSERT_TRUE(SendMsgToSocket(resWriteFd.GetFd(), &sendMsg, sizeof (sendMsg)));
    ASSERT_TRUE(GetMsgFromSocket(resReadFd.GetFd(), &recvMsg, sizeof (recvMsg)));
    ASSERT_EQ(sendMsg, recvMsg);
    RequestLitePerfDelPipeFd();
}

/**
 * @tc.name: LitePerfPipeFdClientTest02
 * @tc.desc: request a pip fd with invalid request data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LitePerfPipeFdClientTest02, TestSize.Level2)
{
    LitePerfFdRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_LITEPERF_CLIENT);

    requestData.pipeType = FaultLoggerPipeType::PIPE_FD_READ;
    requestData.pid = requestData.head.clientPid;
    SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));

    requestData.pipeType = -1;
    int32_t retCode = SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);

    requestData.pipeType = 3;
    retCode = SendRequestToServer(SERVER_SOCKET_NAME, &requestData, sizeof(requestData));
    ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
}

/**
 * @tc.name: LitePerfPipeFdClientTest03
 * @tc.desc: request to delete a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, LitePerfPipeFdClientTest03, TestSize.Level2)
{
    LitePerfFdRequestData requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_LITEPERF_CLIENT);
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
    if (!strcpy_s(requestData.message, sizeof(requestData.message) / sizeof(requestData.message[0]), "Test")) {
        requestData.uid = -1;
        retCode = SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData));
        ASSERT_EQ(retCode, ResponseCode::REQUEST_REJECT);
    }
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
    if (!strcpy_s(requestData.message, sizeof(requestData.message), "Test")) {
        requestData.uid = getuid();
        ASSERT_EQ(SendRequestToServer(SERVER_SDKDUMP_SOCKET_NAME, &requestData, sizeof(requestData)),
            ResponseCode::REQUEST_SUCCESS);
    }
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
    FaultLoggerdSocket faultLoggerdSocket;
    ASSERT_TRUE(faultLoggerdSocket.CreateSocketFileDescriptor(0));
    ASSERT_FALSE(faultLoggerdSocket.StartConnect(nullptr));
    ASSERT_FALSE(faultLoggerdSocket.ReadFileDescriptorFromSocket(nullptr, 0));
    int32_t fd = -1;
    faultLoggerdSocket.CloseSocketFileDescriptor();
    ASSERT_FALSE(faultLoggerdSocket.StartConnect(SERVER_CRASH_SOCKET_NAME));
    ASSERT_FALSE(faultLoggerdSocket.ReadFileDescriptorFromSocket(&fd, 0));
    constexpr int32_t maxConnection = 30;
    SmartFd sockFd = StartListen(nullptr, maxConnection);
    ASSERT_FALSE(sockFd);
    sockFd = StartListen("FaultloggerdSocketAbnormalTest", maxConnection);
    ASSERT_TRUE(sockFd);
    ASSERT_FALSE(SendFileDescriptorToSocket(sockFd.GetFd(), nullptr, 0));
    ASSERT_FALSE(SendMsgToSocket(sockFd.GetFd(), nullptr, 0));
    ASSERT_FALSE(GetMsgFromSocket(sockFd.GetFd(), nullptr, 0));
}

/**
 * @tc.name: AbnormalTest001
 * @tc.desc: Service exception scenario test
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest001, TestSize.Level2)
{
    SmartFd sockFd = StartListen(nullptr, 0);
    ASSERT_FALSE(sockFd);
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
 * @tc.desc: Clean up timed-out connections.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest005, TestSize.Level2)
{
    FaultLoggerdSocket faultLoggerdSocket;
    faultLoggerdSocket.CreateSocketFileDescriptor(0);
    faultLoggerdSocket.StartConnect(SERVER_SOCKET_NAME);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    FaultLoggerdStatsRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    EXPECT_EQ(faultLoggerdSocket.RequestServer({&requestData, sizeof(requestData)}),
        ResponseCode::SEND_DATA_FAILED);
    faultLoggerdSocket.CloseSocketFileDescriptor();
}

/**
 * @tc.name: AbnormalTest006
 * @tc.desc: Establish multiple connections simultaneously.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest006, TestSize.Level2)
{
    FaultLoggerdSocket faultLoggerdSocket;
    faultLoggerdSocket.CreateSocketFileDescriptor(0);
    faultLoggerdSocket.StartConnect(SERVER_SOCKET_NAME);
    FaultLoggerdSocket faultLoggerdSocket2;
    faultLoggerdSocket2.CreateSocketFileDescriptor(0);
    faultLoggerdSocket2.StartConnect(SERVER_SOCKET_NAME);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    FaultLoggerdStatsRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    EXPECT_EQ(faultLoggerdSocket2.RequestServer({&requestData, sizeof(requestData)}),
        ResponseCode::SEND_DATA_FAILED);
    EXPECT_EQ(faultLoggerdSocket.RequestServer({&requestData, sizeof(requestData)}),
        ResponseCode::REQUEST_SUCCESS);
    faultLoggerdSocket.CloseSocketFileDescriptor();
    faultLoggerdSocket2.CloseSocketFileDescriptor();
}

/**
 * @tc.name: AbnormalTest007
 * @tc.desc: After establishing the connection, close the socket directly without sending any data.
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdServiceTest, AbnormalTest007, TestSize.Level2)
{
    FaultLoggerdSocket faultLoggerdSocket;
    faultLoggerdSocket.CreateSocketFileDescriptor(0);
    faultLoggerdSocket.StartConnect(SERVER_SOCKET_NAME);
    faultLoggerdSocket.CloseSocketFileDescriptor();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    FaultLoggerdSocket faultLoggerdSocket2;
    faultLoggerdSocket2.CreateSocketFileDescriptor(0);
    faultLoggerdSocket2.StartConnect(SERVER_SOCKET_NAME);
    FaultLoggerdStatsRequest requestData;
    FillRequestHeadData(requestData.head, FaultLoggerClientType::DUMP_STATS_CLIENT);
    EXPECT_EQ(faultLoggerdSocket2.RequestServer({&requestData, sizeof(requestData)}),
        ResponseCode::REQUEST_SUCCESS);
    faultLoggerdSocket2.CloseSocketFileDescriptor();
}
} // namespace HiviewDFX
} // namespace OHOS
