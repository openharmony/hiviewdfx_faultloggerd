/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "faultlog_client.h"
#include "faultloggerd_test_server.h"
#include <thread>

#include <sys/prctl.h>
#include <unistd.h>

#include "dfx_cutil.h"
#include "dfx_socket_request.h"

#if defined(HAS_LIB_SELINUX)
#include <selinux/selinux.h>
#endif

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class FaultloggerdLiteDumpClientTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
};

void FaultloggerdLiteDumpClientTest::TearDownTestCase()
{
}

void FaultloggerdLiteDumpClientTest::SetUpTestCase()
{
    FaultLoggerdTestServer::GetInstance();
    constexpr int waitTime = 1500; // 1.5s;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
}

/**
 * @tc.name: RequestLiteProcessDump001
 * @tc.desc: Test request lite dump
 * @tc.type: FUNC
 */
 HWTEST_F(FaultloggerdLiteDumpClientTest, RequestLiteProcessDump001, TestSize.Level2)
 {
    GTEST_LOG_(INFO) << "RequestLiteProcessDump001: start.";
    auto ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    ret = RequestLimitedProcessDump(getpid());
    EXPECT_EQ(ret, REQUEST_SUCCESS);

    ret = RequestLimitedProcessDump(999999);
    EXPECT_EQ(ret, REQUEST_REJECT);
    GTEST_LOG_(INFO) << "RequestLiteProcessDump001: end.";
}

/**
 * @tc.name: RequestLiteProcessDump002
 * @tc.desc: Test request lite dump request overlimit
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdLiteDumpClientTest, RequestLiteProcessDump002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestLiteProcessDump002: start.";
    int ret = 0;
    for (int i = 0; i < 11; i++) {
        ret = RequestLimitedProcessDump(getpid());
    }
    EXPECT_EQ(ret, REQUEST_REJECT);

    GTEST_LOG_(INFO) << "RequestLiteProcessDump002: end.";
}

/**
 * @tc.name: RequestLimitedPipeFd001
 * @tc.desc: Test request lite dump pipe
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdLiteDumpClientTest, RequestLimitedPipeFd001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestLimitedPipeFd001: start.";
    int pipeWriteFd;
    auto ret = RequestLimitedPipeFd(-1, &pipeWriteFd, getpid(), "test");
    char procName[NAME_BUF_LEN];
    GetProcessName(procName, sizeof(procName));
    EXPECT_EQ(ret, DEFAULT_ERROR_CODE);
    ret = RequestLimitedPipeFd(3, &pipeWriteFd, getpid(), "test");
    EXPECT_EQ(ret, DEFAULT_ERROR_CODE);
    ret = RequestLimitedPipeFd(PIPE_WRITE, &pipeWriteFd, getpid(), procName);
    EXPECT_EQ(ret, REQUEST_SUCCESS);
    ret = RequestLimitedPipeFd(PIPE_WRITE, &pipeWriteFd, getpid(), procName);
    EXPECT_EQ(ret, SDK_DUMP_REPEAT);
    ret = RequestLimitedPipeFd(PIPE_READ, &pipeWriteFd, getpid(), procName);
    EXPECT_EQ(ret, REQUEST_SUCCESS);
    GTEST_LOG_(INFO) << "RequestLimitedPipeFd001: end.";
}

/**
 * @tc.name: RequestLimitedDelPipeFd001
 * @tc.desc: Test request lite dump pipe
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdLiteDumpClientTest, RequestLimitedDelPipeFd001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestLimitedDelPipeFd001: start.";
    auto ret = RequestLimitedDelPipeFd(9999);
    EXPECT_EQ(ret, REQUEST_SUCCESS);

    ret = RequestLimitedDelPipeFd(getpid());
    EXPECT_EQ(ret, REQUEST_SUCCESS);
    GTEST_LOG_(INFO) << "RequestLimitedDelPipeFd001: end.";
}

/**
 * @tc.name: RequestLimitedDelPipeFd002
 * @tc.desc: Test request lite dump pipe
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdLiteDumpClientTest, RequestLimitedDelPipeFd002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestLimitedDelPipeFd002: start.";
    int ret = 0;
    for (int i = 0; i < 31; i++) {
        ret = RequestLimitedDelPipeFd(getpid());
    }
    EXPECT_EQ(ret, REQUEST_REJECT);
    GTEST_LOG_(INFO) << "RequestLimitedDelPipeFd002: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
