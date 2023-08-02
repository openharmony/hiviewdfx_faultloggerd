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

#include <gtest/gtest.h>

#include "dfx_test_util.h"
#include "faultloggerd_client.h"

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
        int ret = -1;
        constexpr int32_t appUid = 100068;
        setuid(appUid);
        int32_t fd = RequestFileDescriptor(FaultLoggerType::CPP_CRASH);
        if (fd >= 0) {
            close(fd);
            ret = 0;
        }
        
        fd = RequestFileDescriptor(FaultLoggerType::JS_HEAP_SNAPSHOT);
        if (fd >= 0) {
            close(fd);
            ret += 0;
        }
        exit(ret);
    } else if (pid > 0) {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
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
        if (waitpid(pid, &status, 0) == -1) {
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
    if (!IsSelinuxEnforced()) {
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
        if (waitpid(pid, &status, 0) == -1) {
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
#endif
} // namespace HiviewDFX
} // namepsace OHOS
