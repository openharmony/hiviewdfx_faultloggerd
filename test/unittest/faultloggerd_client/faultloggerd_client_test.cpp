/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "faultloggerd_client.h"
#include "faultloggerd_socket.h"

#if defined(HAS_LIB_SELINUX)
#include <selinux/selinux.h>
#endif

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class FaultloggerdClientTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void FaultloggerdClientTest::SetUpTestCase()
{
}

void FaultloggerdClientTest::TearDownTestCase()
{
}

void FaultloggerdClientTest::SetUp()
{
}

void FaultloggerdClientTest::TearDown()
{
}

bool IsSelinuxEnforced()
{
    std::string cmd = "getenforce";
    std::string selinuxStatus = ExecuteCommands(cmd);
    GTEST_LOG_(INFO) << "getenforce return:" << selinuxStatus;
    return (selinuxStatus.find("Enforcing") != std::string::npos) ? true : false;
}

/**
 * @tc.name: FaultloggerdClientTest001
 * @tc.desc: request a file descriptor for logging crash
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, FaultloggerdClientTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest001: start.";
    int32_t fd = RequestFileDescriptor(FaultLoggerType::CPP_CRASH);
    ASSERT_GT(fd, 0);
    close(fd);

    fd = RequestFileDescriptor(FaultLoggerType::JS_HEAP_SNAPSHOT);
    ASSERT_GT(fd, 0);
    close(fd);

    fd = RequestFileDescriptor(FaultLoggerType::JS_RAW_SNAPSHOT);
    ASSERT_GT(fd, 0);
    close(fd);
    GTEST_LOG_(INFO) << "FaultloggerdClientTest001: end.";
}

/**
 * @tc.name: FaultloggerdClientTest002
 * @tc.desc: request a file descriptor for logging with app uid
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, FaultloggerdClientTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest002: start.";
    int32_t pid = fork();
    if (pid == 0) {
        int ret = 0;
        constexpr int32_t appUid = 100068;
        setuid(appUid);
        do {
            int32_t fd = RequestFileDescriptor(FaultLoggerType::CPP_CRASH);
            if (fd < 0) {
                ret = -1;
                break;
            }
            close(fd);

            fd = RequestFileDescriptor(FaultLoggerType::JS_HEAP_SNAPSHOT);
            if (fd < 0) {
                ret = -1;
                break;
            }
            close(fd);

            fd = RequestFileDescriptor(FaultLoggerType::JS_RAW_SNAPSHOT);
            if (fd < 0) {
                ret = -1;
                break;
            }
            close(fd);
        } while (false);
        exit(ret);
    } else if (pid > 0) {
        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            printf("Exit status was %d\n", exitCode);
        }
        ASSERT_EQ(exitCode, 0);
    }
    GTEST_LOG_(INFO) << "FaultloggerdClientTest002: end.";
}

/**
 * @tc.name: FaultloggerdClientTest003
 * @tc.desc: request a file descriptor for logging with inconsistent pid
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, FaultloggerdClientTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest003: start.";
    int32_t pid = fork();
    if (pid == 0) {
        int ret = 1;
        constexpr int32_t appUid = 100068;
        setuid(appUid);
        FaultLoggerdRequest request;
        request.type = FaultLoggerType::JS_HEAP_SNAPSHOT;
        request.pid = appUid;
        int32_t fd = RequestFileDescriptorEx(&request);
        if (fd >= 0) {
            close(fd);
            ret = 0;
        }
        exit(ret);
    } else if (pid > 0) {
        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            printf("Exit status was %d\n", exitCode);
        }
        ASSERT_EQ(exitCode, 1);
    }
    GTEST_LOG_(INFO) << "FaultloggerdClientTest003: end.";
}
#if defined(HAS_LIB_SELINUX)
/**
 * @tc.name: FaultloggerdClientTest004
 * @tc.desc: request a file descriptor for logging with inconsistent pid but in processdump scontext
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, FaultloggerdClientTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest004: start.";

    // If selinux is not opened, skip this test item
    bool isSuccess = IsSelinuxEnforced();
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(INFO) << "Selinux is not opened, skip FaultloggerdClientTest004";
        return;
    }
    int32_t pid = fork();
    if (pid == 0) {
        int ret = 1;
        constexpr int32_t appUid = 100068;
        setcon("u:r:processdump:s0");
        setuid(appUid);
        FaultLoggerdRequest request;
        request.type = FaultLoggerType::JS_HEAP_SNAPSHOT;
        request.pid = appUid;
        int32_t fd = RequestFileDescriptorEx(&request);
        if (fd >= 0) {
            close(fd);
            ret = 0;
        }
        exit(ret);
    } else if (pid > 0) {
        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            printf("Exit status was %d\n", exitCode);
        }
        ASSERT_EQ(exitCode, 0);
    }
    GTEST_LOG_(INFO) << "FaultloggerdClientTest004: end.";
}

/**
 * @tc.name: FaultloggerdClientTest005
 * @tc.desc: sdkdump request which selinux label belongs to { hiview hidumper foundation }
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, FaultloggerdClientTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest005: start.";

    // If selinux is not opened, skip this test item
    bool isSuccess = IsSelinuxEnforced();
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(INFO) << "Selinux is not opened, skip FaultloggerdClientTest005";
        return;
    }

    int32_t pid = fork();
    if (pid == 0) {
        int ret = 1;
        constexpr int32_t appUid = 100068;
        setcon("u:r:hiview:s0");
        setuid(appUid);
        int resp = RequestSdkDump(1, 1);
        if (resp == FaultLoggerCheckPermissionResp::CHECK_PERMISSION_PASS) {
            ret = 0;
        }
        exit(ret);
    } else if (pid > 0) {
        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            printf("Exit status was %d\n", exitCode);
        }
        ASSERT_EQ(exitCode, 0);
    }
    GTEST_LOG_(INFO) << "FaultloggerdClientTest005: end.";
}

/**
 * @tc.name: FaultloggerdClientTest006
 * @tc.desc: sdkdump request which selinux label doesn't belongs to { hiview hidumper foundation }
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, FaultloggerdClientTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest006: start.";

    // If selinux is not opened, skip this test item
    bool isSuccess = IsSelinuxEnforced();
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(INFO) << "Selinux is not opened, skip FaultloggerdClientTest006";
        return;
    }
    int32_t pid = fork();
    if (pid == 0) {
        int ret = 1;
        constexpr int32_t appUid = 100068;
        setcon("u:r:wifi_host:s0");
        setuid(appUid);
        int resp = RequestSdkDump(1, 1);
        if (resp == FaultLoggerCheckPermissionResp::CHECK_PERMISSION_REJECT) {
            ret = 0;
        }
        exit(ret);
    } else if (pid > 0) {
        int status;
        bool isSuccess = waitpid(pid, &status, 0) != -1;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            return;
        }

        int exitCode = -1;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
            printf("Exit status was %d\n", exitCode);
        }
        ASSERT_EQ(exitCode, 0);
    }
    GTEST_LOG_(INFO) << "FaultloggerdClientTest006: end.";
}
#endif

void DoClientProcess(const std::string& socketFileName)
{
    // wait 2 seconds, waiting for the service to be ready
    sleep(2);
    int clientSocketFd = -1;

    // socket connect time out 10 second
    bool retBool = StartConnect(clientSocketFd, socketFileName.c_str(), 10);
    ASSERT_TRUE(retBool);
    ASSERT_NE(clientSocketFd, -1);
    GTEST_LOG_(INFO) << "child connect finished, client fd:" << clientSocketFd;

    int data = 12345; // 12345 is for server Cred test
    retBool = SendMsgIovToSocket(clientSocketFd, reinterpret_cast<void *>(&data), sizeof(data));
    ASSERT_TRUE(retBool);

    GTEST_LOG_(INFO) << "Start read file desc";
    int testFd = ReadFileDescriptorFromSocket(clientSocketFd);
    GTEST_LOG_(INFO) << "recv testFd:" << testFd;
    ASSERT_NE(testFd, -1);
    close(clientSocketFd);
    close(testFd);
}

void DoServerProcess(const std::string& socketFileName)
{
    GTEST_LOG_(INFO) << "server prepare listen";
    int32_t serverSocketFd = -1;
    std::string testFileName = "/data/test.txt";

    // 5: means max connection count is 5
    bool ret = StartListen(serverSocketFd, socketFileName.c_str(), 5);
    ASSERT_TRUE(ret);
    ASSERT_NE(serverSocketFd, -1);
    GTEST_LOG_(INFO) << "server start listen fd:" << serverSocketFd;

    struct timeval timev = {
        20, // recv timeout 20 seconds
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
    GTEST_LOG_(INFO) << "server accept fd:" << connectionFd;

    int optval = 1;
    retOpt = setsockopt(connectionFd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval));
    ASSERT_NE(retOpt, -1);

    struct ucred rcred;
    bool retCred = RecvMsgCredFromSocket(connectionFd, &rcred);
    ASSERT_TRUE(retCred);
    GTEST_LOG_(INFO) << "uid:" << rcred.uid;

    // 0666 for file read and write
    int testFdServer = open(testFileName.c_str(), O_RDWR | O_CREAT, 0666);
    GTEST_LOG_(INFO) << "Start SendFileDescriptorToSocket";
    SendFileDescriptorToSocket(connectionFd, testFdServer);
    GTEST_LOG_(INFO) << "Close server connect fd";
    close(connectionFd);
    close(testFdServer);
    close(serverSocketFd);
    unlink(testFileName.c_str());
}

/**
 * @tc.name: FaultloggerdSocketTest001
 * @tc.desc: test StartListen, RecvMsgCredFromSocket and SendMsgCtlToSocket
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, FaultloggerdSocketTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdSocketTest001: start.";
    std::string testSocketName = "faultloggerd.server.test";

    int32_t pid = fork();
    if (pid == 0) {
        DoClientProcess(testSocketName);
        GTEST_LOG_(INFO) << "client exit";
        exit(0);
    } else if (pid > 0) {
        DoServerProcess(testSocketName);

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
    GTEST_LOG_(INFO) << "FaultloggerdSocketTest001: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
