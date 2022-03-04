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

/* This files contains faultlog system test header modules. */

#ifndef FAULTLOGGERD_SYSTEM_TEST_H
#define FAULTLOGGERD_SYSTEM_TEST_H

#if defined(__arm__)
    #define REGISTERS           "r0:","r1:","r2:","r3:","r4:","r5:","r6:",\
                                "r7:","r8:","r9:","r10:","fp:","ip:","sp:","lr:","pc:"
    #define REGISTERS_NUM       16
    #define REGISTERS_LENGTH    10
#elif defined(__aarch64__)
    #define REGISTERS           "x0:","x1:","x2:","x3:","x4:","x5:","x6:","x7:","x8:",\
                                "x9:","x10:","x11:","x12:","x13:","x14:","x15:","x16:",\
                                "x17:","x18:","x19:","x20:","x21:","x22:","x23:","x24:",\
                                "x25:","x26:","x27:","x28:","x29:","lr:","sp:","pc:"
    #define REGISTERS_NUM       33
    #define REGISTERS_LENGTH    18
#endif

#include <gtest/gtest.h>
#include <map>

namespace OHOS {
namespace HiviewDFX {

static const int ARRAY_SIZE_HUNDRED = 100;

class FaultLoggerdSystemTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    static std::string GetPidMax();
    static std::string GetTidMax();

    // 合并
    static std::string ForkAndRunCommands(const std::vector<std::string>& cmds, int commandStatus);

    // jason update
    static std::string ForkAndCommands(const std::vector<std::string>& cmds, int cresherType, int udid);

    static std::string ProcessDumpCommands(const std::string cmds);
    // 合并
    static std::string GetfileNamePrefix(const std::string errorCMD, int commandStatus);
    static std::string GetstackfileNamePrefix(const std::string errorCMD, int commandStatus);

    static int CountLines(std::string filename);
    static bool IsDigit(std::string pid);

    static int CheckCountNum(std::string filePath, std::string pid, std::string errorCMD);
    static int CheckCountNumPCZero(std::string filePath, std::string pid, std::string errorCMD);
    static int CheckStacktraceCountNum(std::string filePath, std::string pid, std::string errorCMD);
    static int CheckCountNumMultiThread(std::string filePath, std::string pid, std::string errorCMD);
    static int CheckCountNumOverStack(std::string filePath, std::string pid, std::string ErrorCMD);
    static int CheckCountNumStackTop(std::string filePath, std::string pid, std::string ErrorCMD);
    static std::string GetStackTop();
    static std::string GetfileNameForFounation(std::string pidfound);
    static int CheckCountNumKill11(std::string filePath, std::string pid);
    static void Trim(std::string & str);
    static std::string GetFounationPid();

    // 合并
    static void StartCrasherLoop(int type);     // 1. system; 2. root; 3.app; 4. root+cpp
    static void KillCrasherLoopForSomeCase(int type);
    static void StartCrasherLoopForUnsingPidAndTid(int crasherType);    // 1.c 2.c++
    static int crashThread(int threadID);
    static int getApplyPid(std::string applyName);
    static void dumpCatchThread(int threadID);
    static void GetTestFaultLoggerdTid(int testPid);

    static std::string rootTid[ARRAY_SIZE_HUNDRED];
    static std::string appTid[ARRAY_SIZE_HUNDRED];
    static std::string sysTid[ARRAY_SIZE_HUNDRED];
    static std::string testTid[ARRAY_SIZE_HUNDRED];

    // 更新为数组
    static int loopSysPid;
    static int loopRootPid;
    static int loopCppPid;
    static int loopAppPid;

    static char resultBufShell[ARRAY_SIZE_HUNDRED];
    static int appTidCount;
    static int rootTidCount;
    static int sysTidCount;
    static unsigned int unsigLoopSysPid;
    static int count;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULTLOGGERD_SYSTEM_TEST_H
