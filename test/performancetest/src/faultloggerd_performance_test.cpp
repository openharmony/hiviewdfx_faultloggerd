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
    char result_buf_shell[100] = { 0, };
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
    char result_buf_shell[100] = { 0, };
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
    DfxDumpCatcher dumplog;
    std::string msg = "";
    dumplog.DumpCatch(looprootPid, 0, msg);
    int sleepSecond = 5;
    usleep(sleepSecond);
    std::string procCMD = "ls /proc/" + std::to_string(looprootPid) + "/task";
    FILE *procFileInfo = nullptr;

    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    pclose(procFileInfo);
}

void GetTimeLog(std::string errorCMD)
{
    std::string startBytrace = "bytrace --trace_begin";
    system(startBytrace.c_str());
    std::string setBytrace = "param set debug.bytrace.tags.enableflags 1073741824";
    system(setBytrace.c_str());
    std::string startCrash = "/data/crasher_c " + errorCMD;
    system(startCrash.c_str());
    std::string dumpTime = "bytrace --trace_dump";
    GTEST_LOG_(INFO) << "threadCMD = " << dumpTime;
    FILE *procFileInfo = nullptr;
    char ResultBuf[3000];
    procFileInfo = popen(dumpTime.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    while (fgets(ResultBuf, sizeof(ResultBuf), procFileInfo) != nullptr) {
        GTEST_LOG_(INFO) << ResultBuf;
    }
    std::string stopBytrace = "bytrace  --trace_finish";
    system(stopBytrace.c_str());
    pclose(procFileInfo);
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
    dumplog.DumpCatch(FaultPerformanceTest::looprootPid, FaultPerformanceTest::looprootPid, msg);
    GTEST_LOG_(INFO) << "DumpCatch API Performance time(PID(root), TID(root)): " << GetStopTime(befor) << "s";
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
    DfxDumpCatcher dumplog;
    std::string msg;
    clock_t befor = GetStartTime();
    dumplog.DumpCatch(FaultPerformanceTest::looprootPid, 0, msg);
    GTEST_LOG_(INFO) << "DumpCatch API Performance time(PID(root), TID(0)): " << GetStopTime(befor) << "s";
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
    std::string procCMD = "processdump -p " + std::to_string(FaultPerformanceTest::looprootPid) + " -t "+
        std::to_string(FaultPerformanceTest::looprootPid);
    clock_t befor = GetStartTime();
    FaultPerformanceTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "Processdump Command Performance time(PID(root), TID(root)): " << GetStopTime(befor) << "s";
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
    std::string procCMD = "processdump -p " + std::to_string(FaultPerformanceTest::looprootPid);
    clock_t befor = GetStartTime();
    FaultPerformanceTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "Processdump Command Performance time(PID(root)): " << GetStopTime(befor) << "s";
    GTEST_LOG_(INFO) << "FaultPerformanceTest004: end.";
}

/**
 * @tc.name: FaultPerformanceTest005_6
 * @tc.desc: test Crash : SIGFPE
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest005_6, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest005_6: start.";
    GetTimeLog("SIGFPE");
    GTEST_LOG_(INFO) << "FaultPerformanceTest005_6: end.";
}

/**
 * @tc.name: FaultPerformanceTest007_8
 * @tc.desc: test Crash : SIGILL
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest007_8, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest007_8: start.";
    GetTimeLog("SIGILL");
    GTEST_LOG_(INFO) << "FaultPerformanceTest007_8: end.";
}

/**
 * @tc.name: FaultPerformanceTest009_10
 * @tc.desc: test Crash : SIGSEGV
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest009_10, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest009_10: start.";
    GetTimeLog("SIGSEGV");
    GTEST_LOG_(INFO) << "FaultPerformanceTest009_10: end.";
}

/**
 * @tc.name: FaultPerformanceTest011_12
 * @tc.desc: test Crash : SIGTRAP
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest011_12, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest011_12: start.";
    GetTimeLog("SIGTRAP");
    GTEST_LOG_(INFO) << "FaultPerformanceTest011_12: end.";
}

/**
 * @tc.name: FaultPerformanceTest013_14
 * @tc.desc: test Crash : SIGABRT
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest013_14, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest013_14: start.";
    GetTimeLog("SIGABRT");
    GTEST_LOG_(INFO) << "FaultPerformanceTest013_14: end.";
}

/**
 * @tc.name: FaultPerformanceTest015_16
 * @tc.desc: test Crash : SIGBUS
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest015_16, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest015_16: start.";
    GetTimeLog("SIGBUS");
    GTEST_LOG_(INFO) << "FaultPerformanceTest015_16: end.";
}

/**
 * @tc.name: FaultPerformanceTest017_18
 * @tc.desc: test Crash : STACKTRACE
 * @tc.type: FUNC
 */
HWTEST_F (FaultPerformanceTest, FaultPerformanceTest017_18, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultPerformanceTest017_18: start.";
    GetTimeLog("STACKTRACE");
    GTEST_LOG_(INFO) << "FaultPerformanceTest017_18: end.";
}
