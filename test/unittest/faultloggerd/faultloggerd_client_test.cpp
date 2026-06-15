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

constexpr int32_t ENABLE_FLAG = 1;
constexpr int32_t DISABLE_FLAG = 0;
constexpr int32_t UNCHANGED_FLAG = -1;
constexpr int32_t TEST_PID_OFFSET = 10000;
constexpr int32_t NONEXISTENT_TEST_PID = 4000000;
constexpr int32_t TEST_OUTPUT_BYTES = 256;

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
    ASSERT_GE(pipeFds[0], 0);
    ASSERT_GE(pipeFds[1], 0);
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
    data.enableMinidump = ENABLE_FLAG;
    data.enableMinidumpToCrashLog = ENABLE_FLAG;
    EXPECT_EQ(data.enableMinidump, ENABLE_FLAG);
    EXPECT_EQ(data.enableMinidumpToCrashLog, ENABLE_FLAG);
    data.enableMinidump = DISABLE_FLAG;
    data.enableMinidumpToCrashLog = DISABLE_FLAG;
    EXPECT_EQ(data.enableMinidump, DISABLE_FLAG);
    EXPECT_EQ(data.enableMinidumpToCrashLog, DISABLE_FLAG);
    data.enableMinidump = UNCHANGED_FLAG;
    data.enableMinidumpToCrashLog = UNCHANGED_FLAG;
    EXPECT_EQ(data.enableMinidump, UNCHANGED_FLAG);
    EXPECT_EQ(data.enableMinidumpToCrashLog, UNCHANGED_FLAG);
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
    int ret = svc.SetMiniDump(testPid, ENABLE_FLAG, UNCHANGED_FLAG);
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
    int ret = svc.SetMiniDump(testPid, DISABLE_FLAG, UNCHANGED_FLAG);
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
    int ret = svc.SetMiniDump(testPid, UNCHANGED_FLAG, DISABLE_FLAG);
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
    int ret = svc.SetMiniDump(testPid, UNCHANGED_FLAG, ENABLE_FLAG);
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
    int ret = svc.SetMiniDump(testPid, UNCHANGED_FLAG, UNCHANGED_FLAG);
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
    int ret = svc.SetMiniDump(testPid, ENABLE_FLAG, ENABLE_FLAG);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerParsePDumpDataWorkStartTest001
 * @tc.desc: test ParsePDumpData with __DATA_TYPE_WORK_START
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerParsePDumpDataWorkStartTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + TEST_PID_OFFSET;
    svc.enableMinidumpConfigs.erase(testPid);
    svc.disableMinidumpToCrashLogConfigs.emplace(testPid);

    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_START;
    data.header.workid = 100;
    data.data.work_data.pid = testPid;
    data.data.work_data.dump_type = __PDUMP_TYPE_MINIDUMP;
    data.data.work_data.pipefd = -1;

    bool result = svc.ParsePDumpData(data);
    EXPECT_TRUE(result);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerParsePDumpDataWorkEndTest001
 * @tc.desc: test ParsePDumpData with __DATA_TYPE_WORK_END
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerParsePDumpDataWorkEndTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_END;
    data.header.workid = 101;
    data.data.result_data.dump_type = __PDUMP_TYPE_MINIDUMP;
    data.data.result_data.errcode = 0;
    data.data.result_data.output_bytes = TEST_OUTPUT_BYTES;

    bool result = svc.ParsePDumpData(data);
    EXPECT_TRUE(result);
}

/**
 * @tc.name: MinidumpManagerParsePDumpDataInvalidTypeTest001
 * @tc.desc: test ParsePDumpData with invalid data_type returns false
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerParsePDumpDataInvalidTypeTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    struct __pdump_data_s data = {};
    data.header.data_type = static_cast<enum __pdump_data_type>(99);
    data.header.workid = 102;

    bool result = svc.ParsePDumpData(data);
    EXPECT_FALSE(result);
}

/**
 * @tc.name: MinidumpManagerProcessWorkStartCancelTest001
 * @tc.desc: test ProcessWorkStart with both configs disabled triggers cancel
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerProcessWorkStartCancelTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + TEST_PID_OFFSET + 1;
    svc.enableMinidumpConfigs.erase(testPid);
    svc.disableMinidumpToCrashLogConfigs.emplace(testPid);

    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_START;
    data.header.workid = 200;
    data.data.work_data.pid = testPid;
    data.data.work_data.dump_type = __PDUMP_TYPE_MINIDUMP;
    data.data.work_data.pipefd = -1;

    svc.ProcessWorkStart(data);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerProcessWorkStartEnableTest001
 * @tc.desc: test ProcessWorkStart with enableMinidump=true triggers fork path
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerProcessWorkStartEnableTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + TEST_PID_OFFSET + 2;
    svc.enableMinidumpConfigs.emplace(testPid);
    svc.disableMinidumpToCrashLogConfigs.erase(testPid);

    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_START;
    data.header.workid = 201;
    data.data.work_data.pid = testPid;
    data.data.work_data.dump_type = __PDUMP_TYPE_CALLSTACK;
    data.data.work_data.pipefd = -1;

    svc.ProcessWorkStart(data);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerProcessWorkStartCrashLogOnlyTest001
 * @tc.desc: test ProcessWorkStart with only enableMinidumpToCrashLog=true
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerProcessWorkStartCrashLogOnlyTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + TEST_PID_OFFSET + 3;
    svc.enableMinidumpConfigs.erase(testPid);
    svc.disableMinidumpToCrashLogConfigs.erase(testPid);

    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_START;
    data.header.workid = 202;
    data.data.work_data.pid = testPid;
    data.data.work_data.dump_type = __PDUMP_TYPE_COREDUMP;
    data.data.work_data.pipefd = -1;

    svc.ProcessWorkStart(data);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerProcessWorkEndDumpTypeTest001
 * @tc.desc: test ProcessWorkEnd covers all DumpTypeToString branches
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerProcessWorkEndDumpTypeTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    struct __pdump_data_s data = {};
    data.header.workid = 300;

    data.data.result_data.dump_type = __PDUMP_TYPE_CALLSTACK;
    data.data.result_data.errcode = 0;
    data.data.result_data.output_bytes = 50;
    svc.ProcessWorkEnd(data);

    data.data.result_data.dump_type = __PDUMP_TYPE_MINIDUMP;
    svc.ProcessWorkEnd(data);

    data.data.result_data.dump_type = __PDUMP_TYPE_COREDUMP;
    svc.ProcessWorkEnd(data);

    data.data.result_data.dump_type = __PDUMP_TYPE_INVALID;
    svc.ProcessWorkEnd(data);
}

/**
 * @tc.name: MinidumpManagerProcessEnableMinidumpConfigsTest001
 * @tc.desc: test ProcessEnableMinidumpConfigs erases pid from configs
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerProcessEnableMinidumpConfigsTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + TEST_PID_OFFSET + 4;
    svc.enableMinidumpConfigs.emplace(testPid);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);

    svc.ProcessEnableMinidumpConfigs(testPid);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
}

/**
 * @tc.name: PDumpListenerOnEventPollIncompleteDataTest001
 * @tc.desc: test PDumpListener with incomplete pdump data
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, PDumpListenerOnEventPollIncompleteDataTest001, TestSize.Level2)
{
    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);
    int flags = fcntl(pipeFds[0], F_GETFL, 0);
    fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);

    char partialData[10] = {0};
    write(pipeFds[1], partialData, sizeof(partialData));
    close(pipeFds[1]);

    auto listener = std::make_unique<PDumpListener>(SmartFd{pipeFds[0]}, true);
    listener->OnEventPoll();
}

/**
 * @tc.name: PDumpListenerOnEventPollCompleteDataTest001
 * @tc.desc: test PDumpListener with complete WORK_END pdump data
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, PDumpListenerOnEventPollCompleteDataTest001, TestSize.Level2)
{
    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);
    int flags = fcntl(pipeFds[0], F_GETFL, 0);
    fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);

    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_END;
    data.header.workid = 400;
    data.data.result_data.dump_type = __PDUMP_TYPE_MINIDUMP;
    data.data.result_data.errcode = 0;
    data.data.result_data.output_bytes = TEST_OUTPUT_BYTES;

    write(pipeFds[1], &data, sizeof(data));
    close(pipeFds[1]);

    auto listener = std::make_unique<PDumpListener>(SmartFd{pipeFds[0]}, true);
    listener->OnEventPoll();
}

/**
 * @tc.name: PDumpListenerOnEventPollWorkStartCancelTest001
 * @tc.desc: test PDumpListener with WORK_START data (cancel path)
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, PDumpListenerOnEventPollWorkStartCancelTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + 20000;
    svc.enableMinidumpConfigs.erase(testPid);
    svc.disableMinidumpToCrashLogConfigs.emplace(testPid);

    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);
    int flags = fcntl(pipeFds[0], F_GETFL, 0);
    fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);

    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_START;
    data.header.workid = 401;
    data.data.work_data.pid = testPid;
    data.data.work_data.dump_type = __PDUMP_TYPE_CALLSTACK;
    data.data.work_data.pipefd = -1;

    write(pipeFds[1], &data, sizeof(data));
    close(pipeFds[1]);

    auto listener = std::make_unique<PDumpListener>(SmartFd{pipeFds[0]}, true);
    listener->OnEventPoll();

    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
    EXPECT_EQ(svc.disableMinidumpToCrashLogConfigs.count(testPid), 0);
}

/**
 * @tc.name: PDumpListenerOnEventPollWorkStartEnableTest001
 * @tc.desc: test PDumpListener with WORK_START data (fork path)
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, PDumpListenerOnEventPollWorkStartEnableTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + 20001;
    svc.enableMinidumpConfigs.emplace(testPid);
    svc.disableMinidumpToCrashLogConfigs.erase(testPid);

    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);
    int flags = fcntl(pipeFds[0], F_GETFL, 0);
    fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);

    struct __pdump_data_s data = {};
    data.header.data_type = __DATA_TYPE_WORK_START;
    data.header.workid = 402;
    data.data.work_data.pid = testPid;
    data.data.work_data.dump_type = __PDUMP_TYPE_MINIDUMP;
    data.data.work_data.pipefd = -1;

    write(pipeFds[1], &data, sizeof(data));
    close(pipeFds[1]);

    auto listener = std::make_unique<PDumpListener>(SmartFd{pipeFds[0]}, true);
    listener->OnEventPoll();

    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
}

/**
 * @tc.name: PDumpListenerOnEventPollReadErrorTest001
 * @tc.desc: test PDumpListener with invalid fd covers read error branch
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, PDumpListenerOnEventPollReadErrorTest001, TestSize.Level2)
{
    auto listener = std::make_unique<PDumpListener>(SmartFd{-1}, true);
    listener->OnEventPoll();
}

/**
 * @tc.name: PDumpListenerOnEventPollEagainTest001
 * @tc.desc: test PDumpListener with empty non-blocking pipe covers EAGAIN path
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, PDumpListenerOnEventPollEagainTest001, TestSize.Level2)
{
    int pipeFds[2];
    ASSERT_EQ(pipe(pipeFds), 0);
    int flags = fcntl(pipeFds[0], F_GETFL, 0);
    fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);

    auto listener = std::make_unique<PDumpListener>(SmartFd{pipeFds[0]}, true);
    listener->OnEventPoll();
    close(pipeFds[1]);
}

/**
 * @tc.name: PidFdListenerOnEventPollTest001
 * @tc.desc: test PidFdListener::OnEventPoll calls ProcessEnableMinidumpConfigs
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, PidFdListenerOnEventPollTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + 30000;
    svc.enableMinidumpConfigs.emplace(testPid);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);

    int fd = open("/dev/null", O_RDONLY);
    ASSERT_GE(fd, 0);

    auto listener = std::make_unique<PidFdListener>(SmartFd{fd});
    listener->SetPid(testPid);
    listener->OnEventPoll();

    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 0);
}

/**
 * @tc.name: MinidumpManagerSetMiniDumpPfdNegativeTest001
 * @tc.desc: test SetMiniDump returns -1 when pFd_ < 0 on new instance
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerSetMiniDumpPfdNegativeTest001, TestSize.Level2)
{
    MinidumpManagerService newInstance;
    pid_t testPid = getpid() + 40000;
    int ret = newInstance.SetMiniDump(testPid, ENABLE_FLAG, ENABLE_FLAG);
    EXPECT_EQ(ret, -1);
}

/**
 * @tc.name: MinidumpManagerSetMiniDumpAlreadyInConfigsTest001
 * @tc.desc: test SetMiniDump enableMinidump=1 with pid already in configs
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerSetMiniDumpAlreadyInConfigsTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t testPid = getpid() + 50000;
    svc.enableMinidumpConfigs.emplace(testPid);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);

    int ret = svc.SetMiniDump(testPid, ENABLE_FLAG, UNCHANGED_FLAG);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(testPid), 1);

    svc.enableMinidumpConfigs.erase(testPid);
}

/**
 * @tc.name: MinidumpManagerSetMiniDumpPidfdOpenFailTest001
 * @tc.desc: test SetMiniDump with non-existent pid triggers pidfd_open failure
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerSetMiniDumpPidfdOpenFailTest001, TestSize.Level2)
{
    auto& svc = MinidumpManagerService::GetInstance();
    pid_t nonExistPid = NONEXISTENT_TEST_PID;
    svc.enableMinidumpConfigs.erase(nonExistPid);

    int ret = svc.SetMiniDump(nonExistPid, ENABLE_FLAG, UNCHANGED_FLAG);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(svc.enableMinidumpConfigs.count(nonExistPid), 0);
}

/**
 * @tc.name: MinidumpManagerInitNewInstanceTest001
 * @tc.desc: test Init on a new instance covers open and ioctl branches
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdClientTest, MinidumpManagerInitNewInstanceTest001, TestSize.Level2)
{
    MinidumpManagerService newInstance;
    bool result = newInstance.Init();
    GTEST_LOG_(INFO) << "MinidumpManagerInitNewInstanceTest001 result: " << result;
    if (result) {
        close(newInstance.pFd_);
        newInstance.pFd_ = -1;
    }
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
    EXPECT_FALSE(filePath.empty());
    EXPECT_TRUE(filePath.find("cppcrash") != std::string::npos);
}
} // namespace HiviewDFX
} // namespace OHOS
