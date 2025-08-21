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
#include "coredump_manager_service.h"
#include "faultloggerd_test.h"
#include "fault_logger_daemon.h"
#include "smart_fd.h"
#include "coredump_callback_service.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class FaultloggerdCoredumpTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
};

void FaultloggerdCoredumpTest::TearDownTestCase()
{
}

void FaultloggerdCoredumpTest::SetUpTestCase()
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
 HWTEST_F(FaultloggerdCoredumpTest, SendSignalToHapWatchdogThreadTest, TestSize.Level2)
 {
    GTEST_LOG_(INFO) << "SendSignalToHapWatchdogThreadTest: start.";
    pid_t pid = -1;
    siginfo_t si = {};
    EXPECT_EQ(FaultCommonUtil::SendSignalToHapWatchdogThread(pid, si), ResponseCode::DEFAULT_ERROR_CODE);

    pid = GetProcessPid("com.ohos.sceneboard");
    FaultCommonUtil::SendSignalToHapWatchdogThread(pid, si);

    GTEST_LOG_(INFO) << "SendSignalToHapWatchdogThreadTest: end.";
}

/**
 * @tc.name: SendSignalToProcessTest
 * @tc.desc: SendSignalToProcess
 * @tc.type: FUNC
 */
 HWTEST_F(FaultloggerdCoredumpTest, SendSignalToProcessTest, TestSize.Level2)
 {
    GTEST_LOG_(INFO) << "SendSignalToProcessTest: start.";
    pid_t pid = -1;
    siginfo_t si = {};
    EXPECT_EQ(FaultCommonUtil::SendSignalToProcess(pid, si), ResponseCode::REQUEST_SUCCESS);
    pid = GetProcessPid("com.ohos.sceneboard");
    EXPECT_EQ(FaultCommonUtil::SendSignalToProcess(pid, si), ResponseCode::REQUEST_SUCCESS);
    GTEST_LOG_(INFO) << "SendSignalToProcessTest: end.";
}

/**
  * @tc.name: SendStartSignal
  * @tc.desc: SendSignalToProcess
  * @tc.type: FUNC
  */
 HWTEST_F(FaultloggerdCoredumpTest, SendStartSignal, TestSize.Level2)
 {
    GTEST_LOG_(INFO) << "SendStartSignal: start.";
    pid_t pid = -1;
    siginfo_t si = {};
    CoredumpSignalService().SendStartSignal(pid);
    GTEST_LOG_(INFO) << "SendStartSignal: end.";
}

/**
 * @tc.name: GetUcredByPeerCredTest
 * @tc.desc: GetUcredByPeerCred
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, GetUcredByPeerCredTest, TestSize.Level2)
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
HWTEST_F(FaultloggerdCoredumpTest, HandleProcessDumpPidTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest001: start.";
    int32_t targetPid = 123;
    int32_t processDumpPid = -1;

    CoreDumpStatusData data;
    CoredumpCallbackService coredumpStatusService;
    coredumpStatusService.HandleUpdateWorkerPid(data);

    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest001: end.";
}

/**
 * @tc.name: HandleProcessDumpPidTest002
 * @tc.desc: HandleProcessDumpPid
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, HandleProcessDumpPidTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest002: start.";
    int32_t targetPid = 123;
    int32_t processDumpPid = 456;

    CoreDumpStatusData data;
    CoredumpCallbackService coredumpStatusService;
    coredumpStatusService.HandleUpdateWorkerPid(data);

    auto ret = coredumpStatusService.OnRequest("", -1, data);
    EXPECT_EQ(ret, ResponseCode::REQUEST_REJECT);
    GTEST_LOG_(INFO) << "HandleProcessDumpPidTest002: end.";
}

/**
 * @tc.name: DoCoredumpRequestTest
 * @tc.desc: DoCoredumpRequest
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, DoCoredumpRequestTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DoCoredumpRequestTest: start.";
    std::string socketName = "test_socket";
    int32_t connectionFd = 1;
    CoreDumpRequestData requestData = {
        .pid = 666
    };
    CoredumpManagerService coredumpService;
    auto ret = coredumpService.HandleCreate(connectionFd, requestData);
    coredumpService.HandleCancel(requestData);
    requestData.pid = 999;
    coredumpService.HandleCancel(requestData);

    GTEST_LOG_(INFO) << "DoCoredumpRequestTest: end.";
}

/**
 * @tc.name: RecorderProcessMapTest
 * @tc.desc: test sessionManager
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, RecorderProcessMapTest, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RecorderProcessMapTest: start.";
    auto& sessionManager = CoredumpSessionManager::Instance();
    sessionManager.RemoveSession(0);
    GTEST_LOG_(INFO) << "RecorderProcessMapTest: end.";
}

/**
 * @tc.name: CoreDumpCbTest001
 * @tc.desc: Test startcoredumpcb and finishcoredumpcb process
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, CoreDumpCbTest001, TestSize.Level2)
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
HWTEST_F(FaultloggerdCoredumpTest, CoreDumpCbTest002, TestSize.Level2)
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
HWTEST_F(FaultloggerdCoredumpTest, CoreDumpCbTest003, TestSize.Level2)
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
HWTEST_F(FaultloggerdCoredumpTest, CoreDumpCbTest004, TestSize.Level2)
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
HWTEST_F(FaultloggerdCoredumpTest, CoreDumpCbTest005, TestSize.Level2)
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
HWTEST_F(FaultloggerdCoredumpTest, CoreDumpCbTest006, TestSize.Level2)
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
HWTEST_F(FaultloggerdCoredumpTest, CoreDumpCbTest007, TestSize.Level2)
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
 * @tc.name: CoredumpSessionService001
 * @tc.desc: test CoredumpSessionService
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, CoredumpSessionService001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpSessionService001: start.";
    CoredumpSessionService  sessionService;

    CreateCoredumpRequest request;
    pid_t testPid = 666;
    pid_t validPid = 999;
    request.targetPid = 666;

    auto sessionId = sessionService.CreateSession(request);
    EXPECT_EQ(sessionId, testPid);

    auto ret = sessionService.UpdateWorkerPid(sessionId, 124);
    EXPECT_TRUE(ret);
    ret = sessionService.UpdateWorkerPid(validPid, 124);
    EXPECT_FALSE(ret);

    CoredumpCallbackReport reportReq;
    sessionService.UpdateReport(sessionId, reportReq);
    sessionService.UpdateReport(validPid, reportReq);

    sessionService.GetClientFd(sessionId);
    sessionService.GetClientFd(validPid);

    sessionService.WriteTimeoutAndClose(sessionId);
    sessionService.WriteTimeoutAndClose(validPid);

    sessionService.WriteResultAndClose(sessionId);
    sessionService.WriteResultAndClose(validPid);

    CoreDumpResult result;
    sessionService.WriteResult(sessionId, result);
    sessionService.WriteResult(validPid, result);

    ret = sessionService.CancelSession(sessionId);
    ret = sessionService.CancelSession(validPid);
    EXPECT_FALSE(ret);

    GTEST_LOG_(INFO) << "CoredumpSessionService001: end.";
}

/**
 * @tc.name: FaultCoredumpConfig001
 * @tc.desc: test FaultCoredumpConfig
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, FaultCoredumpConfig001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultCoredumpConfig001: start.";
    FaultCoredumpConfig::GetInstance();

    string path = "/system/etc/fault_coredump.json";
    auto& coredumpConfig = FaultCoredumpConfig::GetInstance(path);

    coredumpConfig.Contains(999);

    std::string content = R"({
        "Profiles": {

    })";
    auto ret = coredumpConfig.Parse(content);
    EXPECT_FALSE(ret);

    content = R"({
        "Profiles": {
        }
    })";

    ret = coredumpConfig.Parse(content);
    EXPECT_FALSE(ret);

    content = R"({
        "whitelist": 123
    })";
    ret = coredumpConfig.Parse(content);
    EXPECT_FALSE(ret);
    content = R"({
        "whitelist": ["ABC"]
    })";
    ret = coredumpConfig.Parse(content);

    content = R"({
        "whitelist": [0, 134]
    })";
    ret = coredumpConfig.Parse(content);
    EXPECT_TRUE(ret);

    GTEST_LOG_(INFO) << "FaultCoredumpConfig001: end.";
}

/**
 * @tc.name: CoredumpRequestValidator001
 * @tc.desc: test CoredumpRequestValidator
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, CoredumpRequestValidator001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpRequestValidator001: start.";
    CoredumpRequestValidator validator;
    std::string socketName = "server";
    int32_t connectionFd = -1;
    CoreDumpRequestData request;
    request.pid = -1;
    validator.ValidateRequest(socketName, -1, request);

    request.pid = 666;
    validator.ValidateRequest(socketName, -1, request);

    socketName = "faultloggerd.server";
    validator.ValidateRequest(socketName, -1, request);

    auto ret = CoredumpRequestValidator::CheckCoredumpUID(9999);
    EXPECT_FALSE(ret);

    int fd = open("dev/null", O_RDONLY);
    CoredumpRequestValidator::IsAuthorizedUid(fd);
    close(fd);

    GTEST_LOG_(INFO) << "CoredumpRequestValidator001: end.";
}

/**
 * @tc.name: CoredumpFacade001
 * @tc.desc: test facade
 * @tc.type: FUNC
 */
HWTEST_F(FaultloggerdCoredumpTest, CoredumpFacade001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpFacade001: start.";

    CoredumpFacade facade;
    CreateCoredumpRequest request;
    request.targetPid = 666;
    bool ret = facade.CreateCoredump(request);
    EXPECT_TRUE(ret);

    facade.OnTimeout(666);
    facade.OnTimeout(999);

    request.targetPid = 555;
    facade.CreateCoredump(request);
    facade.CancelCoredump(999);
    facade.CancelCoredump(555);

    GTEST_LOG_(INFO) << "CoredumpFacade001: end.";
}

/**
  * @tc.name: SessionStateContext001
  * @tc.desc: test SessionStateContext
  * @tc.type: FUNC
  */
HWTEST_F(FaultloggerdCoredumpTest, SessionStateContext001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SessionStateContext001: start.";

    auto ret = SessionStateContext::CreateState(CoredumpStatus::PENDING);
    EXPECT_TRUE(ret);
    SessionStateContext::CreateState(CoredumpStatus::RUNNING);
    SessionStateContext::CreateState(CoredumpStatus::SUCCESS);
    SessionStateContext::CreateState(CoredumpStatus::FAILED);
    SessionStateContext::CreateState(CoredumpStatus::CANCEL_PENDING);
    SessionStateContext::CreateState(CoredumpStatus::CANCELED);
    SessionStateContext::CreateState(CoredumpStatus::CANCEL_TIMEOUT);
    SessionStateContext::CreateState(CoredumpStatus::PENDING);
    ret = SessionStateContext::CreateState(CoredumpStatus::INVALID_STATUS);
    EXPECT_FALSE(ret);

    SessionStateContext stateContext;
    CoredumpEvent event = CoredumpEvent::TIMEOUT;
    CoredumpSession session;
    session.status = CoredumpStatus::INVALID_STATUS;
    stateContext.HandleEvent(session, event);

    GTEST_LOG_(INFO) << "SessionStateContext001: end.";
}

/**
  * @tc.name: SessionStateContext002
  * @tc.desc: RecorderProcessMap
  * @tc.type: FUNC
  */
HWTEST_F(FaultloggerdCoredumpTest, SessionStateContext002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SessionStateContext002: start.";
    SessionStateContext stateContext;
    CoredumpEvent event = CoredumpEvent::INVALID_EVENT;

    CoredumpSession session;
    session.status = CoredumpStatus::INVALID_STATUS;
    stateContext.HandleEvent(session, event);

    auto pendingState = SessionStateContext::CreateState(CoredumpStatus::PENDING);
    pendingState->OnEvent(CoredumpEvent::INVALID_EVENT, session);

    auto runningState = SessionStateContext::CreateState(CoredumpStatus::RUNNING);
    runningState->OnEvent(CoredumpEvent::REPORT_FAIL, session);
    runningState->OnEvent(CoredumpEvent::INVALID_EVENT, session);

    auto cancelPendingState = SessionStateContext::CreateState(CoredumpStatus::CANCEL_PENDING);
    cancelPendingState->OnEvent(CoredumpEvent::TIMEOUT, session);
    cancelPendingState->OnEvent(CoredumpEvent::INVALID_EVENT, session);
    EXPECT_TRUE(cancelPendingState);

    pendingState->OnEvent(CoredumpEvent::INVALID_EVENT, session);
    GTEST_LOG_(INFO) << "SessionStateContext002: end.";
}

/**
  * @tc.name: CoredumpCallbackValidator001
  * @tc.desc: RecorderProcessMap
  * @tc.type: FUNC
  */
HWTEST_F(FaultloggerdCoredumpTest, CoredumpCallbackValidator001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpCallbackValidator001: start.";

    std::string socketName = "server";
    int32_t connectionFd = 999;
    CoreDumpStatusData data;
    data.pid = -1;

    auto ret = CoredumpCallbackValidator::ValidateRequest(socketName, connectionFd, data);
    EXPECT_EQ(ret, ResponseCode::REQUEST_REJECT);

    data.pid = getpid();
    CoredumpCallbackValidator::ValidateRequest(socketName, connectionFd, data);
    EXPECT_EQ(ret, ResponseCode::REQUEST_REJECT);
    GTEST_LOG_(INFO) << "CoredumpCallbackValidator001: end.";
}

/**
  * @tc.name: CoredumpTaskScheduler001
  * @tc.desc: RecorderProcessMap
  * @tc.type: FUNC
  */
HWTEST_F(FaultloggerdCoredumpTest, CoredumpTaskScheduler001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpTaskScheduler001: start.";

    CoredumpTaskScheduler scheduler;
    auto& sessionManager = CoredumpSessionManager::Instance();

    CreateCoredumpRequest request;
    request.targetPid = 666;
    request.endTime = GetAbsTimeMilliSeconds();
    scheduler.ScheduleCancelTime(request.targetPid, 1 * 1000);
    sessionManager.CreateSession(request);
    scheduler.ScheduleCancelTime(request.targetPid, 1 * 1000);
    CoredumpSession* session = sessionManager.GetSession(request.targetPid);
    EXPECT_TRUE(session);
    sleep(2);
    session = sessionManager.GetSession(request.targetPid);
    GTEST_LOG_(INFO) << "CoredumpTaskScheduler001: end.";

    scheduler.CancelTimeout(-999);
}
} // namespace HiviewDFX
} // namepsace OHOS
