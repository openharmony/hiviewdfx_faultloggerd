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

#include <gtest/gtest.h>
#include <memory>

#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ProcessDfxTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
    static pid_t GetAccountmgrPid();
};
} // namespace HiviewDFX
} // namespace OHOS

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

namespace {
/**
 * @tc.name: DfxSignalTest001
 * @tc.desc: test DfxSignal functions
 * @tc.type: FUNC
 */
HWTEST_F(ProcessDfxTest, DfxSignalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest001: start.";
    siginfo_t si = {
        .si_signo = SIGSEGV,
        .si_errno = 0,
        .si_code = -1,
        .si_value.sival_int = gettid()
    };
    map<int, string> sigKeys = {
        { SIGILL, string("SIGILL") },
        { SIGTRAP, string("SIGTRAP") },
        { SIGABRT, string("SIGABRT") },
        { SIGBUS, string("SIGBUS") },
        { SIGFPE, string("SIGFPE") },
        { SIGSEGV, string("SIGSEGV") },
        { SIGSTKFLT, string("SIGSTKFLT") },
        { SIGSYS, string("SIGSYS") },
    };
    for (auto sigKey : sigKeys) {
        std::string sigKeyword = "Signal:";
        si.si_signo = sigKey.first;
        sigKeyword += sigKey.second;
        std::string sigStr = PrintSignal(si);
        GTEST_LOG_(INFO) << sigStr;
        ASSERT_TRUE(sigStr.find(sigKeyword) != std::string::npos);
    }
    GTEST_LOG_(INFO) << "DfxSignalTest001: end.";
}

/**
 * @tc.name: ProcessDfxRequestTest001
 * @tc.desc: test cinit process maps node
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDfxTest, ProcessDfxRequestTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest001: start.";
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    auto dfx = processDfx->InitProcessMaps();
    EXPECT_EQ(false, dfx) << "ProcessDfxRequestTest001 Failed";
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
    pid_t accountmgrPid = GetProcessPid(ACCOUNTMGR_NAME);
    if (accountmgrPid == 0) {
        GTEST_LOG_(INFO) << "ProcessDfxRequestTest002: get pid failed.";
        return;
    }
    std::shared_ptr<DfxProcess> processDfx = std::make_shared<DfxProcess>();
    pid_t pid = accountmgrPid;
    pid_t tid = accountmgrPid;
    std::shared_ptr<DfxThread> keyThread = std::make_shared<DfxThread>(pid, tid, tid);
    auto dfx = processDfx->InitProcessThreads(keyThread);
    EXPECT_EQ(true, dfx) << "ProcessDfxRequestTest002 Failed";
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
    auto dfx = processDfx->InitOtherThreads();
    EXPECT_EQ(false, dfx) << "ProcessDfxRequestTest003 Failed";
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
    processDfx->SetPid(pid);
    sleep(1);
    auto getpid = processDfx->GetPid();
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
    processDfx->SetPid(pid);
    sleep(1);
    auto getpid = processDfx->GetPid();
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
    processDfx->SetPid(pid);
    auto getpid = processDfx->GetPid();
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
    uid_t uid = 1;
    processDfx->SetUid(uid);
    auto getuid = processDfx->GetUid();
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
    uid_t uid = 164;
    processDfx->SetUid(uid);
    auto getuid = processDfx->GetUid();
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
    uid_t uid = -1;
    processDfx->SetUid(uid);
    auto getuid = processDfx->GetUid();
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
    processDfx->SetProcessName(input);
    auto output = processDfx->GetProcessName();
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
    processDfx->SetMaps(maps);
    auto output = processDfx->GetMaps();
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
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    std::vector<std::shared_ptr<DfxThread>> input;
    processDfx->SetThreads(input);
    input.push_back(thread);
    auto output = processDfx->GetThreads();
    EXPECT_EQ(false, input.size() == output.size()) << "ProcessDfxRequestTest012 Failed";
    GTEST_LOG_(INFO) << "ProcessDfxRequestTest012: end.";
}
}