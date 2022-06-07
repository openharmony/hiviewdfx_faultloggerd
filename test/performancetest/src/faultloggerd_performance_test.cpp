/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains faultlog performance st test case. */

#include "faultloggerd_performance_test.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <securec.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "syscall.h"
#include "directory_ex.h"
#include "file_ex.h"
#include "dfx_dump_catcher.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

clock_t GetStartTime ()
{
    return clock();
}

double GetStopTime(clock_t befor)
{
    clock_t StartTimer = clock();
    return  ((StartTimer - befor) / double(CLOCKS_PER_SEC));
}

void FaultPerformanceTest::SetUpTestCase(void)
{
}

void FaultPerformanceTest::TearDownTestCase(void)
{
}

void FaultPerformanceTest::SetUp(void)
{
}

void FaultPerformanceTest::TearDown(void)
{
}

int FaultPerformanceTest::looprootPid = 0;
std::string FaultPerformanceTest::ProcessDumpCommands(const std::string cmds)
{
    GTEST_LOG_(INFO) << "threadCMD = " << cmds;
    FILE *procFileInfo = nullptr;
    std::string cmdLog;
    procFileInfo = popen(cmds.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    char result_buf_shell[PERFORMANCE_TEST_NUMBER_ONE_HUNDRED] = {'\0'};
    while (fgets(result_buf_shell, sizeof(result_buf_shell), procFileInfo) != nullptr) {
        cmdLog = cmdLog + result_buf_shell;
    }
    pclose(procFileInfo);
    return cmdLog;
}

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
    char result_buf_shell[PERFORMANCE_TEST_NUMBER_ONE_HUNDRED] = { 0, };
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

int FaultPerformanceTest::getApplyPid(std::string applyName)
{
    std::string procCMD = "pgrep '" + applyName + "'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMD;
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    std::string applyPid;
    char result_buf_shell[100] = { 0, };
    while (fgets(result_buf_shell, sizeof(result_buf_shell), procFileInfo) != nullptr) {
        applyPid = result_buf_shell;
        GTEST_LOG_(INFO) << "applyPid: " << applyPid;
    }
    pclose(procFileInfo);
    GTEST_LOG_(INFO) << applyPid;
    int intApplyPid = std::atoi(applyPid.c_str());
    return intApplyPid;
}

/**
 * @tc.name: FaultPerformanceTest001
 * @tc.desc: test DumpCatch API: PID(root), TID(root)
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest001: start.";
    FaultPerformanceTest::StartRootCrasherLoop();
    DfxDumpCatcher dumplog;
    std::string msg;
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        dumplog.DumpCatch(FaultPerformanceTest::looprootPid, FaultPerformanceTest::looprootPid, msg);
    }
    GTEST_LOG_(INFO) << "DumpCatch API Performance time(PID(root), TID(root)): " << \
        GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED << "s";    
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest001 Failed";
    FaultPerformanceTest::KillCrasherLoopForSomeCase();
    GTEST_LOG_(INFO) << "FaultPerformanceTest001: end.";
}

/**
 * @tc.name: FaultPerformanceTest002
 * @tc.desc: test DumpCatch API: PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest002: start.";
    FaultPerformanceTest::StartRootCrasherLoop();
    DfxDumpCatcher dumplog;
    std::string msg;
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        dumplog.DumpCatch(FaultPerformanceTest::looprootPid, 0, msg);
        sleep(1);
    }
    GTEST_LOG_(INFO) << "DumpCatch API Performance time(PID(root), TID(0)): " << \
        GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED - 1;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest002 Failed";
    FaultPerformanceTest::KillCrasherLoopForSomeCase();
    GTEST_LOG_(INFO) << "FaultPerformanceTest002: end.";
}

/**
 * @tc.name: FaultPerformanceTest003
 * @tc.desc: test processdump command: PID(root), TID(root)
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest003: start.";
    FaultPerformanceTest::StartRootCrasherLoop();
    std::string procCMD = "processdump -p " + std::to_string(FaultPerformanceTest::looprootPid) + " -t "+
        std::to_string(FaultPerformanceTest::looprootPid);
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        FaultPerformanceTest::ProcessDumpCommands(procCMD);
    }
    double timeInterval = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    GTEST_LOG_(INFO) << "Processdump Command Performance time(PID(root), TID(root)): " << timeInterval << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest003 Failed";
    FaultPerformanceTest::KillCrasherLoopForSomeCase();
    GTEST_LOG_(INFO) << "FaultPerformanceTest003: end.";
}

/**
 * @tc.name: FaultPerformanceTest004
 * @tc.desc: test DumpCatch API: PID(root)
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest004: start.";
    FaultPerformanceTest::StartRootCrasherLoop();
    std::string procCMD = "processdump -p " + std::to_string(FaultPerformanceTest::looprootPid);
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        FaultPerformanceTest::ProcessDumpCommands(procCMD);
    }
    GTEST_LOG_(INFO) << "Processdump Command Performance time(PID(root)): " << \
        GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED << "s";

    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest004 Failed";
    FaultPerformanceTest::KillCrasherLoopForSomeCase();
    GTEST_LOG_(INFO) << "FaultPerformanceTest004: end.";
}

/**
 * @tc.name: FaultPerformanceTest005
 * @tc.desc: test DumpCatchMultiPid API: PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest005: start.";
    FaultPerformanceTest::StartRootCrasherLoop();
    DfxDumpCatcher dumplog;
    std::string msg;
    std::string apply = "foundation";
    int applyPid = FaultPerformanceTest::getApplyPid(apply);
    std::vector<int> multiPid {applyPid, FaultPerformanceTest::looprootPid};
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        dumplog.DumpCatchMultiPid(multiPid, msg);
    }
    double timeInterval = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    GTEST_LOG_(INFO) << "DumpCatchMultiPid API time(PID(root), PID(foundation)): " << timeInterval << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest005 Failed";
    FaultPerformanceTest::KillCrasherLoopForSomeCase();
    GTEST_LOG_(INFO) << "FaultPerformanceTest005: end.";
}

/**
 * @tc.name: FaultPerformanceTest006
 * @tc.desc: test DumpCatchFrame API: app PID(app), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest006: start.";
    std::string apply = "test_perfor";
    int testPid = FaultPerformanceTest::getApplyPid(apply);
    GTEST_LOG_(INFO) << testPid;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    std::vector<std::shared_ptr<DfxDumpCatcherFrame>> frameV;
    clock_t befor = GetStartTime();
    for (int i = 0; i < PERFORMANCE_TEST_NUMBER_ONE_HUNDRED; i++) {
        bool ret = dumplog.DumpCatchFrame(testPid, testPid, msg, frameV);
        GTEST_LOG_(INFO) << ret;
    }
    double timeInterval = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    GTEST_LOG_(INFO) << "DumpCatchFrame API time(PID(test_per), PID(test_per)):" << timeInterval << "s";
    double expectTime = PERFORMANCE_TEST_MAX_UNWIND_TIME_S;
    double realTime = GetStopTime(befor)/PERFORMANCE_TEST_NUMBER_ONE_HUNDRED;
    EXPECT_EQ(true, realTime < expectTime) << "FaultPerformanceTest006 Failed";
    GTEST_LOG_(INFO) << "FaultPerformanceTest006: end.";
}
