/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "dfx_define.h"
#include "catchframe_local.h"
#include "dfx_test_util.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class CatchFrameLocalTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

static const int THREAD_ALIVE_TIME = 2;

static const int CREATE_THREAD_TIMEOUT = 300000;

static pid_t g_threadId = 0;

static pid_t g_processId = 0;

int g_testPid = 0;

void CatchFrameLocalTest::SetUpTestCase()
{
    InstallTestHap("/data/FaultloggerdJsTest.hap");
    std::string testBundleName = TEST_BUNDLE_NAME;
    std::string testAbiltyName = testBundleName + ".MainAbility";
    g_testPid = LaunchTestHap(testAbiltyName, testBundleName);
}

void CatchFrameLocalTest::TearDownTestCase()
{
    StopTestHap(TEST_BUNDLE_NAME);
    UninstallTestHap(TEST_BUNDLE_NAME);
}

void CatchFrameLocalTest::SetUp()
{}

void CatchFrameLocalTest::TearDown()
{}

/**
 * @tc.name: DumpCatcherInterfacesTest001
 * @tc.desc: test CatchFrame API: PID(getpid()), TID(gettid())
 * @tc.type: FUNC
 */
HWTEST_F(CatchFrameLocalTest, CatchFrameLocalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CatchFrameLocalTest001: start.";
    DfxCatchFrameLocal dumplog(getpid());
    bool ret = dumplog.InitFrameCatcher();
    EXPECT_EQ(ret, true);
    std::vector<DfxFrame> frameV;
    ret = dumplog.CatchFrame(gettid(), frameV);
    EXPECT_EQ(ret, true);
    dumplog.DestroyFrameCatcher();
    EXPECT_GT(frameV.size(), 0) << "CatchFrameLocalTest001 Failed";
    GTEST_LOG_(INFO) << "CatchFrameLocalTest001: end.";
}

/**
 * @tc.name: CatchFrameLocalTest002
 * @tc.desc: test CatchFrame API: PID(test hap), TID(test hap main thread)
 * @tc.type: FUNC
 */
HWTEST_F(CatchFrameLocalTest, CatchFrameLocalTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CatchFrameLocalTest002: start.";
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    DfxCatchFrameLocal dumplog(g_testPid);
    std::vector<DfxFrame> frameV;
    bool ret = dumplog.InitFrameCatcher();
    EXPECT_EQ(ret, true);
    ret = dumplog.CatchFrame(g_testPid, frameV);
    EXPECT_EQ(ret, false);
    dumplog.DestroyFrameCatcher();
    EXPECT_EQ(frameV.size(), 0) << "CatchFrameLocalTest002 Failed";
    GTEST_LOG_(INFO) << "CatchFrameLocalTest002: end.";
}

/**
 * @tc.name: CatchFrameLocalTest003
 * @tc.desc: test CatchFrame API: TID(gettid())
 * @tc.type: FUNC
 */
HWTEST_F(CatchFrameLocalTest, CatchFrameLocalTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CatchFrameLocalTest003: start.";
    DfxCatchFrameLocal dumplog(getpid());
    bool result = dumplog.InitFrameCatcher();
    EXPECT_EQ(result, true);
    std::vector<DfxFrame> frameV;
    bool ret = dumplog.CatchFrame(gettid(), frameV, true);
    dumplog.DestroyFrameCatcher();
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "CatchFrameLocalTest003 Failed";
    GTEST_LOG_(INFO) << "CatchFrameLocalTest003: end.";
}

/**
 * @tc.name: CatchFrameLocalTest004
 * @tc.desc: test CatchFrame API: app TID(-11)
 * @tc.type: FUNC
 */
HWTEST_F(CatchFrameLocalTest, CatchFrameLocalTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CatchFrameLocalTest004: start.";
    DfxCatchFrameLocal dumplog(getpid());
    bool result = dumplog.InitFrameCatcher();
    EXPECT_EQ(result, true);
    std::vector<DfxFrame> frameV;
    bool ret = dumplog.CatchFrame(-11, frameV, true);
    dumplog.DestroyFrameCatcher();
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "CatchFrameLocalTest004 Failed";
    GTEST_LOG_(INFO) << "CatchFrameLocalTest004: end.";
}

/**
 * @tc.name: CatchFrameLocalTest005
 * @tc.desc: test CatchFrame API: TID(-1)
 * @tc.type: FUNC
 */
HWTEST_F(CatchFrameLocalTest, CatchFrameLocalTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CatchFrameLocalTest005: start.";
    DfxCatchFrameLocal dumplog(getpid());
    bool result = dumplog.InitFrameCatcher();
    EXPECT_EQ(result, true);
    std::vector<DfxFrame> frameV;
    bool ret = dumplog.CatchFrame(-1, frameV);
    dumplog.DestroyFrameCatcher();
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "CatchFrameLocalTest005 Failed";
    GTEST_LOG_(INFO) << "CatchFrameLocalTest005: end.";
}

int32_t g_tid = 0;
std::mutex g_mutex;
__attribute__((noinline)) void Test002()
{
    printf("Test002\n");
    g_mutex.lock();
    g_mutex.unlock();
}

__attribute__((noinline)) void Test001()
{
    g_tid = gettid();
    printf("Test001:%d\n", g_tid);
    Test002();
}

/**
 * @tc.name: CatchFrameLocalTest006
 * @tc.desc: test unwind in newly create threads
 * @tc.type: FUNC
 */
HWTEST_F(CatchFrameLocalTest, CatchFrameLocalTest006, TestSize.Level2)
{
    DfxCatchFrameLocal dumplog(getpid());
    std::vector<DfxFrame> frames;
    bool result = dumplog.InitFrameCatcher();
    EXPECT_EQ(result, true);

    g_mutex.lock();
    std::thread worker(Test001);
    sleep(1);
    if (g_tid <= 0) {
        result = false;
    }

    bool hasEmptyBinaryName = false;
    if (result) {
        result = dumplog.CatchFrame(g_tid, frames);
        for (auto& frame : frames) {
            printf("name:%s\n", frame.mapName.c_str());
            if (frame.mapName.empty()) {
                hasEmptyBinaryName = true;
            }
        }
    }

    g_mutex.unlock();
    g_tid = 0;
    if (worker.joinable()) {
        worker.join();
    }

    dumplog.DestroyFrameCatcher();
    if (hasEmptyBinaryName) {
        FAIL() << "BinaryName should not be empty.\n";
    }
    EXPECT_EQ(result, true);
    EXPECT_GT(frames.size(), 0) << "CatchFrameLocalTest006 Failed";
}

/**
 * @tc.name: DumpCatcherInterfacesTest007
 * @tc.desc: test CatchFrame API: PID(getpid()), mapFrames
 * @tc.type: FUNC
 */
HWTEST_F(CatchFrameLocalTest, CatchFrameLocalTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CatchFrameLocalTest001: start.";
    DfxCatchFrameLocal dumplog(getpid());
    bool ret = dumplog.InitFrameCatcher();
    EXPECT_EQ(ret, true);
    std::map<int, std::vector<DfxFrame>> mapFrames;
    ret = dumplog.CatchFrame(mapFrames);
    EXPECT_EQ(ret, true);
    dumplog.DestroyFrameCatcher();
    EXPECT_GT(mapFrames.size(), 0) << "CatchFrameLocalTest007 Failed";
    GTEST_LOG_(INFO) << "CatchFrameLocalTest007: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
