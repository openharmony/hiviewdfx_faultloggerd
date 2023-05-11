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

/* This files contains faultlog system test header modules. */

#ifndef FAULTLOGGERD_SYSTEM_TEST_H
#define FAULTLOGGERD_SYSTEM_TEST_H

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

    static std::string ForkAndRunCommands(const std::vector<std::string>& cmds, int commandStatus);
    static std::string ForkAndCommands(int crasherType, int udid);

    static std::string GetfileNamePrefix(const std::string errorCMD, int commandStatus);
    static std::string GetstackfileNamePrefix(const std::string errorCMD, int commandStatus);

    static int CheckCountNum(std::string& filePath, std::string& pid, std::string& errorCMD);
    static int CheckCountNumAbort(std::string& filePath, std::string& pid);
    static int CheckCountNumPCZero(std::string& filePath, std::string& pid);
    static int CheckCountNumMultiThread(std::string& filePath, std::string& pid);
    static int CheckCountNumOverStack(std::string& filePath, std::string& pid);
    static int CheckCountNumStackTop(std::string& filePath, std::string& pid, std::string& ErrorCMD);
    static std::string GetStackTop();

    static void StartCrasherLoop(int type);     // 1. system; 2. root; 3.app; 4. root+cpp
    static void KillCrasherLoopForSomeCase(int type);
    static void StartCrasherLoopForUnsingPidAndTid(int crasherType);    // 1.c 2.c++
    static void TestDumpCatch(const int targetPid, const std::string processName, const int threadIdx);

    static std::string rootTid[ARRAY_SIZE_HUNDRED];
    static std::string appTid[ARRAY_SIZE_HUNDRED];
    static std::string sysTid[ARRAY_SIZE_HUNDRED];

    static int loopSysPid;
    static int loopRootPid;
    static int loopCppPid;
    static int loopAppPid;

    static char resultBufShell[ARRAY_SIZE_HUNDRED];
    static int appTidCount;
    static int rootTidCount;
    static int sysTidCount;
    static unsigned int unsigLoopSysPid;
    static int checkCnt;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULTLOGGERD_SYSTEM_TEST_H
