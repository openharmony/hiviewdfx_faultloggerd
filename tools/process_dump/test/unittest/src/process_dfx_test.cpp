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

/* This files contains unit test of process module. */

#include "process_dfx_test.h"

#include <memory>

#include "dfx_process.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

void ProcessDfxTest::SetUpTestCase(void)
{
}


void ProcessDfxTest::TearDownTestCase(void)
{
}

void ProcessDfxTest::SetUp(void)
{
}

void ProcessDfxTest::TearDown(void)
{
}

int ProcessDfxTest::loopRootPid_ = -1;

std::string ProcessDfxTest::GetTelephonyPid()
{
    std::string procCMD = "pgrep 'telephony'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMD;
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        return std::to_string(loopRootPid_);
    }
    std::string pidLog;
    char result_buf_shell[PERFORMANCE_TEST_NUMBER_ONE_HUNDRED] = { 0, };
    if (fgets(result_buf_shell, sizeof(result_buf_shell), procFileInfo) != nullptr) {
        pidLog = result_buf_shell;
        loopRootPid_ = atoi(pidLog.c_str());
    }
    pclose(procFileInfo);
    return std::to_string(loopRootPid_);
}

namespace {
/**
 * @tc.name: ProcessDfxRequestTest001
 * @tc.desc: test cinit process maps node
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest001: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    auto dfx = false;
    if (processDfx != nullptr) {
        dfx = processDfx->InitProcessMaps();
    }
    EXPECT_EQ(true, dfx != true) << "ProcessDfxRequestTest001 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest001: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest002
 * @tc.desc: test init process threads
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest002: start.";
    ProcessDfxTest::GetTelephonyPid();
    if (ProcessDfxTest::loopRootPid_ ==  -1) {
        GTEST_LOG_(INFO) << "ProcessDfxRequestTest002: get pid failed.";
        return;
    }
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t pid = ProcessDfxTest::loopRootPid_;
    pid_t tid = ProcessDfxTest::loopRootPid_;
    std::shared_ptr<DfxThread> keyThread = std::make_shared<DfxThread>(pid, tid);
    auto dfx = false;
    if (processDfx != nullptr && keyThread != nullptr) {
        dfx = processDfx->InitProcessThreads(keyThread);
    }
    ProcessDfxTest::loopRootPid_ = -1;
    EXPECT_EQ(true, dfx == true) << "ProcessDfxRequestTest002 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest002: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest003
 * @tc.desc: test init other threads
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest003: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    auto dfx = false;
    if (processDfx != nullptr) {
        dfx = processDfx->InitOtherThreads();
    }
    EXPECT_EQ(true, dfx != true) << "ProcessDfxRequestTest003 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest003: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest004
 * @tc.desc: test get pid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest004: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t pid = 1;
    auto getpid = 0;
    if (processDfx != nullptr) {
        processDfx->SetPid(pid);
        sleep(1);
        getpid = processDfx->GetPid();
    }
    EXPECT_EQ(true, getpid == pid) << "ProcessDfxRequestTest004 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest004: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest005
 * @tc.desc: test get pid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest005: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t pid = 164;
    auto getpid = 0;
    if (processDfx != nullptr) {
        processDfx->SetPid(pid);
        sleep(1);
        getpid = processDfx->GetPid();
    }
    EXPECT_EQ(true, getpid == pid) << "ProcessDfxRequestTest005 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest005: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest006
 * @tc.desc: test get pid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest006: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t pid = 10000;
    auto getpid = 0;
    if (processDfx != nullptr) {
        processDfx->SetPid(pid);
        getpid = processDfx->GetPid();
    }
    EXPECT_EQ(true, getpid == pid) << "ProcessDfxRequestTest006 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest006: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest007
 * @tc.desc: test get uid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest007: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t uid = 1;
    pid_t getuid = 0;
    if (processDfx != nullptr) {
        processDfx->SetUid(uid);
        getuid = processDfx->GetUid();
    }
    EXPECT_EQ(true, getuid == uid) << "ProcessDfxRequestTest007 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest007: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest008
 * @tc.desc: test get uid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest008: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t uid = 164;
    pid_t getuid = 0;
    if (processDfx != nullptr) {
        processDfx->SetUid(uid);
        getuid = processDfx->GetUid();
    }
    EXPECT_EQ(true, getuid == uid) << "ProcessDfxRequestTest008 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest008: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest009
 * @tc.desc: test get uid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest009: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t uid = -1;
    pid_t getuid = 0;
    if (processDfx != nullptr) {
        processDfx->SetUid(uid);
        getuid = processDfx->GetUid();
    }
    EXPECT_EQ(true, getuid == uid) << "ProcessDfxRequestTest009 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest009: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest010
 * @tc.desc: test get process name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest010: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    std::string input = "processName";
    std::string output = "";
    if (processDfx != nullptr) {
        processDfx->SetProcessName(input);
        output =  processDfx->GetProcessName();
    }
    EXPECT_EQ(true, input == output) << "ProcessDfxRequestTest010 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest010: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest011
 * @tc.desc: test get map
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest011: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    std::shared_ptr<DfxElfMaps> maps = std::make_shared<DfxElfMaps>();
    std::shared_ptr<DfxElfMaps> output = std::make_shared<DfxElfMaps>();
    if (processDfx != nullptr && maps != nullptr) {
        processDfx->SetMaps(maps);
        output = processDfx->GetMaps();
    }
    EXPECT_EQ(true, output == maps) << "ProcessDfxRequestTest011 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest011: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest012
 * @tc.desc: test get threads
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest012: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t pid = 100;
    pid_t tid = 100;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid);
    std::vector<std::shared_ptr<DfxThread>> input;
    std::vector<std::shared_ptr<DfxThread>> output;
    if (processDfx != nullptr && thread != nullptr) {
        processDfx->SetThreads(input);
        input.push_back(thread);
        output = processDfx->GetThreads();
    }
    EXPECT_EQ(false, input.size() == output.size()) << "ProcessDfxRequestTest012 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest012: end.";
}
}
