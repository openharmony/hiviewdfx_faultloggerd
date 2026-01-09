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
#include "lperf_event_record.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class LperfEventRecordTest : public testing::Test {};

struct TestRecordSample {
    perf_event_header header_;
    LperfRecordSampleData data_;
};

/**
 * @tc.name: CreateLperfRecordTest001
 * @tc.desc: test LperfEventRecord type PERF_RECORD_SAMPLE
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventRecordTest, CreateLperfRecordTest001, TestSize.Level1)
{
    GTEST_LOG_(INFO) << "CreateLperfRecordTest001: start.";
    TestRecordSample sample = {
        {PERF_RECORD_SAMPLE, PERF_RECORD_MISC_KERNEL, sizeof(TestRecordSample)},
        {}};
    uint64_t ips[4] = {0, 1, 2, 3};
    sample.data_.ips = ips;
    sample.data_.nr = 4;
    sample.data_.pid = 2;
    sample.data_.tid = 3;
    sample.data_.time = 4;

    LperfRecordSample record = LperfRecordFactory::GetLperfRecord(PERF_RECORD_SAMPLE, (uint8_t *)&sample);
    EXPECT_EQ(record.GetType(), PERF_RECORD_SAMPLE);
    EXPECT_EQ(record.GetName(), "sample");
    EXPECT_EQ(record.data_.pid, 2);
    EXPECT_EQ(record.data_.tid, 3);
    EXPECT_EQ(record.data_.time, 4);
    EXPECT_EQ(record.data_.nr, 4);
    GTEST_LOG_(INFO) << "CreateLperfRecordTest001: end.";
}

/**
 * @tc.name: CreateLperfRecordTest002
 * @tc.desc: test LperfEventRecord invalid type
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventRecordTest, CreateLperfRecordTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CreateLperfRecordTest002: start.";
    TestRecordSample sample = {
        {PERF_RECORD_MMAP, PERF_RECORD_MISC_KERNEL, sizeof(TestRecordSample)},
        {}};
    uint64_t ips[4] = {0, 1, 2, 3};
    sample.data_.ips = ips;
    sample.data_.nr = 4;
    sample.data_.pid = 2;
    sample.data_.tid = 3;
    sample.data_.time = 4;
    LperfRecordSample record = LperfRecordFactory::GetLperfRecord(PERF_RECORD_MMAP, (uint8_t *)&sample);
    EXPECT_EQ(record.data_.pid, 0);
    EXPECT_EQ(record.data_.tid, 0);
    EXPECT_EQ(record.data_.time, 0);
    EXPECT_EQ(record.data_.nr, 0);
    EXPECT_EQ(record.data_.ips, nullptr);
    GTEST_LOG_(INFO) << "CreateLperfRecordTest002: end.";
}

/**
 * @tc.name: CreateLperfRecordTest003
 * @tc.desc: test LperfEventRecord invalid data
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventRecordTest, CreateLperfRecordTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CreateLperfRecordTest003: start.";
    LperfRecordSample record = LperfRecordFactory::GetLperfRecord(PERF_RECORD_SAMPLE, nullptr);
    EXPECT_EQ(record.GetType(), PERF_RECORD_MMAP);
    EXPECT_EQ(record.data_.pid, 0);
    EXPECT_EQ(record.data_.tid, 0);
    EXPECT_EQ(record.data_.time, 0);
    EXPECT_EQ(record.data_.nr, 0);
    EXPECT_EQ(record.data_.ips, nullptr);
    GTEST_LOG_(INFO) << "CreateLperfRecordTest003: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
