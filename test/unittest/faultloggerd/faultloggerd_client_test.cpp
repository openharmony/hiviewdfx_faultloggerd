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
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "fault_common_util.h"
#include "fault_coredump_service.h"
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
 * @tc.name: SendSignalToHapWatchdogThreadTest
 * @tc.desc: Test send signal to watchdog thread
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, SendSignalToHapWatchdogThreadTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SendSignalToHapWatchdogThreadTest: start.";
    pid_t pid = -1;
    siginfo_t si = {};
    ASSERT_EQ(FaultCommonUtil::SendSignalToHapWatchdogThread(pid, si), ResponseCode::DEFAULT_ERROR_CODE);
    GTEST_LOG_(INFO) << "SendSignalToHapWatchdogThreadTest: end.";
}

/**
 * @tc.name: SendSignalToProcessTest
 * @tc.desc: SendSignalToProcess
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, SendSignalToProcessTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SendSignalToProcessTest: start.";
    pid_t pid = -1;
    siginfo_t si = {};
    ASSERT_EQ(FaultCommonUtil::SendSignalToProcess(pid, si), ResponseCode::REQUEST_SUCCESS);
    GTEST_LOG_(INFO) << "SendSignalToProcessTest: end.";
}

/**
 * @tc.name: GetUcredByPeerCredTest
 * @tc.desc: GetUcredByPeerCred
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, GetUcredByPeerCredTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetUcredByPeerCredTest: start.";
    int32_t connectionFd = 0;
    struct ucred rcred;
    bool result = FaultCommonUtil::GetUcredByPeerCred(rcred, connectionFd);
    EXPECT_FALSE(result);
    GTEST_LOG_(INFO) << "GetUcredByPeerCredTest: end.";
}

/**
 * @tc.name: HandleProcessDumpPidTest001
 * @tc.desc: HandleProcessDumpPid
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, HandleProcessDumpPidTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest001: start.";
    int32_t targetPid = 123;
    int32_t processDumpPid = -1;
    CoredumpStatusService coredumpStatusService;
    EXPECT_FALSE(coredumpStatusService.HandleProcessDumpPid(targetPid, processDumpPid));

    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest001: end.";
}

/**
 * @tc.name: HandleProcessDumpPidTest002
 * @tc.desc: HandleProcessDumpPid
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, HandleProcessDumpPidTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest002: start.";
    int32_t targetPid = 123;
    int32_t processDumpPid = 456;
    CoredumpStatusService coredumpStatusService;
    EXPECT_FALSE(coredumpStatusService.HandleProcessDumpPid(targetPid, processDumpPid));
    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest002: end.";
}

/**
 * @tc.name: DoCoredumpRequestTest
 * @tc.desc: DoCoredumpRequest
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, DoCoredumpRequestTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DoCoredumpRequestTest: start.";
    std::string socketName = "test_socket";
    int32_t connectionFd = 1;
    CoreDumpRequestData requestData = {};
    CoredumpService coredumpService;
    ASSERT_EQ(coredumpService.DoCoredumpRequest(socketName, connectionFd, requestData), ResponseCode::REQUEST_REJECT);
    GTEST_LOG_(INFO) << "DoCoredumpRequestTest: end.";
}

/**
 * @tc.name: RecorderProcessMapTest
 * @tc.desc: RecorderProcessMap
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RecorderProcessMapTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RecorderProcessMapTest: start.";
    RecorderProcessMap recorderProcessMap;
    int32_t coredumpSocketId = 0;
    int32_t processDumpPid = getpid();
    bool flag = false;
    EXPECT_FALSE(recorderProcessMap.ClearTargetPid(getpid()));
    EXPECT_FALSE(recorderProcessMap.SetCancelFlag(getpid(), true));
    EXPECT_FALSE(recorderProcessMap.SetProcessDumpPid(getpid(), getpid()));
    EXPECT_FALSE(recorderProcessMap.GetCoredumpSocketId(getpid(), coredumpSocketId));
    EXPECT_FALSE(recorderProcessMap.GetProcessDumpPid(getpid(), processDumpPid));
    EXPECT_FALSE(recorderProcessMap.GetCancelFlag(getpid(), flag));
    GTEST_LOG_(INFO) << "RecorderProcessMapTest: end.";
}

/**
 * @tc.name: CoreDumpCbTest001
 * @tc.desc: Test startcoredumpcb and finishcoredumpcb process
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, CoreDumpCbTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoreDumpCbTest001: start.";
    int32_t retCode = ResponseCode::REQUEST_SUCCESS;
    std::string fileName = "com.ohos.sceneboard.dmp";
    ASSERT_EQ(StartCoredumpCb(getpid(), getpid()), ResponseCode::ABNORMAL_SERVICE);
    ASSERT_EQ(FinishCoredumpCb(getpid(), fileName, retCode), ResponseCode::REQUEST_SUCCESS);
    GTEST_LOG_(INFO) << "CoreDumpCbTest001: end.";
}

/**
 * @tc.name: CoreDumpCbTest002
 * @tc.desc: Test coredump process
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, CoreDumpCbTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoreDumpCbTest002: start.";
    int32_t retCode = ResponseCode::REQUEST_SUCCESS;
    std::string fileName = "com.ohos.sceneboard.dmp";

    auto threadFunc = [&fileName, retCode]() {
        sleep(1);
        StartCoredumpCb(getpid(), getpid());
        FinishCoredumpCb(getpid(), fileName, retCode);
    };

    std::thread t(threadFunc);
    t.detach();
    auto ret = SaveCoredumpToFileTimeout(getpid());
    ASSERT_EQ(ret, "com.ohos.sceneboard.dmp");
    GTEST_LOG_(INFO) << "CoreDumpCbTest002: end.";
}

/**
 * @tc.name: CoreDumpCbTest003
 * @tc.desc: Test coredump cancel process
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, CoreDumpCbTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoreDumpCbTest003: start.";
    auto threadFunc = []() {
        sleep(1);
        ASSERT_EQ(CancelCoredump(getpid()), ResponseCode::REQUEST_SUCCESS);
        StartCoredumpCb(getpid(), getpid()); // to clean processmap
    };

    std::thread t(threadFunc);
    t.detach();
    auto ret = SaveCoredumpToFileTimeout(getpid());
    ASSERT_EQ(ret, "");
    GTEST_LOG_(INFO) << "CoreDumpCbTest003: end.";
}

/**
 * @tc.name: CoreDumpCbTest004
 * @tc.desc: Test coredump cancel process
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, CoreDumpCbTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoreDumpCbTest004: start.";
    auto threadFunc = []() {
        sleep(1);
        StartCoredumpCb(getpid(), getpid());
        ASSERT_EQ(CancelCoredump(getpid()), ResponseCode::REQUEST_SUCCESS);
    };

    std::thread t(threadFunc);
    t.detach();
    auto ret = SaveCoredumpToFileTimeout(getpid());
    ASSERT_EQ(ret, "");
    GTEST_LOG_(INFO) << "CoreDumpCbTest004: end.";
}

/**
 * @tc.name: CoreDumpCbTest005
 * @tc.desc: Test coredump cancel process
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, CoreDumpCbTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoreDumpCbTest005: start.";
    ASSERT_EQ(CancelCoredump(getpid()), ResponseCode::DEFAULT_ERROR_CODE);
    GTEST_LOG_(INFO) << "CoreDumpCbTest005: end.";
}

/**
 * @tc.name: CoreDumpCbTest006
 * @tc.desc: Test coredump repeat process
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, CoreDumpCbTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoreDumpCbTest006: start.";
    int32_t retCode = ResponseCode::REQUEST_SUCCESS;
    std::string fileName = "com.ohos.sceneboard.dmp";

    auto threadFunc = [&fileName, retCode]() {
        sleep(1);
        ASSERT_EQ(SaveCoredumpToFileTimeout(getpid()), "");
        StartCoredumpCb(getpid(), getpid());
        FinishCoredumpCb(getpid(), fileName, retCode);
    };

    std::thread t(threadFunc);
    t.detach();
    auto ret = SaveCoredumpToFileTimeout(getpid());
    ASSERT_EQ(ret, "com.ohos.sceneboard.dmp");
    GTEST_LOG_(INFO) << "CoreDumpCbTest006: end.";
}

/**
 * @tc.name: CoreDumpCbTest007
 * @tc.desc: Test coredump process when targetPid = -1
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, CoreDumpCbTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoreDumpCbTest007: start.";
    int32_t targetPid = -1;
    int32_t retCode = ResponseCode::REQUEST_SUCCESS;
    std::string fileName = "com.ohos.sceneboard.dmp";
    ASSERT_EQ(SaveCoredumpToFileTimeout(targetPid), "");
    ASSERT_EQ(CancelCoredump(targetPid), ResponseCode::DEFAULT_ERROR_CODE);
    ASSERT_EQ(StartCoredumpCb(targetPid, getpid()), ResponseCode::DEFAULT_ERROR_CODE);
    ASSERT_EQ(FinishCoredumpCb(targetPid, fileName, retCode), ResponseCode::DEFAULT_ERROR_CODE);
    GTEST_LOG_(INFO) << "CoreDumpCbTest007: end.";
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
