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

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <linux/perf_event.h>
#include "dfx_test_util.h"
#include "lperf_events.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class LperfEventsTest : public testing::Test {};

/**
 * @tc.name: LperfEventsTestTest002
 * @tc.desc: test LperfEvents invalid tids
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfEventsTestTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfEventsTestTest002: start.";
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
    LperfEvents lperfEvents_;
    lperfEvents_.SetTid({getpid()});
    lperfEvents_.SetTimeOut(20000);
    lperfEvents_.SetSampleFrequency(100);
    EXPECT_EQ(lperfEvents_.PrepareRecord(), -1);
    GTEST_LOG_(INFO) << "LperfEventsTestTest005: end.";
}

/**
 * @tc.name: LperfReadRecordsNullMmapPageTest001
 * @tc.desc: test LperfEvents ReadRecordsFromMmaps with null mmapPage returns safely
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfReadRecordsNullMmapPageTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfReadRecordsNullMmapPageTest001: start.";
    LperfEvents lperfEvents_;
    lperfEvents_.lperfMmap_.mmapPage = nullptr;
    lperfEvents_.lperfMmap_.buf = nullptr;
    lperfEvents_.lperfMmap_.bufSize = 0;
    lperfEvents_.ReadRecordsFromMmaps();
    EXPECT_EQ(lperfEvents_.lperfMmap_.dataSize, 0u);
    GTEST_LOG_(INFO) << "LperfReadRecordsNullMmapPageTest001: end.";
}

/**
 * @tc.name: LperfReadRecordsEmptyBufferTest001
 * @tc.desc: test LperfEvents ReadRecordsFromMmaps with empty buffer does not invoke callback
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfReadRecordsEmptyBufferTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfReadRecordsEmptyBufferTest001: start.";
    LperfEvents lperfEvents_;
    uint8_t dataBuf[64] = {0};
    perf_event_mmap_page page = {};
    page.data_head = 0;
    page.data_tail = 0;
    lperfEvents_.lperfMmap_.mmapPage = &page;
    lperfEvents_.lperfMmap_.buf = dataBuf;
    lperfEvents_.lperfMmap_.bufSize = sizeof(dataBuf);
    bool callbackCalled = false;
    lperfEvents_.SetRecordCallBack([&callbackCalled](LperfRecordSample&) { callbackCalled = true; });
    lperfEvents_.ReadRecordsFromMmaps();
    EXPECT_FALSE(callbackCalled);
    EXPECT_EQ(lperfEvents_.lperfMmap_.dataSize, 0u);
    GTEST_LOG_(INFO) << "LperfReadRecordsEmptyBufferTest001: end.";
}

/**
 * @tc.name: LperfReadRecordsHugeDataSizeBailTest001
 * @tc.desc: test LperfEvents ReadRecordsFromMmaps with corrupt huge dataSize bails without runaway
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfReadRecordsHugeDataSizeBailTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfReadRecordsHugeDataSizeBailTest001: start.";
    LperfEvents lperfEvents_;
    uint8_t dataBuf[64] = {0};
    perf_event_mmap_page page = {};
    page.data_head = 0x10000000ULL; // 256MiB, far beyond bufSize, simulates corruption
    page.data_tail = 0;
    lperfEvents_.lperfMmap_.mmapPage = &page;
    lperfEvents_.lperfMmap_.buf = dataBuf;
    lperfEvents_.lperfMmap_.bufSize = sizeof(dataBuf);
    lperfEvents_.lperfMmap_.dataSize = 0;
    bool callbackCalled = false;
    lperfEvents_.SetRecordCallBack([&callbackCalled](LperfRecordSample&) { callbackCalled = true; });
    lperfEvents_.ReadRecordsFromMmaps();
    EXPECT_FALSE(callbackCalled);
    EXPECT_EQ(lperfEvents_.lperfMmap_.dataSize, 0u);
    EXPECT_EQ(page.data_tail, page.data_head);
    GTEST_LOG_(INFO) << "LperfReadRecordsHugeDataSizeBailTest001: end.";
}

/**
 * @tc.name: LperfGetRecordFromMmapSizeGuardTest001
 * @tc.desc: test LperfEvents GetRecordFromMmap with header size exceeding dataSize stops drain
 * @tc.type: FUNC
 */
HWTEST_F(LperfEventsTest, LperfGetRecordFromMmapSizeGuardTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LperfGetRecordFromMmapSizeGuardTest001: start.";
    LperfEvents lperfEvents_;
    uint8_t dataBuf[128] = {0};
    perf_event_mmap_page page = {};
    page.data_tail = 0;
    lperfEvents_.lperfMmap_.mmapPage = &page;
    lperfEvents_.lperfMmap_.buf = dataBuf;
    lperfEvents_.lperfMmap_.bufSize = sizeof(dataBuf);
    lperfEvents_.lperfMmap_.dataSize = 8;
    perf_event_header hdr = {};
    hdr.type = PERF_RECORD_SAMPLE;
    hdr.size = 100; // exceeds dataSize(8)
    lperfEvents_.lperfMmap_.header = hdr;
    bool callbackCalled = false;
    lperfEvents_.SetRecordCallBack([&callbackCalled](LperfRecordSample&) { callbackCalled = true; });
    lperfEvents_.GetRecordFromMmap(lperfEvents_.lperfMmap_);
    EXPECT_FALSE(callbackCalled);
    EXPECT_EQ(lperfEvents_.lperfMmap_.dataSize, 0u);
    GTEST_LOG_(INFO) << "LperfGetRecordFromMmapSizeGuardTest001: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
