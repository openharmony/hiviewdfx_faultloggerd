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
#include <filesystem>
#include <gtest/gtest.h>
#include "faultlog_client.h"
#include "faultloggerd_test.h"
#include <thread>
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
    auto ret = RequestLimitedProcessDump(getuid());
    EXPECT_EQ(ret, REQUEST_SUCCESS);

    ret = RequestLimitedProcessDump(999999);
    EXPECT_EQ(ret, REQUEST_SUCCESS);

    GTEST_LOG_(INFO) << "RequestLiteProcessDump001: end.";
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
    auto ret = RequestLimitedPipeFd(-1, &pipeWriteFd, 3000, getuid());
    EXPECT_EQ(ret, DEFAULT_ERROR_CODE);
    ret = RequestLimitedPipeFd(3, &pipeWriteFd, 3000, getuid());
    EXPECT_EQ(ret, DEFAULT_ERROR_CODE);

    ret = RequestLimitedPipeFd(PIPE_WRITE, &pipeWriteFd, 3000, 9999);
    EXPECT_EQ(ret, REQUEST_SUCCESS);

    ret = RequestLimitedPipeFd(PIPE_WRITE, &pipeWriteFd, 3000, getuid());
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

    ret = RequestLimitedDelPipeFd(getuid());
    EXPECT_EQ(ret, REQUEST_SUCCESS);
    GTEST_LOG_(INFO) << "RequestLimitedDelPipeFd001: end.";
}

} // namespace HiviewDFX
} // namepsace OHOS
