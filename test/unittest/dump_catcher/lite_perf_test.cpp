/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <cstring>
#include <gtest/gtest.h>
#include <sys/utsname.h>
#include <thread>
#include <unistd.h>
#include "dfx_test_util.h"
#include "dfx_dump_catcher.h"
#include "file_ex.h"
#include "lite_perf.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {

class LitePerfTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void LitePerfTest::SetUpTestCase()
{}

void LitePerfTest::TearDownTestCase()
{}

void LitePerfTest::SetUp()
{}

void LitePerfTest::TearDown()
{}

/**
 * @tc.name: LitePerfTest001
 * @tc.desc: test LitePerf normal
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest001: start.";
    static bool threadExit = false;
    const int testTimes = 5000;
    auto testThread = [&testTimes] {
        std::vector<int> tids;
        int tid = getpid();
        tids.emplace_back(tid);
        tids.emplace_back(gettid());
        int freq = 100;
        int durationMs = testTimes;
        bool parseMiniDebugInfo = false;
        LitePerf litePerf;
        if (!IsLinuxKernel()) {
            int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
            EXPECT_EQ(ret, 0);
            std::string sampleStack;
            ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
            EXPECT_EQ(ret, 0);
            ASSERT_TRUE(sampleStack.size() != 0);
        }
        EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
        threadExit = true;
    };
    std::thread th(testThread);

    int times = 0;
    int sleepTime = 100;
    while (!threadExit) {
        if (times > testTimes || sleepTime <= 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        times += sleepTime;
        sleepTime--;
    }
    th.join();
    GTEST_LOG_(INFO) << "LitePerfTest001: end.";
}

/**
 * @tc.name: LitePerfTest002
 * @tc.desc: test LitePerf tids empty
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest002: start.";
    std::vector<int> tids;
    int tid = getpid();
    int freq = 100;
    int durationMs = 5000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    if (!IsLinuxKernel()) {
        int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
        EXPECT_EQ(ret, -1);
        std::string sampleStack;
        ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
        EXPECT_EQ(ret, -1);
        ASSERT_TRUE(sampleStack.size() == 0);
    }
    EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    GTEST_LOG_(INFO) << "LitePerfTest002: end.";
}

/**
 * @tc.name: LitePerfTest003
 * @tc.desc: test LitePerf invalid freq
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest003: start.";
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = 2000;
    int durationMs = 5000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    if (!IsLinuxKernel()) {
        int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
        EXPECT_EQ(ret, -1);
        std::string sampleStack;
        ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
        EXPECT_EQ(ret, -1);
        ASSERT_TRUE(sampleStack.size() == 0);
    }
    EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    GTEST_LOG_(INFO) << "LitePerfTest003: end.";
}

/**
 * @tc.name: LitePerfTest004
 * @tc.desc: test LitePerf invalid freq -1
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest004: start.";
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = -1;
    int durationMs = 5000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    if (!IsLinuxKernel()) {
        int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
        EXPECT_EQ(ret, -1);
        std::string sampleStack;
        ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
        EXPECT_EQ(ret, -1);
        ASSERT_TRUE(sampleStack.size() == 0);
    }
    EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    GTEST_LOG_(INFO) << "LitePerfTest004: end.";
}

/**
 * @tc.name: LitePerfTest005
 * @tc.desc: test LitePerf invalid time
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest005: start.";
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = 100;
    int durationMs = 20000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    if (!IsLinuxKernel()) {
        int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
        EXPECT_EQ(ret, -1);
        std::string sampleStack;
        ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
        EXPECT_EQ(ret, -1);
        ASSERT_TRUE(sampleStack.size() == 0);
    }
    EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    GTEST_LOG_(INFO) << "LitePerfTest005: end.";
}

/**
 * @tc.name: LitePerfTest006
 * @tc.desc: test LitePerf invalid time -1
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest006: start.";
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = 100;
    int durationMs = -1;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    if (!IsLinuxKernel()) {
        int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
        EXPECT_EQ(ret, -1);
        std::string sampleStack;
        ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
        EXPECT_EQ(ret, -1);
        ASSERT_TRUE(sampleStack.size() == 0);
    }
    EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    GTEST_LOG_(INFO) << "LitePerfTest006: end.";
}

/**
 * @tc.name: LitePerfTest007
 * @tc.desc: test LitePerf invalid tids
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest007: start.";
    std::vector<int> tids {1, 2, 3, 4, 5};
    int freq = 100;
    int durationMs = 5000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    if (!IsLinuxKernel()) {
        int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
        EXPECT_EQ(ret, -1);
        for (auto tid : tids) {
            std::string sampleStack;
            sampleStack.clear();
            ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
            EXPECT_EQ(ret, -1);
        }
    }
    EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    GTEST_LOG_(INFO) << "LitePerfTest007: end.";
}

/**
 * @tc.name: LitePerfTest008
 * @tc.desc: test LitePerf and DumpCatcher as the same time
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest008: start.";
    int fd[2];
    EXPECT_TRUE(CreatePipeFd(fd));

    pid_t pid = 0;
    pid = fork();
    if (pid < 0) {
        GTEST_LOG_(INFO) << "LitePerfTest008: Failed to vfork.";
    } else if (pid == 0) {
        NotifyProcStart(fd);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        pid_t parentPid = getppid();
        GTEST_LOG_(INFO) << "LitePerfTest008: parentPid: " << parentPid;
        DfxDumpCatcher dumplog;
        string msg = "";
        bool ret = dumplog.DumpCatch(parentPid, 0, msg);
        EXPECT_TRUE(ret) << "LitePerfTest008: DumpCatch0 msg Failed.";
    } else {
        WaitProcStart(fd);
        std::vector<int> tids;
        int tid = getpid();
        GTEST_LOG_(INFO) << "pid: " << tid;
        tids.emplace_back(tid);
        int freq = 100;
        int durationMs = 5000;
        bool parseMiniDebugInfo = false;
        LitePerf litePerf;
        if (!IsLinuxKernel()) {
            int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
            EXPECT_EQ(ret, 0) << "LitePerfTest008: StartProcessStackSampling Failed.";
            std::string sampleStack;
            litePerf.CollectSampleStackByTid(tid, sampleStack);
        }
        EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    }
    GTEST_LOG_(INFO) << "LitePerfTest008: end.";
}

/**
 * @tc.name: LitePerfTest009
 * @tc.desc: test LitePerf CollectProcessStackSampling
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTest009: start.";
    pid_t tid = gettid();

    LitePerfConfig config{};
    config.tids.emplace_back(tid);
    config.durationMs = 1500;
    LitePerf litePerf;
    std::string perfContent;
    if (!IsLinuxKernel()) {
        std::thread([&]{
            litePerf.CollectProcessStackSampling(config, true, perfContent);
        }).detach();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ASSERT_EQ(litePerf.CollectProcessStackSampling(config, true, perfContent), PerfErrorCode::PERF_SAMPING);
        LitePerf litePerf2;
        ASSERT_EQ(litePerf2.CollectProcessStackSampling(config, true, perfContent), PerfErrorCode::PERF_SAMPING);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    EXPECT_EQ(litePerf.FinishProcessStackSampling(), 0);
    GTEST_LOG_(INFO) << "LitePerfTest009: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
