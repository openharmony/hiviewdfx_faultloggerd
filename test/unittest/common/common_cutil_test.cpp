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
#include <securec.h>
#include <string>
#include <vector>
#include "dfx_cutil.h"
#include "dfx_define.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class CommonCutilTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
/**
 * @tc.name: DfxCutilTest001
 * @tc.desc: test cutil functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonCutilTest, DfxCutilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCutilTest001: start.";
    char threadName[NAME_LEN];
    char processName[NAME_LEN];
    ASSERT_TRUE(GetThreadName(threadName, sizeof(threadName)));
    ASSERT_TRUE(GetProcessName(processName, sizeof(processName)));
    ASSERT_GT(GetRealPid(), 0);
    GTEST_LOG_(INFO) << "DfxCutilTest001: end.";
}

/**
 * @tc.name: DfxCutilTest002
 * @tc.desc: test cutil functions GetThreadName null
 * @tc.type: FUNC
 */
HWTEST_F(CommonCutilTest, DfxCutilTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCutilTest002: start.";
    ASSERT_FALSE(GetThreadName(nullptr, 0));
    GTEST_LOG_(INFO) << "DfxCutilTest002: end.";
}

/**
 * @tc.name: DfxCutilTest003
 * @tc.desc: test cutil functions GetTimeMilliseconds
 * @tc.type: FUNC
 */
HWTEST_F(CommonCutilTest, DfxCutilTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCutilTest003: start.";
    uint64_t msNow = GetTimeMilliseconds();
    GTEST_LOG_(INFO) << "current time(ms):" << msNow;
    ASSERT_NE(msNow, 0);
    GTEST_LOG_(INFO) << "DfxCutilTest003: end.";
}

/**
 * @tc.name: DfxCutilTest004
 * @tc.desc: test cutil functions TrimAndDupStr
 * @tc.type: FUNC
 */
HWTEST_F(CommonCutilTest, DfxCutilTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCutilTest004: start.";
    ASSERT_FALSE(TrimAndDupStr(nullptr, nullptr));
    GTEST_LOG_(INFO) << "DfxCutilTest004: end.";
}

/**
 * @tc.name: DfxCutilTest005
 * @tc.desc: test cutil functions TrimAndDupStr
 * @tc.type: FUNC
 */
HWTEST_F(CommonCutilTest, DfxCutilTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxCutilTest005: start.";
    const char src[] = "ab cd \n ef";
    char dst[11] = {0}; // 11: The length is consistent with the src[] array
    ASSERT_TRUE(TrimAndDupStr(src, dst));
    GTEST_LOG_(INFO) << "dst:" << dst;
    ASSERT_EQ(strncmp(dst, "abcd", 5), 0); // 5:length of "abcd"
    GTEST_LOG_(INFO) << "DfxCutilTest005: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS