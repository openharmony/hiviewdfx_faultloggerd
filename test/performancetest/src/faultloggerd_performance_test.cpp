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

#include "faultloggerd_performance_test.h"

#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_define.h"
#include "dfx_dump_catcher.h"
#include "catchframe_local.h"
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace {
static const int PERFORMANCE_TEST_NUMBER_ONE_HUNDRED = 100;
static const double PERFORMANCE_TEST_MAX_UNWIND_TIME_S = 0.03;
static const double PERFORMANCE_TEST_MAX_UNWIND_TIME_NEW_S = 0.04;

clock_t GetStartTime()
{
    return clock();
}

double GetStopTime(clock_t befor)
{
    clock_t StartTimer = clock();
    return  ((StartTimer - befor) / double(CLOCKS_PER_SEC));
}
}

void FaultPerformanceTest::SetUpTestCase(void)
{
}

void FaultPerformanceTest::TearDownTestCase(void)
{
}

void FaultPerformanceTest::SetUp(void)
{
    GTEST_LOG_(INFO) << "SetUp";
    FaultPerformanceTest::StartRootCrasherLoop();
}

void FaultPerformanceTest::TearDown(void)
{
    GTEST_LOG_(INFO) << "TearDown";
    FaultPerformanceTest::KillCrasherLoopForSomeCase();
}

int FaultPerformanceTest::looprootPid = 0;

std::string FaultPerformanceTest::ForkAndRootCommands(const std::vector<std::string>& cmds)
{
    int rootuid = 0;
    setuid(rootuid);
    system("/data/crasher_c thread-Loop &");
    std::string procCMD = "pgrep 'crasher'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMD;
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    std::string pidLog;
    char result_buf_shell[NAME_BUF_LEN] = { 0, };
    if (fgets(result_buf_shell, sizeof(result_buf_shell), procFileInfo) != nullptr) {
        pidLog = result_buf_shell;
        looprootPid = atoi(pidLog.c_str());
    }
    pclose(procFileInfo);
    return std::to_string(looprootPid);
}

void FaultPerformanceTest::StartRootCrasherLoop()
{
    int rootuid = 0;
    setuid(rootuid);
    std::vector<std::string> cmds { "crasher", "thread-Loop" };
    FaultPerformanceTest::ForkAndRootCommands(cmds);
    if (looprootPid == 0) {
        exit(0);
    }
}

void FaultPerformanceTest::KillCrasherLoopForSomeCase()
{
    int rootuid = 0;
    setuid(rootuid);
    system(("kill -9 " + std::to_string(FaultPerformanceTest::looprootPid)).c_str());
}

namespace {
/**
 * @tc.name: FaultPerformanceTest001
 * @tc.desc: test DumpCatch API: PID(root), TID(root)
 * @tc.type: PERF
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest001: start.";
    DfxDumpCatcher dumplog;
    std::string msg;
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        dumplog.DumpCatch(FaultPerformanceTest::looprootPid, FaultPerformanceTest::looprootPid, msg);
    }
    GTEST_LOG_(INFO) << "DumpCatch API Performance time(PID(root), TID(root)): " <<
        GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest001 Failed";
    GTEST_LOG_(INFO) << "FaultPerformanceTest001: end.";
}

/**
 * @tc.name: FaultPerformanceTest002
 * @tc.desc: test DumpCatch API: PID(root), TID(0)
 * @tc.type: PERF
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest002: start.";
    DfxDumpCatcher dumplog;
    std::string msg;
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        dumplog.DumpCatch(FaultPerformanceTest::looprootPid, 0, msg);
        usleep(200000); // 200000 : sleep 200ms
    }
    GTEST_LOG_(INFO) << "DumpCatch API Performance time(PID(root), TID(0)): " <<
        (GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED) << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = (GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED) - 0.2; // 0.2 : 200ms
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest002 Failed";
    GTEST_LOG_(INFO) << "FaultPerformanceTest002: end.";
}

/**
 * @tc.name: FaultPerformanceTest003
 * @tc.desc: test dumpcatcher command: PID(root), TID(root)
 * @tc.type: PERF
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest003: start.";
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultPerformanceTest::looprootPid) + " -t "+
        std::to_string(FaultPerformanceTest::looprootPid);
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        ExecuteCommands(procCMD);
    }
    double timeInterval = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    GTEST_LOG_(INFO) << "dumpcatcher Command Performance time(PID(root), TID(root)): " << timeInterval << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest003 Failed";
    GTEST_LOG_(INFO) << "FaultPerformanceTest003: end.";
}

/**
 * @tc.name: FaultPerformanceTest004
 * @tc.desc: test DumpCatch API: PID(root)
 * @tc.type: PERF
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest004: start.";
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultPerformanceTest::looprootPid);
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        ExecuteCommands(procCMD);
    }
    GTEST_LOG_(INFO) << "dumpcatcher Command Performance time(PID(root)): " <<
        (GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED) << "s";

    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest004 Failed";
    GTEST_LOG_(INFO) << "FaultPerformanceTest004: end.";
}

/**
 * @tc.name: FaultPerformanceTest005
 * @tc.desc: test DumpCatchMultiPid API: PID(root), TID(0)
 * @tc.type: PERF
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest005: start.";
    DfxDumpCatcher dumplog;
    std::string msg;
    std::string name = "foundation";
    int pid = GetProcessPid(name);
    std::vector<int> multiPid {pid, FaultPerformanceTest::looprootPid};
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        dumplog.DumpCatchMultiPid(multiPid, msg);
    }
    double timeInterval = GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    GTEST_LOG_(INFO) << "DumpCatchMultiPid API time(PID(root), PID(foundation)): " << timeInterval << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_NEW_S;
    double realTime = GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest005 Failed";
    GTEST_LOG_(INFO) << "FaultPerformanceTest005: end.";
}

/**
 * @tc.name: FaultPerformanceTest006
 * @tc.desc: test DumpCatchFrame API: app PID(app), TID(0)
 * @tc.type: PERF
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest006: start.";
    std::string name = "test_perfor";
    int testPid = GetProcessPid(name);
    GTEST_LOG_(INFO) << testPid;
    DfxCatchFrameLocal dumplog(testPid);
    if (!dumplog.InitFrameCatcher()) {
        GTEST_LOG_(ERROR) << "Failed to suspend thread(" << testPid << ").";
    }
    std::vector<DfxFrame> frameV;
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        bool ret = dumplog.CatchFrame(testPid, frameV, 0, true);
        GTEST_LOG_(INFO) << ret;
    }
    double timeInterval = GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    GTEST_LOG_(INFO) << "DumpCatchFrame API time(PID(test_per), PID(test_per)):" << timeInterval << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor) / PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest006 Failed";
    GTEST_LOG_(INFO) << "FaultPerformanceTest006: end.";
}
}
