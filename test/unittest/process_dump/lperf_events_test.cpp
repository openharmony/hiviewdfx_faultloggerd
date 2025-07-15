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
#include "dfx_test_util.h"
#include "lperf_events.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class LperfEventsTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void LperfEventsTest::SetUpTestCase()
{}

void LperfEventsTest::TearDownTestCase()
{}

void LperfEventsTest::SetUp()
{}

void LperfEventsTest::TearDown()
{}

/**
 * @tc.name: LperfEventsTestTest002
 * @tc.desc: test LperfEvents invalid tids
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfEventsTestTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfEventsTestTest002: start.";
    if (IsLinuxKernel()) {
        return;
    }
    LperfEvents lperfEvents_;
    lperfEvents_.SetTid({});
    lperfEvents_.SetTimeOut(100);
    lperfEvents_.SetSampleFrequency(5000);
    EXPECT_EQ(lperfEvents_.PrepareRecord(), -1);
    GTEST_LOG_(INFO) << "LperfEventsTestTest002: end.";
}

/**
 * @tc.name: LperfEventsTestTest003
 * @tc.desc: test LperfEvents invalid freq
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfEventsTestTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfEventsTestTest003: start.";
    if (IsLinuxKernel()) {
        return;
    }
    LperfEvents lperfEvents_;
    lperfEvents_.SetTid({getpid()});
    lperfEvents_.SetTimeOut(2000);
    lperfEvents_.SetSampleFrequency(5000);
    EXPECT_EQ(lperfEvents_.PrepareRecord(), -1);
    GTEST_LOG_(INFO) << "LperfEventsTestTest003: end.";
}

/**
 * @tc.name: LperfEventsTestTest004
 * @tc.desc: test LperfEvents invalid freq -1
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfEventsTestTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfEventsTestTest004: start.";
    if (IsLinuxKernel()) {
        return;
    }
    LperfEvents lperfEvents_;
    lperfEvents_.SetTid({getpid()});
    lperfEvents_.SetTimeOut(5000);
    lperfEvents_.SetSampleFrequency(-1);
    EXPECT_EQ(lperfEvents_.PrepareRecord(), -1);
    GTEST_LOG_(INFO) << "LperfEventsTestTest004: end.";
}

/**
 * @tc.name: LperfEventsTestTest005
 * @tc.desc: test LperfEvents invalid time
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfEventsTestTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfEventsTestTest005: start.";
    if (IsLinuxKernel()) {
        return;
    }
    LperfEvents lperfEvents_;
    lperfEvents_.SetTid({getpid()});
    lperfEvents_.SetTimeOut(20000);
    lperfEvents_.SetSampleFrequency(100);
    EXPECT_EQ(lperfEvents_.PrepareRecord(), -1);
    GTEST_LOG_(INFO) << "LperfEventsTestTest005: end.";
}

/**
 * @tc.name: LperfEventsTestTest006
 * @tc.desc: test LperfEvents invalid time -1
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfEventsTestTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfEventsTestTest006: start.";
    if (IsLinuxKernel()) {
        return;
    }
    LperfEvents lperfEvents_;
    lperfEvents_.SetTid({getpid()});
    lperfEvents_.SetTimeOut(-1);
    lperfEvents_.SetSampleFrequency(100);
    EXPECT_EQ(lperfEvents_.PrepareRecord(), -1);
    GTEST_LOG_(INFO) << "LperfEventsTestTest006: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
