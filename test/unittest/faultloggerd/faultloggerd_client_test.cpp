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
#include "faultlog_client.h"
#include "fault_common_util.h"
#include "faultloggerd_test.h"
#include "faultloggerd_test_server.h"
#include "fault_logger_daemon.h"
#include "smart_fd.h"
#include "minidump_manager_service.h"
#include "dfx_socket_request.h"

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
 * @tc.name: RequestFileDescriptorEx001
 * @tc.desc: test the function for RequestFileDescriptorEx with invalid parameter.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestFileDescriptorEx001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestFileDescriptorEx001: start.";
    SmartFd sFd(RequestFileDescriptorEx(nullptr));
    ASSERT_EQ(sFd.GetFd(), -1);
    GTEST_LOG_(INFO) << "RequestFileDescriptorEx001: end.";
}

/**
 * @tc.name: RequestSdkDumpTest002
 * @tc.desc: test the function for RequestSdkDump.
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestSdkDumpTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestSdkDumpTest002: start.";
    FaultLoggerdRequest faultLoggerdRequest;
    faultLoggerdRequest.type = FaultLoggerType::CPP_CRASH;
    faultLoggerdRequest.pid = getpid();
    faultLoggerdRequest.tid = gettid();
    faultLoggerdRequest.time = GetTimeMilliSeconds();
    SmartFd sFd(RequestFileDescriptorEx(&faultLoggerdRequest));
    ASSERT_GE(sFd.GetFd(), 0);
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

#ifdef __aarch64__
/**
 * @tc.name: RequestSetMinidumpToCrashLogTest001
 * @tc.desc: test RequestSetMinidumpToCrashLog enable
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestSetMinidumpToCrashLogTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestSetMinidumpToCrashLogTest001: start.";
    int32_t ret = RequestSetMinidumpToCrashLog(true);
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "RequestSetMinidumpToCrashLogTest001: end.";
}

/**
 * @tc.name: RequestSetMinidumpToCrashLogTest002
 * @tc.desc: test RequestSetMinidumpToCrashLog disable
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, RequestSetMinidumpToCrashLogTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RequestSetMinidumpToCrashLogTest002: start.";
    int32_t ret = RequestSetMinidumpToCrashLog(false);
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "RequestSetMinidumpToCrashLogTest002: end.";
}

/**
 * @tc.name: MiniDumpRequestDataFieldTest001
 * @tc.desc: test MiniDumpRequestData enableMinidumpToCrashLog field
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MiniDumpRequestDataFieldTest001, TestSize.Level2)
{
    MiniDumpRequestData data{};
    data.head.clientType = MINIDUMP_CLIENT;
    data.pid = getpid();
    data.enableMinidump = 1;
    data.enableMinidumpToCrashLog = 1;
    EXPECT_EQ(data.enableMinidump, 1);
    EXPECT_EQ(data.enableMinidumpToCrashLog, 1);
    data.enableMinidump = 0;
    data.enableMinidumpToCrashLog = 0;
    EXPECT_EQ(data.enableMinidump, 0);
    EXPECT_EQ(data.enableMinidumpToCrashLog, 0);
    data.enableMinidump = -1;
    data.enableMinidumpToCrashLog = -1;
    EXPECT_EQ(data.enableMinidump, -1);
    EXPECT_EQ(data.enableMinidumpToCrashLog, -1);
}

/**
 * @tc.name: MinidumpManagerServiceSetMiniDumpTest001
 * @tc.desc: test SetMiniDump with enableMinidump=1 adds to configs
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerServiceSetMiniDumpTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid();
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    int ret = svc.SetMiniDump(testPid, 1, -1);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);
    svc.enableMinidumpConfigs.erase(testPid);
}

/**
 * @tc.name: MinidumpManagerServiceSetMiniDumpTest002
 * @tc.desc: test SetMiniDump with enableMinidump=0 removes from configs
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerServiceSetMiniDumpTest002, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid();
    svc.enableMinidumpConfigs.emplace(testPid);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);
    int ret = svc.SetMiniDump(testPid, 0, -1);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerServiceSetMiniDumpTest003
 * @tc.desc: test SetMiniDump with enableMinidumpToCrashLog=0 adds to disable set
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerServiceSetMiniDumpTest003, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    svc.disableMinidumpToCrashLogConfigs.clear();
    pid_t testPid = getpid();
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
    int ret = svc.SetMiniDump(testPid, -1, 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 1);
    svc.disableMinidumpToCrashLogConfigs.erase(testPid);
}

/**
 * @tc.name: MinidumpManagerServiceSetMiniDumpTest004
 * @tc.desc: test SetMiniDump with enableMinidumpToCrashLog=1 removes from disable set
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerServiceSetMiniDumpTest004, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    svc.disableMinidumpToCrashLogConfigs.clear();
    pid_t testPid = getpid();
    svc.disableMinidumpToCrashLogConfigs.emplace(testPid);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 1);
    int ret = svc.SetMiniDump(testPid, -1, 1);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerServiceSetMiniDumpTest005
 * @tc.desc: test SetMiniDump both disabled triggers clear_dumpable and erase
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerServiceSetMiniDumpTest005, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid();
    svc.enableMinidumpConfigs.erase(testPid);
    svc.disableMinidumpToCrashLogConfigs.emplace(testPid);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 1);
    int ret = svc.SetMiniDump(testPid, -1, -1);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 1);
}

/**
 * @tc.name: MinidumpManagerServiceSetMiniDumpTest006
 * @tc.desc: test SetMiniDump with pFd_ < 0 returns error
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerServiceSetMiniDumpTest006, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid();
    svc.enableMinidumpConfigs.erase(testPid);
    svc.disableMinidumpToCrashLogConfigs.erase(testPid);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
    int ret = svc.SetMiniDump(testPid, 1, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}
#endif

/**
 * @tc.name: TempFileManagerCreateFileDescriptorMinidumpTest001
 * @tc.desc: test CreateFileDescriptor for MINIDUMP type creates .dmp extension
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, TempFileManagerCreateFileDescriptorMinidumpTest001, TestSize.Level2)
{
    std::string filePath;
    SmartFd fd(TempFileManager::CreateFileDescriptor(
        FaultLoggerType::MINIDUMP, getpid(), gettid(), GetTimeMilliSeconds(), filePath));
    ASSERT_GE(fd.GetFd(), 0);
    EXPECT_TRUE(filePath.find(".dmp") != std::string::npos);
}

/**
 * @tc.name: TempFileManagerCreateFileDescriptorFilePathTest002
 * @tc.desc: test CreateFileDescriptor returns filePath for CPP_CRASH
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, TempFileManagerCreateFileDescriptorFilePathTest002, TestSize.Level2)
{
    std::string filePath;
    SmartFd fd(TempFileManager::CreateFileDescriptor(
        FaultLoggerType::CPP_CRASH, getpid(), gettid(), GetTimeMilliSeconds(), filePath));
    ASSERT_GE(fd.GetFd(), 0);
    EXPECT_TRUE(!filePath.empty());
    EXPECT_TRUE(filePath.find("cppcrash") != std::string::npos);
}
} // namespace HiviewDFX
} // namepsace OHOS
