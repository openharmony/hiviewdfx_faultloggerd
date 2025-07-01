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
 * @tc.name: LitePerfTestTest002
 * @tc.desc: test LitePerf invalid tids
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTestTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTestTest002: start.";
    if (IsLinuxKernel()) {
        return;
    }
    std::vector<int> tids;
    int tid = getpid();
    int freq = 100;
    int durationMs = 5000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
    EXPECT_EQ(ret, -1);
    std::string sampleStack;
    ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
    EXPECT_EQ(ret, -1);
    ASSERT_TRUE(sampleStack.size() == 0);
    ret = litePerf.FinishProcessStackSampling();
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "LitePerfTest002: end.";
}

/**
 * @tc.name: LitePerfTestTest003
 * @tc.desc: test LitePerf invalid freq
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTestTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTestTest003: start.";
    if (IsLinuxKernel()) {
        return;
    }
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = 2000;
    int durationMs = 5000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
    EXPECT_EQ(ret, -1);
    std::string sampleStack;
    ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
    EXPECT_EQ(ret, -1);
    ASSERT_TRUE(sampleStack.size() == 0);
    ret = litePerf.FinishProcessStackSampling();
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "LitePerfTest003: end.";
}

/**
 * @tc.name: LitePerfTestTest004
 * @tc.desc: test LitePerf invalid freq -1
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTestTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTestTest004: start.";
    if (IsLinuxKernel()) {
        return;
    }
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = -1;
    int durationMs = 5000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
    EXPECT_EQ(ret, -1);
    std::string sampleStack;
    ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
    EXPECT_EQ(ret, -1);
    ASSERT_TRUE(sampleStack.size() == 0);
    ret = litePerf.FinishProcessStackSampling();
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "LitePerfTest004: end.";
}

/**
 * @tc.name: LitePerfTestTest005
 * @tc.desc: test LitePerf invalid time
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTestTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTestTest005: start.";
    if (IsLinuxKernel()) {
        return;
    }
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = 100;
    int durationMs = 20000;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
    EXPECT_EQ(ret, -1);
    std::string sampleStack;
    ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
    EXPECT_EQ(ret, -1);
    ASSERT_TRUE(sampleStack.size() == 0);
    ret = litePerf.FinishProcessStackSampling();
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "LitePerfTest005: end.";
}

/**
 * @tc.name: LitePerfTestTest006
 * @tc.desc: test LitePerf invalid time -1
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfTest, LitePerfTestTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfTestTest006: start.";
    if (IsLinuxKernel()) {
        return;
    }
    std::vector<int> tids;
    int tid = getpid();
    tids.emplace_back(tid);
    int freq = 100;
    int durationMs = -1;
    bool parseMiniDebugInfo = false;
    LitePerf litePerf;
    int ret = litePerf.StartProcessStackSampling(tids, freq, durationMs, parseMiniDebugInfo);
    EXPECT_EQ(ret, -1);
    std::string sampleStack;
    ret = litePerf.CollectSampleStackByTid(tid, sampleStack);
    EXPECT_EQ(ret, -1);
    ASSERT_TRUE(sampleStack.size() == 0);
    ret = litePerf.FinishProcessStackSampling();
    EXPECT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "LitePerfTestTest006: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
