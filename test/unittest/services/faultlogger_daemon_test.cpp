/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include <securec.h>
#ifndef is_ohos_lite
#include "gmock/gmock.h"
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_exception.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "faultloggerd_socket.h"
#include "fault_logger_config.h"
#include "fault_logger_daemon.h"
#include "fault_logger_pipe.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerDaemonTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

#ifndef is_ohos_lite
class MockFaultLoggerDaemon : public FaultLoggerDaemon {
public:
    MOCK_METHOD(bool, CreateSockets, (), (override));
    MOCK_METHOD(bool, InitEnvironment, (), (override));
    MOCK_METHOD(bool, CreateEventFd, (), (override));
    MOCK_METHOD(void, WaitForRequest, (), (override));
};
#endif
} // namespace HiviewDFX
} // namespace OHOS

namespace {
constexpr int32_t FAULTLOGGERD_FUZZ_READ_BUFF = 1024;
static constexpr uint32_t ROOT_UID = 0;
static constexpr uint32_t BMS_UID = 1000;
static constexpr uint32_t HIVIEW_UID = 1201;
static constexpr uint32_t HIDUMPER_SERVICE_UID = 1212;
static constexpr uint32_t FOUNDATION_UID = 5523;
static constexpr uint32_t PIPE_TYPE_COUNT = 3;

/**
 * @tc.name: FaultLoggerDaemonTest001
 * @tc.desc: test HandleDefaultClientRequest/HandleLogFileDesClientRequest func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest001: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);
    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        GTEST_LOG_(ERROR) << "memset_s failed" ;
        ASSERT_TRUE(false);
    }
    faultloggerdRequest.type = 0;
    faultloggerdRequest.pid = getpid();
    faultloggerdRequest.tid = gettid();
    faultloggerdRequest.uid = getuid();
    daemon->HandleDefaultClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.type = 2; // 2 : CPP_CRASH
    daemon->HandleDefaultClientRequest(-1, &faultloggerdRequest);
    daemon->HandleLogFileDesClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.type = 101; // 101 : CPP_STACKTRACE
    daemon->HandleDefaultClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.type = 102; // 102 : JS_STACKTRACE
    daemon->HandleDefaultClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.type = 103; // 103 : JS_HEAP_SNAPSHOT
    daemon->HandleDefaultClientRequest(-1, &faultloggerdRequest);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest001: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest002
 * @tc.desc: test HandleSdkDumpRequest/HandlePipeFdClientRequest func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest002: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);
    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        GTEST_LOG_(ERROR) << "memset_s failed" ;
        ASSERT_TRUE(false);
    }
    faultloggerdRequest.type = 2; // 2 : CPP_CRASH
    faultloggerdRequest.pid = getpid();
    faultloggerdRequest.tid = gettid();
    faultloggerdRequest.uid = getuid();
    daemon->HandleSdkDumpRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.pipeType = FaultLoggerPipeType::PIPE_FD_READ;
    daemon->HandlePipeFdClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE;
    daemon->HandlePipeFdClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    daemon->HandlePipeFdClientRequest(-1, &faultloggerdRequest);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest002: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest003
 * @tc.desc: test HandleSdkDumpRequest func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest003: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);
    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        GTEST_LOG_(ERROR) << "memset_s failed" ;
        ASSERT_TRUE(false);
    }
    faultloggerdRequest.type = 2; // 2 : CPP_CRASH
    faultloggerdRequest.pid = getpid();
    faultloggerdRequest.tid = gettid();
    faultloggerdRequest.uid = getuid();
    daemon->HandleSdkDumpRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.tid = 0;
    daemon->HandleSdkDumpRequest(-1, &faultloggerdRequest);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest003: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest004
 * @tc.desc: test CreateFileForRequest func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest004: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    int32_t type = (int32_t)FaultLoggerType::CPP_CRASH;
    int32_t pid = getpid();
    uint64_t time = GetTimeMilliSeconds();
    int fd = daemon->CreateFileForRequest(type, pid, 0, time);
    ASSERT_NE(fd, -1);
    close(fd);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest004: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest005
 * @tc.desc: test CreateFileForRequest JIT_CODE_LOG and FFRT_CRASH_LOG branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest005: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);

    int32_t pid = getpid();
    uint64_t time = GetTimeMilliSeconds();
    int32_t type = static_cast<int32_t>(FaultLoggerType::JIT_CODE_LOG);
    int fd = daemon->CreateFileForRequest(type, pid, 0, time);
    ASSERT_NE(fd, -1);
    type = static_cast<int32_t>(FaultLoggerType::FFRT_CRASH_LOG);
    fd = daemon->CreateFileForRequest(type, pid, 0, time);
    ASSERT_NE(fd, -1);
    close(fd);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest005: end.";
}

void DoClientProcess(const std::string& socketFileName)
{
    sleep(2); // 2 : wait 2 seconds, waiting for the service to be ready
    int clientSocketFd = -1;
    bool ret = StartConnect(clientSocketFd, socketFileName.c_str(), 10); // 10 : socket connect time out 10 second
    ASSERT_TRUE(ret);
    ASSERT_NE(clientSocketFd, -1);
    GTEST_LOG_(INFO) << "child connect finished, client fd:" << clientSocketFd;

    int data = 12345; // 12345 is for server Cred test
    ret = SendMsgIovToSocket(clientSocketFd, reinterpret_cast<void *>(&data), sizeof(data));
    ASSERT_TRUE(ret);

    GTEST_LOG_(INFO) << "Start read file desc";
    int testFd = ReadFileDescriptorFromSocket(clientSocketFd);
    GTEST_LOG_(INFO) << "recv testFd:" << testFd;
    ASSERT_NE(testFd, -1);
    close(clientSocketFd);
    close(testFd);
}

void TestSecurityCheck(const std::string& socketFileName)
{
    int32_t serverSocketFd = -1;
    bool ret = StartListen(serverSocketFd, socketFileName.c_str(), 5); // 5: means max connection count is 5
    ASSERT_TRUE(ret);
    ASSERT_NE(serverSocketFd, -1);
    GTEST_LOG_(INFO) << "server start listen fd:" << serverSocketFd;

    struct timeval timev = {
        20, // 20 : recv timeout 20 seconds
        0
    };
    void* pTimev = &timev;
    int retOpt = OHOS_TEMP_FAILURE_RETRY(setsockopt(serverSocketFd, SOL_SOCKET, SO_RCVTIMEO,
        static_cast<const char*>(pTimev), sizeof(struct timeval)));
    ASSERT_NE(retOpt, -1);

    struct sockaddr_un clientAddr;
    socklen_t clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));
    int32_t connectionFd = OHOS_TEMP_FAILURE_RETRY(accept(serverSocketFd,
        reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrSize));
    ASSERT_GT(connectionFd, 0);

    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        GTEST_LOG_(ERROR) << "memset_s failed" ;
        ASSERT_TRUE(false);
    }
    faultloggerdRequest.type = 2; // 2 : CPP_CRASH
    faultloggerdRequest.pid = getpid();
    faultloggerdRequest.tid = gettid();
    faultloggerdRequest.uid = getuid();

    FaultLoggerCheckPermissionResp resp = daemon->SecurityCheck(connectionFd, &faultloggerdRequest);
    ASSERT_EQ(resp, FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS);

    close(connectionFd);
    close(serverSocketFd);
}

/**
 * @tc.name: FaultLoggerDaemonTest006
 * @tc.desc: test SecurityCheck abnormal branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest006: start.";
    std::string testSocketName = "faultloggerd.server.test";
    int32_t pid = fork();
    if (pid == 0) {
        DoClientProcess(testSocketName);
        GTEST_LOG_(INFO) << "client exit";
        exit(0);
    } else if (pid > 0) {
        TestSecurityCheck(testSocketName);

        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            GTEST_LOG_(INFO) << "Exit status was " << exitCode;
        }
        ASSERT_EQ(exitCode, 0);
    }
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest006: end.";
}

void TestCheckRequestCredential(const std::string& socketFileName)
{
    int32_t serverSocketFd = -1;
    bool ret = StartListen(serverSocketFd, socketFileName.c_str(), 5); // 5: means max connection count is 5
    ASSERT_TRUE(ret);
    ASSERT_NE(serverSocketFd, -1);
    GTEST_LOG_(INFO) << "server start listen fd:" << serverSocketFd;

    struct timeval timev = {
        20, // 20 : recv timeout 20 seconds
        0
    };
    void* pTimev = &timev;
    int retOpt = OHOS_TEMP_FAILURE_RETRY(setsockopt(serverSocketFd, SOL_SOCKET, SO_RCVTIMEO,
        static_cast<const char*>(pTimev), sizeof(struct timeval)));
    ASSERT_NE(retOpt, -1);

    struct sockaddr_un clientAddr;
    socklen_t clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));
    int32_t connectionFd = OHOS_TEMP_FAILURE_RETRY(accept(serverSocketFd,
        reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrSize));
    ASSERT_GT(connectionFd, 0);

    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        GTEST_LOG_(ERROR) << "memset_s failed" ;
        ASSERT_TRUE(false);
    }
    faultloggerdRequest.type = 2; // 2 : CPP_CRASH
    faultloggerdRequest.pid = getpid();
    faultloggerdRequest.tid = gettid();
    faultloggerdRequest.uid = getuid();

    bool result = daemon->CheckRequestCredential(connectionFd, nullptr);
    ASSERT_EQ(result, false);
    result = daemon->CheckRequestCredential(connectionFd, &faultloggerdRequest);
    ASSERT_EQ(result, false);

    close(connectionFd);
    close(serverSocketFd);
}

/**
 * @tc.name: FaultLoggerDaemonTest007
 * @tc.desc: test CheckRequestCredential abnormal branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest007: start.";
    std::string testSocketName = "faultloggerd.server.test";
    int32_t pid = fork();
    if (pid == 0) {
        DoClientProcess(testSocketName);
        GTEST_LOG_(INFO) << "client exit";
        exit(0);
    } else if (pid > 0) {
        TestCheckRequestCredential(testSocketName);

        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            GTEST_LOG_(INFO) << "Exit status was " << exitCode;
        }
        ASSERT_EQ(exitCode, 0);
    }
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest007: end.";
}

void TestHandleExceptionRequest(const std::string& socketFileName)
{
    int32_t serverSocketFd = -1;
    bool ret = StartListen(serverSocketFd, socketFileName.c_str(), 5); // 5: means max connection count is 5
    ASSERT_TRUE(ret);
    ASSERT_NE(serverSocketFd, -1);
    GTEST_LOG_(INFO) << "server start listen fd:" << serverSocketFd;

    struct timeval timev = {
        20, // 20 : recv timeout 20 seconds
        0
    };
    void* pTimev = &timev;
    int retOpt = OHOS_TEMP_FAILURE_RETRY(setsockopt(serverSocketFd, SOL_SOCKET, SO_RCVTIMEO,
        static_cast<const char*>(pTimev), sizeof(struct timeval)));
    ASSERT_NE(retOpt, -1);

    struct sockaddr_un clientAddr;
    socklen_t clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));
    int32_t connectionFd = OHOS_TEMP_FAILURE_RETRY(accept(serverSocketFd,
        reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrSize));
    ASSERT_GT(connectionFd, 0);

    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    struct FaultLoggerdRequest faultloggerdRequest;
    if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
        GTEST_LOG_(ERROR) << "memset_s failed" ;
        ASSERT_TRUE(false);
    }
    faultloggerdRequest.type = 2; // 2 : CPP_CRASH
    faultloggerdRequest.pid = getpid();
    faultloggerdRequest.tid = gettid();
    faultloggerdRequest.uid = getuid();

    daemon->HandleExceptionRequest(connectionFd, &faultloggerdRequest);

    close(connectionFd);
    close(serverSocketFd);
}

/**
 * @tc.name: FaultLoggerDaemonTest008
 * @tc.desc: test HandleExceptionRequest abnormal branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest008: start.";
    std::string testSocketName = "faultloggerd.server.test";
    int32_t pid = fork();
    if (pid == 0) {
        DoClientProcess(testSocketName);
        GTEST_LOG_(INFO) << "client exit";
        exit(0);
    } else if (pid > 0) {
        TestHandleExceptionRequest(testSocketName);

        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            GTEST_LOG_(INFO) << "Exit status was " << exitCode;
        }
        ASSERT_EQ(exitCode, 0);
    }
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest008: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest009
 * @tc.desc: test HandleAccept abnormal branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest009: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);
    daemon->HandleAccept(1, 1);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest009: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest010
 * @tc.desc: test HandleRequestByClientType abnormal branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest010: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);

    int32_t connectionFd = 1;

    FaultLoggerdRequest request;
    request.clientType = FaultLoggerClientType::DEFAULT_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::LOG_FILE_DES_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::PRINT_T_HILOG_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::PERMISSION_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::PIPE_FD_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::SDK_DUMP_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::REPORT_EXCEPTION_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = -1;
    daemon->HandleRequestByClientType(connectionFd, &request);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest010: end.";
}

void TestHandleRequestByPipeType(std::shared_ptr<FaultLoggerDaemon> daemon, FaultLoggerPipe2* faultLoggerPipe,
    FaultLoggerdRequest request, int32_t pipeType, bool isChangeFd)
{
    int pipeFd[PIPE_NUM_SZ] = { -1, -1 };
    request.pipeType = pipeType;
    daemon->HandleRequestByPipeType(pipeFd, 1, &request, faultLoggerPipe);
    if (isChangeFd) {
        ASSERT_NE(pipeFd[PIPE_BUF_INDEX], -1);
    } else {
        ASSERT_EQ(pipeFd[PIPE_BUF_INDEX], -1);
    }
}

/**
 * @tc.name: FaultLoggerDaemonTest011
 * @tc.desc: test HandleRequestByPipeType abnormal branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest011: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);

    FaultLoggerdRequest request;
    FaultLoggerPipe2* faultLoggerPipe = new FaultLoggerPipe2(GetTimeMilliSeconds());
    bool isChangeFds4Pipe[PIPE_TYPE_COUNT] = { false, true, false };
    for (int i = 0; i < PIPE_TYPE_COUNT; i++) {
        TestHandleRequestByPipeType(daemon, faultLoggerPipe, request, i, isChangeFds4Pipe[i]);
    }
    delete(faultLoggerPipe);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest011: end.";
}

#ifndef is_ohos_lite
/**
 * @tc.name: FaultLoggerDaemonTest012
 * @tc.desc: test FaultLoggerDaemon HandleStaticForFuzzer
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest012, TestSize.Level2)
{
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    int params[][2] = {
        {FaultLoggerType::CPP_STACKTRACE, ROOT_UID},
        {FaultLoggerType::JS_STACKTRACE, BMS_UID},
        {FaultLoggerType::LEAK_STACKTRACE, HIVIEW_UID},
        {FaultLoggerType::FFRT_CRASH_LOG, HIDUMPER_SERVICE_UID},
        {FaultLoggerType::JIT_CODE_LOG, FOUNDATION_UID},
        {FaultLoggerType::JS_HEAP_LEAK_LIST, FOUNDATION_UID},
    };
    bool ret = false;
    for (int i = 0; i < sizeof(params) / sizeof(params[0]); i++) {
        ret = daemon->HandleStaticForFuzzer(params[i][0], params[i][1]);
        EXPECT_TRUE(ret);
    }
    ret = daemon->HandleStaticForFuzzer(-1, 1);
    EXPECT_FALSE(ret);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest012: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest013
 * @tc.desc: test FaultLoggerDaemon StartServer and AddEvent
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest013: start.";
    MockFaultLoggerDaemon mockDaemon;

    EXPECT_CALL(mockDaemon, CreateSockets()).WillOnce(testing::Return(false));
    EXPECT_EQ(mockDaemon.StartServer(), -1);
    EXPECT_CALL(mockDaemon, CreateSockets()).WillOnce(testing::Return(true));
    EXPECT_CALL(mockDaemon, InitEnvironment()).WillOnce(testing::Return(false));
    EXPECT_EQ(mockDaemon.StartServer(), -1);
    EXPECT_CALL(mockDaemon, CreateSockets()).WillOnce(testing::Return(true));
    EXPECT_CALL(mockDaemon, InitEnvironment()).WillOnce(testing::Return(true));
    EXPECT_CALL(mockDaemon, CreateEventFd()).WillOnce(testing::Return(false));
    EXPECT_EQ(mockDaemon.StartServer(), -1);
    EXPECT_CALL(mockDaemon, CreateSockets()).WillOnce(testing::Return(true));
    EXPECT_CALL(mockDaemon, InitEnvironment()).WillOnce(testing::Return(true));
    EXPECT_CALL(mockDaemon, CreateEventFd()).WillOnce(testing::Return(true));
    EXPECT_CALL(mockDaemon, WaitForRequest()).WillOnce(testing::Return());
    EXPECT_EQ(mockDaemon.StartServer(), 0);

    FaultLoggerDaemon daemon;
    EXPECT_EQ(daemon.InitEnvironment(), true);
    EXPECT_EQ(daemon.CreateEventFd(), true);
    daemon.CleanupEventFd();
    daemon.CleanupSockets();
    daemon.AddEvent(1, 1, 1);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest013: end.";
}
#endif

bool CheckReadResp(int sockfd)
{
    char controlBuffer[SOCKET_BUFFER_SIZE] = {0};
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(sockfd, controlBuffer, sizeof(controlBuffer) - 1));
    if (nread != static_cast<ssize_t>(strlen(FAULTLOGGER_DAEMON_RESP))) {
        return false;
    }
    return true;
}

void HandleRequestByPipeTypeCommon(std::shared_ptr<FaultLoggerDaemon> daemon, int32_t pipeType,
    bool isPassCheck = false, bool isChangeFd = false)
{
    int pipeFd[PIPE_NUM_SZ] = { -1, -1 };
    FaultLoggerdRequest request;
    request.pipeType = pipeType;
    std::unique_ptr<FaultLoggerPipe2> ptr = std::make_unique<FaultLoggerPipe2>(GetTimeMilliSeconds());

    if (!isPassCheck) {
        daemon->HandleRequestByPipeType(pipeFd, 1, &request, ptr.get());
        if (!isChangeFd) {
            EXPECT_EQ(pipeFd[PIPE_BUF_INDEX], -1);
        } else {
            EXPECT_NE(pipeFd[PIPE_BUF_INDEX], -1);
        }
        CloseFd(pipeFd[PIPE_BUF_INDEX]);
        CloseFd(pipeFd[PIPE_RES_INDEX]);
        return;
    }

    int socketFd[2]; // 2 : the length of the array
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
        pid_t pid = fork();
        if (pid == 0) {
            sleep(1);
            if (CheckReadResp(socketFd[1])) {
                std::string test = "test";
                OHOS_TEMP_FAILURE_RETRY(write(socketFd[1], test.c_str(), sizeof(test)));
            }
        } else if (pid > 0) {
            daemon->connectionMap_[socketFd[0]] = socketFd[0];
            daemon->HandleRequestByPipeType(pipeFd, socketFd[0], &request, ptr.get());
            if (!isChangeFd) {
                EXPECT_EQ(pipeFd[PIPE_BUF_INDEX], -1);
            } else {
                EXPECT_NE(pipeFd[PIPE_BUF_INDEX], -1);
            }
            CloseFd(pipeFd[PIPE_BUF_INDEX]);
            CloseFd(pipeFd[PIPE_RES_INDEX]);
            close(socketFd[1]);
        }
    }
}

/**
 * @tc.name: FaultLoggerDaemonTest014
 * @tc.desc: test FaultLoggerDaemon for HandleRequestByPipeType
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest014, TestSize.Level4)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest014: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    daemon->InitEnvironment();
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_READ, true, true);
    sleep(2);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_READ, false, false);
    HandleRequestByPipeTypeCommon(daemon, FaultLoggerPipeType::PIPE_FD_WRITE, false, true);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest014: end.";
}

void HandleRequestTestCommon(std::shared_ptr<FaultLoggerDaemon> daemon, char* buff, size_t len,
    void *(*startRoutine)(void *))
{
    int socketFd[2]; // 2 : the length of the array
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
        OHOS_TEMP_FAILURE_RETRY(write(socketFd[1], buff, len));
        daemon->connectionMap_[socketFd[0]] = socketFd[0];

        if (!startRoutine) {
            daemon->HandleRequest(0, socketFd[0]);
        } else {
            pthread_t threadId;
            if (pthread_create(&threadId, nullptr, startRoutine, reinterpret_cast<void*>(socketFd[1])) != 0) {
                perror("Failed to create thread");
                close(socketFd[0]);
                close(socketFd[1]);
                return;
            }

            daemon->HandleRequest(0, socketFd[0]);
            EXPECT_NE(socketFd[0], -1);

            pthread_join(threadId, nullptr);
        }
        close(socketFd[1]);
    }
}

void HandleRequestByClientTypeForDefaultClientTest(std::shared_ptr<FaultLoggerDaemon> daemon)
{
    FaultLoggerdRequest requst;
    requst.clientType = FaultLoggerClientType::DEFAULT_CLIENT;
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    requst.type = FaultLoggerType::CPP_CRASH;
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    requst.type = FaultLoggerType::JS_HEAP_SNAPSHOT;
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    daemon->crashTimeMap_[1] = time(nullptr) - 10; // 10 : Get the first 10 seconds of the current time
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    daemon->crashTimeMap_[1] = time(nullptr);
    HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
}

void *ReadThread1(void *param)
{
    long fd = reinterpret_cast<long>(param);
    char buff[FAULTLOGGERD_FUZZ_READ_BUFF];
    OHOS_TEMP_FAILURE_RETRY(read(fd, buff, sizeof(buff)));
    char msg[] = "any test str";
    OHOS_TEMP_FAILURE_RETRY(write(fd, reinterpret_cast<char*>(msg), sizeof(msg)));
    return nullptr;
}

void *ReadThread2(void *param)
{
    long fd = reinterpret_cast<long>(param);
    char buff[FAULTLOGGERD_FUZZ_READ_BUFF];
    OHOS_TEMP_FAILURE_RETRY(read(fd, buff, sizeof(buff)));
    CrashDumpException test;
    test.error = CRASH_DUMP_LOCAL_REPORT;
    OHOS_TEMP_FAILURE_RETRY(write(fd, reinterpret_cast<char*>(&test), sizeof(CrashDumpException)));
    return nullptr;
}

void *ReadThread3(void *param)
{
    long fd = reinterpret_cast<long>(param);
    char buff[FAULTLOGGERD_FUZZ_READ_BUFF];
    OHOS_TEMP_FAILURE_RETRY(read(fd, buff, sizeof(buff)));
    CrashDumpException test{};
    OHOS_TEMP_FAILURE_RETRY(write(fd, reinterpret_cast<char*>(&test), sizeof(CrashDumpException)));
    return nullptr;
}

void HandleRequestByClientTypeTest(std::shared_ptr<FaultLoggerDaemon> daemon)
{
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::LOG_FILE_DES_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::PRINT_T_HILOG_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::PERMISSION_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::SDK_DUMP_CLIENT;
        requst.pid = 1;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::PIPE_FD_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = FaultLoggerClientType::REPORT_EXCEPTION_CLIENT;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread1);
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread2);
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), ReadThread3);
    }
    {
        FaultLoggerdRequest requst;
        requst.clientType = -1;
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest), nullptr);
    }
}

/**
 * @tc.name: FaultLoggerDaemonTest015
 * @tc.desc: test FaultLoggerDaemon for HandleRequest
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest015, TestSize.Level4)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest015: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    {
        int32_t epollFd = 1;
        int32_t connectionFd = 4; // 4 : simulate an fd greater than 3
        daemon->HandleRequest(epollFd, connectionFd);
        connectionFd = 2;
        daemon->HandleRequest(epollFd, connectionFd);
        EXPECT_EQ(epollFd, 1);
        epollFd = -1;
        daemon->HandleRequest(epollFd, connectionFd);
        EXPECT_EQ(epollFd, -1);
        connectionFd = 4;
        daemon->HandleRequest(epollFd, connectionFd);
        EXPECT_EQ(epollFd, -1);
    }
    {
        int socketFd[2]; // 2 : the length of the array
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
            close(socketFd[1]);
            daemon->HandleRequest(0, socketFd[0]);
        }
    }
    {
        FaultLoggerdRequest requst;
        int socketFd[2]; // 2 : the length of the array
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socketFd) == 0) {
            OHOS_TEMP_FAILURE_RETRY(write(socketFd[1], reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdRequest)));
            daemon->HandleRequest(0, socketFd[0]);
            close(socketFd[1]);
        }
        EXPECT_NE(socketFd[1], -1);
    }

    if (!daemon->InitEnvironment()) {
        daemon->CleanupSockets();
    }
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest015: end.";
}

std::vector<FaultLoggerdStatsRequest> InitStatsRequests()
{
    std::vector<FaultLoggerdStatsRequest> statsRequest;
    {
        FaultLoggerdStatsRequest requst{};
        requst.type = PROCESS_DUMP;
        requst.signalTime = GetTimeMilliSeconds();
        requst.pid = 1;
        requst.processdumpStartTime = time(nullptr);
        requst.processdumpFinishTime = time(nullptr) + 10; // 10 : Get the last 10 seconds of the current time
        statsRequest.emplace_back(requst);
    }
    {
        FaultLoggerdStatsRequest requst{};
        auto lastRequst = statsRequest.back();
        requst.type = DUMP_CATCHER;
        requst.pid = lastRequst.pid;
        requst.requestTime = time(nullptr);
        requst.dumpCatcherFinishTime = time(nullptr) + 10; // 10 : Get the last 10 seconds of the current time
        requst.result = 1;
        statsRequest.emplace_back(requst);
    }
    {
        FaultLoggerdStatsRequest requst{};
        requst.type = DUMP_CATCHER;
        requst.pid = 1;
        requst.requestTime = time(nullptr);
        requst.dumpCatcherFinishTime = time(nullptr) + 10; // 10 : Get the last 10 seconds of the current time
        requst.result = 1;
        statsRequest.emplace_back(requst);
    }
    return statsRequest;
}

/**
 * @tc.name: FaultLoggerDaemonTest016
 * @tc.desc: test FaultLoggerDaemon for HandleDumpStats
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest016, TestSize.Level4)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest016: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    std::vector<FaultLoggerdStatsRequest> statsRequest = InitStatsRequests();
    for (auto requst : statsRequest) {
        HandleRequestTestCommon(daemon, reinterpret_cast<char*>(&requst), sizeof(FaultLoggerdStatsRequest), nullptr);
    }
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest016: end.";
}

/**
 * @tc.name: FaultLoggerDaemonTest017
 * @tc.desc: test FaultLoggerDaemon for HandleRequest
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest017, TestSize.Level4)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest017: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    HandleRequestByClientTypeForDefaultClientTest(daemon);
    HandleRequestByClientTypeTest(daemon);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest017: end.";
}
}