/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <ctime>
#include <securec.h>
#include <string>
#include <vector>
#include "dfx_define.h"
#include "dfx_util.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "string_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class CommonTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxCommonTest"
#endif

/**
 * @tc.name: DfxUtilTest001
 * @tc.desc: test DfxUtil GetCurrentTimeStr
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest001: start.";
    time_t now = time(nullptr);
    std::string timeStr = GetCurrentTimeStr(static_cast<uint64_t>(now));
    GTEST_LOG_(INFO) << timeStr;
    ASSERT_NE(timeStr, "invalid timestamp\n");
    now += 100000; // 100000 : test time offset
    timeStr = GetCurrentTimeStr(static_cast<uint64_t>(now));
    GTEST_LOG_(INFO) << timeStr;
    ASSERT_NE(timeStr, "invalid timestamp\n");
    GTEST_LOG_(INFO) << "DfxUtilTest001: end.";
}

/**
 * @tc.name: DfxUtilTest002
 * @tc.desc: test DfxUtil TrimAndDupStr
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest002: start.";
    std::string testStr = " abcd  ";
    std::string resStr;
    bool ret = TrimAndDupStr(testStr, resStr);
    ASSERT_EQ(ret, true);
    ASSERT_EQ(resStr, "abcd");
    GTEST_LOG_(INFO) << "DfxUtilTest002: end.";
}

/**
 * @tc.name: DfxDumpResTest001
 * @tc.desc: test DfxDumpRes functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxDumpResTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpResTest001: start.";
    int32_t res = DUMP_ESUCCESS;
    GTEST_LOG_(INFO) << DfxDumpRes::ToString(res);
    GTEST_LOG_(INFO) << "DfxDumpResTest001: end.";
}

/**
 * @tc.name: DfxLogTest001
 * @tc.desc: test DfxLog functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxLogTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxLogTest001: start.";
    InitDebugFd(STDERR_FILENO);
    EXPECT_FALSE(CheckDebugLevel());
    GTEST_LOG_(INFO) << "DfxLogTest001: end.";
}

/**
 * @tc.name: StringUtilTest001
 * @tc.desc: test StartsWith functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, StringUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StringUtilTest001: start.";
    std::string start = "[anon:ArkTS Code2024]";
    bool ret = StartsWith(start, "[anon:ArkTS Code");
    EXPECT_TRUE(ret);
    std::string end = "test.hap";
    ret = EndsWith(end, ".hap");
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << "StringUtilTest001: end.";
}

/**
 * @tc.name: TestCalDumpTimeDiff001
 * @tc.desc: test CalDumpTimeDiff
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, TestCalDumpTimeDiff001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "TestCalDumpTimeDiff001: start.";

    // curTime endTime result
    std::vector<vector<int>> testData = {
        {10, 10, 0},
        {100, 5, -1},
        {NUMBER_ONE_MILLION - 1, 100, 101},
        {5, 100, 95},
        {100, NUMBER_ONE_MILLION - 1, -1}
    };

    for (const auto& data : testData) {
        EXPECT_EQ(CalDumpTimeDiff(data[0], data[1]), data[2]) << "cal diff error start " << data[0] <<
            ",end " << data[1] << ",expectedDiff " << data[2];
    }

    GTEST_LOG_(INFO) << "TestCalDumpTimeDiff001: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS