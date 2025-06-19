/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#if defined(HAS_LIB_SELINUX)
#include <selinux/selinux.h>
#endif

#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "faultloggerd_test.h"
#include "fault_logger_daemon.h"
#include "smart_fd.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class FaultloggerdClientTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
};

void FaultloggerdClientTest::TearDownTestCase()
{
    ClearTempFiles();
}

void FaultloggerdClientTest::SetUpTestCase()
{
    FaultLoggerdTestServer::GetInstance();
    constexpr int waitTime = 1500; // 1.5s;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
}

/**
 * @tc.name: RequestSdkDumpTest001
 * @tc.desc: test the function for RequestSdkDump.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestSdkDumpTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestSdkDumpTest001: start.";
    int pipeFds[] = { -1, -1 };
    ASSERT_EQ(RequestSdkDump(getpid(), gettid(), pipeFds), ResponseCode::REQUEST_SUCCESS);
    ASSERT_TRUE(pipeFds[0] >= 0);
    ASSERT_TRUE(pipeFds[1] >= 0);
    GTEST_LOG_(INFO) << "RequestSdkDumpTest001: end.";
}

/**
 * @tc.name: RequestFileDescriptorTest001
 * @tc.desc: test the function for RequestFileDescriptor.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestFileDescriptorTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestFileDescriptorTest001: start.";
    SmartFd sFd(RequestFileDescriptor(FaultLoggerType::CPP_CRASH));
    ASSERT_GT(sFd.GetFd(), 0);
    GTEST_LOG_(INFO) << "RequestFileDescriptorTest001: end.";
}

/**
 * @tc.name: RequestFileDescriptorTest002
 * @tc.desc: test the function for RequestFileDescriptor.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestFileDescriptorTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestFileDescriptorTest002: start.";
    SmartFd sFd(RequestFileDescriptor(FaultLoggerType::FFRT_CRASH_LOG));
    ASSERT_EQ(sFd.GetFd(), -1);
    GTEST_LOG_(INFO) << "RequestFileDescriptorTest002: end.";
}

/**
 * @tc.name: RequestFileDescriptorTest003
 * @tc.desc: test the function for RequestFileDescriptor.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestFileDescriptorTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestFileDescriptorTest003: start.";
    SmartFd sFd(RequestFileDescriptor(FaultLoggerType::CJ_HEAP_SNAPSHOT));
    ASSERT_EQ(sFd.GetFd(), -1);
    GTEST_LOG_(INFO) << "RequestFileDescriptorTest003: end.";
}

/**
 * @tc.name: LogFileDesClientTest01
 * @tc.desc: test the function for RequestFileDescriptorEx.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestFileDescriptorEx001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestFileDescriptorEx001: start.";
    FaultLoggerdRequest faultLoggerdRequest;
    faultLoggerdRequest.type = FaultLoggerType::CPP_CRASH;
    faultLoggerdRequest.pid = getpid();
    faultLoggerdRequest.tid = gettid();
    faultLoggerdRequest.time = GetTimeMilliSeconds();
    SmartFd sFd(RequestFileDescriptorEx(&faultLoggerdRequest));
    ASSERT_GE(sFd.GetFd(), 0);
    GTEST_LOG_(INFO) << "RequestFileDescriptorEx001: end.";
}

/**
 * @tc.name: RequestFileDescriptorEx002
 * @tc.desc: test the function for RequestFileDescriptorEx with invalid parameter.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestFileDescriptorEx002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestFileDescriptorEx002: start.";
    SmartFd sFd(RequestFileDescriptorEx(nullptr));
    ASSERT_EQ(sFd.GetFd(), -1);
    GTEST_LOG_(INFO) << "RequestFileDescriptorEx002: end.";
}

/**
 * @tc.name: RequestSdkDumpTest002
 * @tc.desc: test the function for RequestSdkDump.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestSdkDumpTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestSdkDumpTest002: start.";
    int pipeFds[] = { -1, -1 };
    ASSERT_EQ(RequestSdkDump(0, 0, pipeFds), -1);
    ASSERT_EQ(RequestSdkDump(1, -1, pipeFds), -1);
    ASSERT_EQ(RequestSdkDump(getpid(), gettid(), pipeFds), ResponseCode::SDK_PROCESS_CRASHED);
    GTEST_LOG_(INFO) << "RequestSdkDumpTest002: end.";
}

/**
 * @tc.name: RequestPipeFdTest001
 * @tc.desc: test the function for RequestPipeFd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestPipeFdTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestPipeFd001: start.";
    int pipeFds[] = { -1, -1 };
    ASSERT_EQ(RequestPipeFd(0, PIPE_FD_READ - 1, pipeFds), -1);
    ASSERT_EQ(RequestPipeFd(0, PIPE_FD_DELETE + 1, pipeFds), -1);
    ASSERT_EQ(RequestPipeFd(getpid(), PIPE_FD_READ, pipeFds), ResponseCode::REQUEST_SUCCESS);
    SmartFd buffFd{pipeFds[0]};
    SmartFd resFd{pipeFds[1]};
    ASSERT_GE(buffFd.GetFd(), 0);
    ASSERT_GE(resFd.GetFd(), 0);
    GTEST_LOG_(INFO) << "RequestPipeFd001: end.";
}

/**
 * @tc.name: RequestDelPipeFdTest001
 * @tc.desc: test the function for RequestDelPipeFd.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestDelPipeFdTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest001: start.";
    ASSERT_EQ(RequestDelPipeFd(getpid()), ResponseCode::REQUEST_SUCCESS);
    GTEST_LOG_(INFO) << "FaultloggerdClientTest001: end.";
}

/**
 * @tc.name: ReportDumpStatsTest001
 * @tc.desc: test the function for ReportDumpStats.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, ReportDumpStatsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultloggerdClientTest001: start.";
    ASSERT_EQ(ReportDumpStats(nullptr), -1);
    FaultLoggerdStatsRequest request;
    ASSERT_EQ(ReportDumpStats(&request), 0);
    GTEST_LOG_(INFO) << "FaultloggerdClientTest001: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
