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
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "dfx_exception.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "fault_logger_config.h"
#include "faultloggerd_socket.h"

#define private public
#include "fault_logger_daemon.h"
#undef private

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
} // namespace HiviewDFX
} // namespace OHOS

namespace {
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
    faultloggerdRequest.pipeType = FaultLoggerPipeType::PIPE_FD_READ_BUF;
    daemon->HandlePipeFdClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE_BUF;
    daemon->HandlePipeFdClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.pipeType = FaultLoggerPipeType::PIPE_FD_READ_RES;
    daemon->HandlePipeFdClientRequest(-1, &faultloggerdRequest);
    faultloggerdRequest.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE_RES;
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
    int fd = daemon->CreateFileForRequest(type, pid, 0, time, false);
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
    int fd = daemon->CreateFileForRequest(type, pid, 0, time, false);
    ASSERT_NE(fd, -1);
    type = static_cast<int32_t>(FaultLoggerType::FFRT_CRASH_LOG);
    fd = daemon->CreateFileForRequest(type, pid, 0, time, false);
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
    ASSERT_EQ(resp, FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT);

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
        if (waitpid(pid, &status, 0) == -1) {
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
        if (waitpid(pid, &status, 0) == -1) {
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
        if (waitpid(pid, &status, 0) == -1) {
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
 * @tc.desc: test HandleRequest and HandleRequestByClientType abnormal branch
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerDaemonTest, FaultLoggerDaemonTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest010: start.";
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    bool ret = daemon->InitEnvironment();
    ASSERT_TRUE(ret);

    int32_t epollFd = 1;
    int32_t connectionFd = 4; // 4 : simulate an fd greater than 3
    daemon->HandleRequest(epollFd, connectionFd);
    connectionFd = 1;
    daemon->HandleRequest(epollFd, connectionFd);

    FaultLoggerdRequest request;
    request.clientType = FaultLoggerClientType::DEFAULT_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::LOG_FILE_DES_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::PRINT_T_HILOG_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::PERMISSION_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::SDK_DUMP_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::PIPE_FD_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = FaultLoggerClientType::REPORT_EXCEPTION_CLIENT;
    daemon->HandleRequestByClientType(connectionFd, &request);
    request.clientType = -1;
    daemon->HandleRequestByClientType(connectionFd, &request);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest010: end.";
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

    int fd = -1;
    int32_t connectionFd = 1;
    FaultLoggerdRequest request;
    FaultLoggerPipe2* faultLoggerPipe = new FaultLoggerPipe2(GetTimeMilliSeconds());
    request.pipeType = FaultLoggerPipeType::PIPE_FD_READ_BUF;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE_BUF;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_READ_RES;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_WRITE_RES;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_READ_BUF;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_WRITE_BUF;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_READ_RES;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = FaultLoggerPipeType::PIPE_FD_DELETE;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    request.pipeType = -1;
    daemon->HandleRequestByPipeType(fd, connectionFd, &request, faultLoggerPipe);
    GTEST_LOG_(INFO) << "FaultLoggerDaemonTest011: end.";
}

}