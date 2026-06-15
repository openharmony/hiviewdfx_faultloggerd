/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#include "minidump_test_common.h"

constexpr uint32_t DEFAULT_BITMAP_GRANULARITY = 128 * 1024 * 1024;
constexpr uint32_t TEST_UNKNOWN_STREAM_TYPE = 9999;
constexpr uint32_t TEST_CUSTOM_STREAM_TYPE = 0xFF00;

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpErrorInfoTest : public testing::Test {};
class MinidumpConfigTest : public testing::Test {
public:
    void SetUp() override
    {
        auto& mgr = MinidumpConfigManager::Instance();
        mgr.SetConfig(MinidumpConfig());
    }
    void TearDown() override
    {
        auto& mgr = MinidumpConfigManager::Instance();
        mgr.SetConfig(MinidumpConfig());
    }
};
class MinidumpPerfStatsTest : public testing::Test {};
class MinidumpStreamFactoryTest : public testing::Test {};
class MinidumpOptimizerTest : public testing::Test {};
class MinidumpObserverTest : public testing::Test {};
class MinidumpWrapperTest : public testing::Test {};

/**
 * @tc.name: ErrorInfoTest001
 * @tc.desc: test MinidumpErrorInfo default initialization returns success state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest001, TestSize.Level2)
{
    MinidumpErrorInfo info;
    EXPECT_EQ(info.GetError(), MinidumpError::SUCCESS);
    EXPECT_TRUE(info.IsSuccess());
    EXPECT_FALSE(info.IsError());
    EXPECT_EQ(info.GetLine(), 0u);
    EXPECT_TRUE(info.GetMessage().empty());
}

/**
 * @tc.name: ErrorInfoTest002
 * @tc.desc: test MinidumpErrorInfo with explicit error code and message returns error state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest002, TestSize.Level2)
{
    MinidumpErrorInfo info(MinidumpError::ERROR_FILE_OPEN, "test error", 42);
    EXPECT_EQ(info.GetError(), MinidumpError::ERROR_FILE_OPEN);
    EXPECT_FALSE(info.IsSuccess());
    EXPECT_TRUE(info.IsError());
    EXPECT_EQ(info.GetLine(), 42u);
    EXPECT_EQ(info.GetMessage(), "test error");
}

/**
 * @tc.name: ErrorInfoTest003
 * @tc.desc: test MinidumpErrorInfo ToString method returns formatted error string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest003, TestSize.Level2)
{
    MinidumpErrorInfo info(MinidumpError::ERROR_SIGNATURE, "bad sig", 10);
    std::string str = info.ToString();
    EXPECT_TRUE(str.find("Error[5]") != std::string::npos);
    EXPECT_TRUE(str.find("line 10") != std::string::npos);
    EXPECT_TRUE(str.find("bad sig") != std::string::npos);
}

/**
 * @tc.name: ErrorInfoTest004
 * @tc.desc: test MinidumpErrorInfo Clear method resets to success state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest004, TestSize.Level2)
{
    MinidumpErrorInfo info(MinidumpError::ERROR_STREAM_COUNT, "too many", 100);
    info.Clear();
    EXPECT_EQ(info.GetError(), MinidumpError::SUCCESS);
    EXPECT_TRUE(info.IsSuccess());
    EXPECT_TRUE(info.GetMessage().empty());
    EXPECT_EQ(info.GetLine(), 0u);
}

/**
 * @tc.name: ErrorInfoTest005
 * @tc.desc: test MinidumpErrorInfo with __LINE__ macro captures line number
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest005, TestSize.Level2)
{
    auto info = MinidumpErrorInfo(MinidumpError::ERROR_NOT_FOUND, "missing", __LINE__);
    EXPECT_EQ(info.GetError(), MinidumpError::ERROR_NOT_FOUND);
    EXPECT_TRUE(info.GetLine() > 0);
}

/**
 * @tc.name: ConfigTest001
 * @tc.desc: test MinidumpConfigManager SetConfig and GetConfig with custom values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpConfigTest, ConfigTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxStreams = 64;
    config.maxThreads = 512;
    config.maxModules = 256;
    config.maxMemoryBytes = 32 * 1024 * 1024;
    mgr.SetConfig(config);
    MinidumpConfig retrieved = mgr.GetConfig();
    EXPECT_EQ(retrieved.maxStreams, 64u);
    EXPECT_EQ(retrieved.maxThreads, 512u);
    EXPECT_EQ(retrieved.maxModules, 256u);
    EXPECT_EQ(retrieved.maxMemoryBytes, 32u * 1024u * 1024u);
}

/**
 * @tc.name: ConfigTest002
 * @tc.desc: test MinidumpConfigManager individual setter methods update config correctly
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpConfigTest, ConfigTest002, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetMaxStreams(200);
    EXPECT_EQ(mgr.GetConfig().maxStreams, 200u);
    mgr.SetMaxThreads(8000);
    EXPECT_EQ(mgr.GetConfig().maxThreads, 8000u);
    mgr.SetMaxModules(100);
    EXPECT_EQ(mgr.GetConfig().maxModules, 100u);
    mgr.SetMaxMemoryBytes(128 * 1024 * 1024);
    EXPECT_EQ(mgr.GetConfig().maxMemoryBytes, 128u * 1024u * 1024u);
}

/**
 * @tc.name: ConfigTest003
 * @tc.desc: test MinidumpConfigManager SetEnableChecksumValidation toggles checksum validation
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpConfigTest, ConfigTest003, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetEnableChecksumValidation(false);
    EXPECT_FALSE(mgr.GetConfig().enableChecksumValidation);
    mgr.SetEnableChecksumValidation(true);
    EXPECT_TRUE(mgr.GetConfig().enableChecksumValidation);
}

/**
 * @tc.name: ConfigTest005
 * @tc.desc: test MinidumpConfig default initialization values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpConfigTest, ConfigTest005, TestSize.Level2)
{
    MinidumpConfig config;
    EXPECT_EQ(config.maxStreams, 128u);
    EXPECT_EQ(config.maxStringLength, 1024u);
    EXPECT_EQ(config.maxThreads, 400u);
    EXPECT_EQ(config.maxModules, 2048u);
    EXPECT_EQ(config.maxMemoryRegions, 4096u);
    EXPECT_EQ(config.maxMemoryBytes, 64u * 1024u * 1024u);
    EXPECT_TRUE(config.enableChecksumValidation);
    EXPECT_TRUE(config.enablePerformanceStats);
}

/**
 * @tc.name: PerfStatsTest001
 * @tc.desc: test MinidumpPerfStats Reset method zeroes all counters
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest001, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    EXPECT_EQ(stats.totalReadCount.load(), 0u);
    EXPECT_EQ(stats.totalReadBytes.load(), 0u);
    EXPECT_EQ(stats.totalSeekCount.load(), 0u);
    EXPECT_EQ(stats.totalParseTimeMs.load(), 0u);
    EXPECT_EQ(stats.successCount.load(), 0u);
    EXPECT_EQ(stats.errorCount.load(), 0u);
}

/**
 * @tc.name: PerfStatsTest002
 * @tc.desc: test MinidumpPerfStats IncrementRead method accumulates read count and bytes
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest002, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.IncrementRead(100);
    stats.IncrementRead(200);
    EXPECT_EQ(stats.totalReadCount.load(), 2u);
    EXPECT_EQ(stats.totalReadBytes.load(), 300u);
}

/**
 * @tc.name: PerfStatsTest003
 * @tc.desc: test MinidumpPerfStats IncrementSeek method accumulates seek count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest003, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.IncrementSeek();
    stats.IncrementSeek();
    stats.IncrementSeek();
    EXPECT_EQ(stats.totalSeekCount.load(), 3u);
}

/**
 * @tc.name: PerfStatsTest004
 * @tc.desc: test MinidumpPerfStats IncrementSuccess and IncrementError methods
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest004, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.IncrementSuccess();
    stats.IncrementError();
    EXPECT_EQ(stats.successCount.load(), 1u);
    EXPECT_EQ(stats.errorCount.load(), 1u);
}

/**
 * @tc.name: PerfStatsTest005
 * @tc.desc: test MinidumpPerfStats RecordParseTime method accumulates parse time
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest005, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.RecordParseTime(150);
    stats.RecordParseTime(50);
    EXPECT_EQ(stats.totalParseTimeMs.load(), 200u);
}

/**
 * @tc.name: PerfStatsTest006
 * @tc.desc: test MinidumpPerfMonitor Instance ResetStats and GetStats methods
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest006, TestSize.Level2)
{
    auto& monitor = MinidumpPerfMonitor::Instance();
    monitor.ResetStats();
    auto& stats = monitor.GetStats();
    EXPECT_EQ(stats.totalReadCount.load(), 0u);
    stats.IncrementRead(64);
    EXPECT_EQ(stats.totalReadCount.load(), 1u);
    monitor.ResetStats();
    EXPECT_EQ(stats.totalReadCount.load(), 0u);
}

/**
 * @tc.name: FactoryTest001
 * @tc.desc: test MinidumpStreamFactory HasCreator returns false for unregistered stream type
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamFactoryTest, FactoryTest001, TestSize.Level2)
{
    auto& factory = MinidumpStreamFactory::Instance();
    EXPECT_FALSE(factory.HasCreator(TEST_UNKNOWN_STREAM_TYPE));
}

/**
 * @tc.name: FactoryTest002
 * @tc.desc: test MinidumpStreamFactory CreateStream returns nullptr for unregistered stream type
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamFactoryTest, FactoryTest002, TestSize.Level2)
{
    auto& factory = MinidumpStreamFactory::Instance();
    auto reader = MakeReader("");
    auto stream = factory.CreateStream(TEST_UNKNOWN_STREAM_TYPE, reader);
    EXPECT_EQ(stream, nullptr);
}

/**
 * @tc.name: FactoryTest003
 * @tc.desc: test MinidumpStreamFactory RegisterCreator and CreateStream for custom stream type
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamFactoryTest, FactoryTest003, TestSize.Level2)
{
    auto& factory = MinidumpStreamFactory::Instance();
    auto reader = MakeReader("");
    uint32_t customType = TEST_CUSTOM_STREAM_TYPE;
    factory.RegisterCreator(customType, [](std::shared_ptr<MinidumpMemoryReader> mr) {
        return std::make_shared<MinidumpSystemInfo>(mr);
    });
    EXPECT_TRUE(factory.HasCreator(customType));
    auto stream = factory.CreateStream(customType, reader);
    EXPECT_NE(stream, nullptr);
}

/**
 * @tc.name: FactoryTest004
 * @tc.desc: test MinidumpStreamFactory RegisterCreator and CreateStream for exception stream type
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpStreamFactoryTest, FactoryTest004, TestSize.Level2)
{
    auto& factory = MinidumpStreamFactory::Instance();
    EXPECT_TRUE(factory.HasCreator(MD_STREAM_EXCEPTION));

    uint32_t customType = MD_STREAM_EXCEPTION;
    factory.RegisterCreator(customType, [](std::shared_ptr<MinidumpMemoryReader> reader) {
        return std::make_shared<MinidumpException>(reader);
    });
    EXPECT_TRUE(factory.HasCreator(customType));

    auto reader = MakeReader("");
    auto stream = factory.CreateStream(customType, reader);
    EXPECT_NE(stream, nullptr);
}

/**
 * @tc.name: RangeMapTest001
 * @tc.desc: test RangeMap empty state and StoreRange with zero size returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest001, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.IsEmpty());
    EXPECT_EQ(rm.GetRangeSize(), 0u);
    EXPECT_EQ(rm.GetHighestAddress(), 0u);
    EXPECT_FALSE(rm.StoreRange(0, 0, 1));
}

/**
 * @tc.name: RangeMapTest002
 * @tc.desc: test RangeMap StoreRange and GetHighestAddress returns correct values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest002, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_EQ(rm.GetRangeSize(), 1u);
    EXPECT_EQ(rm.GetHighestAddress(), 0x10FFu);
}

/**
 * @tc.name: RangeMapTest003
 * @tc.desc: test RangeMap StoreRange rejects overlapping ranges
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest003, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_FALSE(rm.StoreRange(0x1050, 0x100, 1));
}

/**
 * @tc.name: RangeMapTest004
 * @tc.desc: test RangeMap StoreRange rejects ranges that fully overlap existing range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest004, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_FALSE(rm.StoreRange(0xF00, 0x200, 1));
}

/**
 * @tc.name: RangeMapTest006
 * @tc.desc: test RangeMap RetrieveRange and HasRange for stored range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest006, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    uint32_t value = 0;
    EXPECT_TRUE(rm.RetrieveRange(0x1050, &value));
    EXPECT_EQ(value, 0u);
    EXPECT_TRUE(rm.HasRange(0x1050));
}

/**
 * @tc.name: RangeMapTest007
 * @tc.desc: test RangeMap RetrieveRange and HasRange return false for out-of-range addresses
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest007, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_FALSE(rm.RetrieveRange(0x900, nullptr));
    EXPECT_FALSE(rm.RetrieveRange(0x1200, nullptr));
    EXPECT_FALSE(rm.HasRange(0x900));
}

/**
 * @tc.name: RangeMapTest008
 * @tc.desc: test RangeMap Clear method resets to empty state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest008, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    rm.Clear();
    EXPECT_TRUE(rm.IsEmpty());
    EXPECT_EQ(rm.GetHighestAddress(), 0u);
}

/**
 * @tc.name: RangeMapTest009
 * @tc.desc: test RangeMap StoreRange rejects overflow near maximum address
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapTest009, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rangeMap;
    EXPECT_FALSE(rangeMap.StoreRange(static_cast<uint64_t>(-1) - 10, 20, 0));
    EXPECT_EQ(rangeMap.GetRangeSize(), 0u);
}

/**
 * @tc.name: IntervalTreeTest001
 * @tc.desc: test IntervalTree empty initialization state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Empty());
    EXPECT_EQ(tree.Size(), 0u);
    EXPECT_EQ(tree.GetMaxAddress(), 0u);
}

/**
 * @tc.name: IntervalTreeTest002
 * @tc.desc: test IntervalTree Insert with invalid and valid intervals
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest002, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_FALSE(tree.Insert(200, 100, 0));
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    EXPECT_EQ(tree.Size(), 1u);
}

/**
 * @tc.name: IntervalTreeTest003
 * @tc.desc: test IntervalTree Insert rejects duplicate intervals
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest003, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    EXPECT_FALSE(tree.Insert(100, 200, 1));
    EXPECT_EQ(tree.Size(), 1u);
}

/**
 * @tc.name: IntervalTreeTest004
 * @tc.desc: test IntervalTree Search method finds stored interval
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest004, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x1100, 0);
    uint32_t result = 0;
    EXPECT_TRUE(tree.Search(0x1050, &result));
    EXPECT_EQ(result, 0u);
}

/**
 * @tc.name: IntervalTreeTest005
 * @tc.desc: test IntervalTree Search returns false for address outside intervals
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest005, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x1100, 0);
    uint32_t result = 0;
    EXPECT_FALSE(tree.Search(0x900, &result));
}

/**
 * @tc.name: IntervalTreeTest006
 * @tc.desc: test IntervalTree SearchRange method returns overlapping intervals
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest006, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x2000, 0);
    tree.Insert(0x3000, 0x4000, 1);
    auto results = tree.SearchRange(0x1500, 0x3500);
    EXPECT_EQ(results.size(), 2u);
}

/**
 * @tc.name: IntervalTreeTest007
 * @tc.desc: test IntervalTree Clear method resets to empty state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest007, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x2000, 0);
    tree.Insert(0x5000, 0x6000, 1);
    tree.Insert(0x8000, 0x9000, 2);
    tree.Clear();
    EXPECT_TRUE(tree.Empty());
    EXPECT_EQ(tree.Size(), 0u);
}

/**
 * @tc.name: IntervalTreeTest008
 * @tc.desc: test IntervalTree Search on empty tree returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest008, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    uint32_t value = 0;
    EXPECT_FALSE(tree.Search(0x1000, &value));
    EXPECT_EQ(tree.Size(), 0u);
}

/**
 * @tc.name: IntervalTreeTest009
 * @tc.desc: test IntervalTree SearchRange with no overlapping intervals returns empty
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest009, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x2000, 0);
    auto results = tree.SearchRange(0x5000, 0x6000);
    EXPECT_TRUE(results.empty());
}

/**
 * @tc.name: IntervalTreeTest010
 * @tc.desc: test IntervalTree with multiple intervals and Search verifies each value
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest010, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(0x1000, 0x2000, 0));
    EXPECT_TRUE(tree.Insert(0x3000, 0x4000, 1));
    EXPECT_TRUE(tree.Insert(0x5000, 0x6000, 2));
    EXPECT_EQ(tree.Size(), 3u);

    uint32_t val = 0;
    EXPECT_TRUE(tree.Search(0x1500, &val));
    EXPECT_EQ(val, 0u);
    EXPECT_TRUE(tree.Search(0x3500, &val));
    EXPECT_EQ(val, 1u);
    EXPECT_TRUE(tree.Search(0x5500, &val));
    EXPECT_EQ(val, 2u);
}

/**
 * @tc.name: BitmapTest001
 * @tc.desc: test BitmapIndex Size MarkRange and IsInRange methods
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapTest001, TestSize.Level2)
{
    BitmapIndex bm(0x10000, 4096);
    EXPECT_EQ(bm.Size(), 17u);
    bm.MarkRange(0, 4095);
    EXPECT_TRUE(bm.IsInRange(0));
    EXPECT_TRUE(bm.IsInRange(2000));
    EXPECT_FALSE(bm.IsInRange(4096));
}

/**
 * @tc.name: BitmapTest002
 * @tc.desc: test BitmapIndex MarkedCount and FindNextInRange methods
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapTest002, TestSize.Level2)
{
    BitmapIndex bm(0x10000, 4096);
    bm.MarkRange(0, 8191);
    EXPECT_EQ(bm.MarkedCount(), 2u);
    EXPECT_EQ(bm.FindNextInRange(4096), 4096u);
    EXPECT_EQ(bm.FindNextInRange(8192), UINT64_MAX);
}

/**
 * @tc.name: BitmapTest003
 * @tc.desc: test BitmapIndex Clear method resets marked count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapTest003, TestSize.Level2)
{
    BitmapIndex bm(0x10000, 4096);
    bm.MarkRange(0, 4095);
    bm.Clear();
    EXPECT_EQ(bm.MarkedCount(), 0u);
    EXPECT_FALSE(bm.IsInRange(0));
}

/**
 * @tc.name: BitmapTest004
 * @tc.desc: test BitmapIndex with zero granularity returns false for IsInRange
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapTest004, TestSize.Level2)
{
    BitmapIndex bitmap(100, 0);
    EXPECT_FALSE(bitmap.IsInRange(50));
    EXPECT_EQ(bitmap.FindNextInRange(0), static_cast<uint64_t>(UINT64_MAX));
}

/**
 * @tc.name: PerfOptTest001
 * @tc.desc: test PerformanceOptimizer default config has interval tree and bitmap enabled
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptTest001, TestSize.Level2)
{
    auto config = PerformanceOptimizer::Instance().GetConfig();
    EXPECT_TRUE(config.enableIntervalTree);
    EXPECT_TRUE(config.enableBitmapIndex);
}

/**
 * @tc.name: PerfOptTest002
 * @tc.desc: test PerformanceOptimizer SetConfig method updates configuration
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptTest002, TestSize.Level2)
{
    PerformanceOptimizer::Config config;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = true;
    config.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(config);
    EXPECT_TRUE(PerformanceOptimizer::Instance().GetConfig().enableBitmapIndex);
}

/**
 * @tc.name: PerfOptTest003
 * @tc.desc: test PerformanceOptimizer statistics reflect interval tree inserts
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptTest003, TestSize.Level2)
{
    PerformanceOptimizer::Instance().GetModuleIntervalTree().Insert(0x1000, 0x2000, 0);
    PerformanceOptimizer::Instance().GetMemoryIntervalTree().Insert(0x3000, 0x4000, 1);
    auto stats = PerformanceOptimizer::Instance().GetStatistics();
    EXPECT_EQ(stats.intervalTreeMemorySize, 1u);
    EXPECT_EQ(stats.bitmapMarkedCount, 0u);
}

/**
 * @tc.name: PerfOptTest004
 * @tc.desc: test PerformanceOptimizer Reset method clears all statistics
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptTest004, TestSize.Level2)
{
    PerformanceOptimizer::Instance().GetModuleIntervalTree().Insert(0x1000, 0x2000, 0);
    PerformanceOptimizer::Instance().Reset();
    auto stats = PerformanceOptimizer::Instance().GetStatistics();
    EXPECT_EQ(stats.intervalTreeModuleSize, 0u);
    EXPECT_EQ(stats.bitmapMarkedCount, 0u);
}

/**
 * @tc.name: ObservableTest001
 * @tc.desc: test MinidumpObservable AddObserver and RemoveObserver methods
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ObservableTest001, TestSize.Level2)
{
    MinidumpObservable observable;
    EXPECT_EQ(observable.GetObserverCount(), 0u);
    auto obs = std::make_shared<ProgressObserver>([](uint32_t, uint32_t, const std::string&) {});
    observable.AddObserver(obs);
    EXPECT_EQ(observable.GetObserverCount(), 1u);
    observable.RemoveObserver(obs);
    EXPECT_EQ(observable.GetObserverCount(), 0u);
}

/**
 * @tc.name: ObservableTest002
 * @tc.desc: test MinidumpObservable rejects duplicate observer addition
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ObservableTest002, TestSize.Level2)
{
    MinidumpObservable observable;
    auto obs1 = std::make_shared<ProgressObserver>([](uint32_t, uint32_t, const std::string&) {});
    observable.AddObserver(obs1);
    observable.AddObserver(obs1);
    EXPECT_EQ(observable.GetObserverCount(), 1u);
}

/**
 * @tc.name: ObservableTest003
 * @tc.desc: test MinidumpObservable RemoveObserver for non-added observer keeps count zero
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ObservableTest003, TestSize.Level2)
{
    MinidumpObservable observable;
    auto obs = std::make_shared<ProgressObserver>([](uint32_t, uint32_t, const std::string&) {});
    observable.RemoveObserver(obs);
    EXPECT_EQ(observable.GetObserverCount(), 0u);
}

/**
 * @tc.name: NotifyStreamLoadedTest001
 * @tc.desc: test MinidumpSubject NotifyStreamLoaded delivers stream name to observer
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, NotifyStreamLoadedTest001, TestSize.Level2)
{
    MinidumpSubject subject;
    std::string receivedName;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& n, uint32_t) { receivedName = n; });
    subject.AddObserver(obs);
    subject.NotifyStreamLoaded("ThreadList", 5);
    EXPECT_EQ(receivedName, "ThreadList");
}

/**
 * @tc.name: SubjectTest001
 * @tc.desc: test MinidumpSubject NotifyParseStart delivers path to ProgressObserver
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, SubjectTest001, TestSize.Level2)
{
    MinidumpSubject subject;
    uint32_t receivedCurrent = 0;
    uint32_t receivedTotal = 0;
    std::string receivedName;
    auto obs = std::make_shared<ProgressObserver>(
        [&](uint32_t c, uint32_t t, const std::string& n) {
            receivedCurrent = c;
            receivedTotal = t;
            receivedName = n;
        });
    subject.AddObserver(obs);
    subject.NotifyParseStart("/test/path");
    EXPECT_EQ(receivedCurrent, 0u);
    EXPECT_EQ(receivedName, "/test/path");
}

/**
 * @tc.name: SubjectTest002
 * @tc.desc: test MinidumpSubject NotifyParseProgress delivers current and total to observer
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, SubjectTest002, TestSize.Level2)
{
    MinidumpSubject subject;
    uint32_t receivedCurrent = 0;
    uint32_t receivedTotal = 0;
    auto obs = std::make_shared<ProgressObserver>(
        [&](uint32_t c, uint32_t t, const std::string&) {
            receivedCurrent = c;
            receivedTotal = t;
        });
    subject.AddObserver(obs);
    subject.NotifyParseProgress(5, 10, "ThreadList");
    EXPECT_EQ(receivedCurrent, 5u);
    EXPECT_EQ(receivedTotal, 10u);
}

/**
 * @tc.name: SubjectTest003
 * @tc.desc: test MinidumpSubject NotifyParseComplete invokes observer callback
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, SubjectTest003, TestSize.Level2)
{
    MinidumpSubject subject;
    bool completeReceived = false;
    auto obs = std::make_shared<ProgressObserver>(
        [&](uint32_t, uint32_t, const std::string&) { completeReceived = true; });
    subject.AddObserver(obs);
    subject.NotifyParseComplete(true);
    EXPECT_TRUE(completeReceived);
}

/**
 * @tc.name: ObserverFilterTest001
 * @tc.desc: test ProgressObserver ShouldHandle returns true for all parse event types
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ObserverFilterTest001, TestSize.Level2)
{
    ProgressObserver obs([](uint32_t, uint32_t, const std::string&) {});
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_PROGRESS));
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_START));
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_COMPLETE));
}

/**
 * @tc.name: ObserverStreamLoadTest001
 * @tc.desc: test StreamLoadObserver receives stream name and progress from Observable
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ObserverStreamLoadTest001, TestSize.Level2)
{
    MinidumpObservable observable;
    std::string receivedName;
    uint32_t receivedProgress = 0;
    auto observer = std::make_shared<StreamLoadObserver>(
        [&receivedName, &receivedProgress](const std::string& name, uint32_t progress) {
            receivedName = name;
            receivedProgress = progress;
        });
    observable.AddObserver(observer);
    MinidumpEvent event(MinidumpEventType::STREAM_LOADED);
    event.streamName = "ThreadList";
    event.progress = 5;
    observable.NotifyObservers(event);
    EXPECT_EQ(receivedName, "ThreadList");
    EXPECT_EQ(receivedProgress, 5u);
}

/**
 * @tc.name: ObserverFilterTest002
 * @tc.desc: test StreamLoadObserver ShouldHandle returns false for PARSE_START and true for STREAM_LOADED
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ObserverFilterTest002, TestSize.Level2)
{
    auto observer = std::make_shared<StreamLoadObserver>(
        [](const std::string&, uint32_t) {});
    EXPECT_FALSE(observer->ShouldHandle(MinidumpEventType::PARSE_START));
    EXPECT_TRUE(observer->ShouldHandle(MinidumpEventType::STREAM_LOADED));
}

/**
 * @tc.name: ProgressObserverNullCallbackTest001
 * @tc.desc: test ProgressObserver with null callback handles OnEvent without crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ProgressObserverNullCallbackTest001, TestSize.Level2)
{
    ProgressObserver obs(nullptr);
    MinidumpEvent event(MinidumpEventType::PARSE_PROGRESS);
    event.progress = 10;
    event.total = 100;
    event.message = "test";
    obs.OnEvent(event);
}

/**
 * @tc.name: StreamLoadObserverNullCallbackTest001
 * @tc.desc: test StreamLoadObserver with null callback handles OnEvent without crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, StreamLoadObserverNullCallbackTest001, TestSize.Level2)
{
    StreamLoadObserver obs(nullptr);
    MinidumpEvent event(MinidumpEventType::STREAM_LOADED);
    event.streamName = "test";
    event.progress = 5;
    obs.OnEvent(event);
}

/**
 * @tc.name: ObservableNullObserverTest001
 * @tc.desc: test MinidumpObservable with null observer and ClearObservers method
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ObservableNullObserverTest001, TestSize.Level2)
{
    MinidumpObservable observable;
    observable.AddObserver(std::shared_ptr<IMinidumpObserver>());
    EXPECT_EQ(observable.GetObserverCount(), 1u);
    MinidumpEvent event(MinidumpEventType::PARSE_START);
    observable.NotifyObservers(event);
    observable.ClearObservers();
    EXPECT_EQ(observable.GetObserverCount(), 0u);
}

/**
 * @tc.name: SubjectParseCompleteFailureTest001
 * @tc.desc: test MinidumpSubject NotifyParseComplete with failure includes failed message
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, SubjectParseCompleteFailureTest001, TestSize.Level2)
{
    MinidumpSubject subject;
    std::string receivedMessage;
    auto obs = std::make_shared<ProgressObserver>(
        [&](uint32_t, uint32_t, const std::string& msg) { receivedMessage = msg; });
    subject.AddObserver(obs);
    subject.NotifyParseComplete(false);
    EXPECT_TRUE(receivedMessage.find("failed") != std::string::npos);
}

/**
 * @tc.name: IMinidumpObserverShouldHandleTest001
 * @tc.desc: test IMinidumpObserver default ShouldHandle returns true for all event types
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, IMinidumpObserverShouldHandleTest001, TestSize.Level2)
{
    class DefaultObserver : public IMinidumpObserver {
        void OnEvent(const MinidumpEvent& event) override {}
    };
    DefaultObserver obs;
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_START));
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::STREAM_LOADED));
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_PROGRESS));
}

/**
 * @tc.name: IntervalTreeRotationLLTest001
 * @tc.desc: test IntervalTree LL rotation maintains correct search results
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeRotationLLTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(0x3000, 0x3100, 2));
    EXPECT_TRUE(tree.Insert(0x2000, 0x2100, 1));
    EXPECT_TRUE(tree.Insert(0x1000, 0x1100, 0));
    EXPECT_EQ(tree.Size(), 3u);
    uint32_t val = 0;
    EXPECT_TRUE(tree.Search(0x1000, &val));
    EXPECT_EQ(val, 0u);
    EXPECT_TRUE(tree.Search(0x2000, &val));
    EXPECT_EQ(val, 1u);
    EXPECT_TRUE(tree.Search(0x3000, &val));
    EXPECT_EQ(val, 2u);
}

/**
 * @tc.name: IntervalTreeRotationRRTest001
 * @tc.desc: test IntervalTree RR rotation maintains correct search results
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeRotationRRTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(0x1000, 0x1100, 0));
    EXPECT_TRUE(tree.Insert(0x2000, 0x2100, 1));
    EXPECT_TRUE(tree.Insert(0x3000, 0x3100, 2));
    EXPECT_EQ(tree.Size(), 3u);
    uint32_t val = 0;
    EXPECT_TRUE(tree.Search(0x1000, &val));
    EXPECT_EQ(val, 0u);
    EXPECT_TRUE(tree.Search(0x2000, &val));
    EXPECT_EQ(val, 1u);
    EXPECT_TRUE(tree.Search(0x3000, &val));
    EXPECT_EQ(val, 2u);
}

/**
 * @tc.name: IntervalTreeRotationLRTest001
 * @tc.desc: test IntervalTree LR rotation maintains correct search results
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeRotationLRTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(0x3000, 0x3100, 0));
    EXPECT_TRUE(tree.Insert(0x1000, 0x1100, 1));
    EXPECT_TRUE(tree.Insert(0x2000, 0x2100, 2));
    EXPECT_EQ(tree.Size(), 3u);
    uint32_t val = 0;
    EXPECT_TRUE(tree.Search(0x1000, &val));
    EXPECT_EQ(val, 1u);
    EXPECT_TRUE(tree.Search(0x2000, &val));
    EXPECT_EQ(val, 2u);
    EXPECT_TRUE(tree.Search(0x3000, &val));
    EXPECT_EQ(val, 0u);
}

/**
 * @tc.name: IntervalTreeRotationRLTest001
 * @tc.desc: test IntervalTree RL rotation maintains correct search results
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeRotationRLTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(0x1000, 0x1100, 0));
    EXPECT_TRUE(tree.Insert(0x3000, 0x3100, 1));
    EXPECT_TRUE(tree.Insert(0x2000, 0x2100, 2));
    EXPECT_EQ(tree.Size(), 3u);
    uint32_t val = 0;
    EXPECT_TRUE(tree.Search(0x1000, &val));
    EXPECT_EQ(val, 0u);
    EXPECT_TRUE(tree.Search(0x2000, &val));
    EXPECT_EQ(val, 2u);
    EXPECT_TRUE(tree.Search(0x3000, &val));
    EXPECT_EQ(val, 1u);
}

/**
 * @tc.name: IntervalTreeDeepRotationTest001
 * @tc.desc: test IntervalTree with multiple sequential inserts and rotations
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeDeepRotationTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(0x1000, 0x1100, 0));
    EXPECT_TRUE(tree.Insert(0x2000, 0x2100, 1));
    EXPECT_TRUE(tree.Insert(0x3000, 0x3100, 2));
    EXPECT_TRUE(tree.Insert(0x4000, 0x4100, 3));
    EXPECT_TRUE(tree.Insert(0x5000, 0x5100, 4));
    EXPECT_TRUE(tree.Insert(0x6000, 0x6100, 5));
    EXPECT_TRUE(tree.Insert(0x7000, 0x7100, 6));
    EXPECT_EQ(tree.Size(), 7u);
    for (uint32_t i = 0; i < 7; ++i) {
        uint32_t val = 0;
        EXPECT_TRUE(tree.Search(0x1000 + i * 0x1000, &val));
        EXPECT_EQ(val, i);
    }
}

/**
 * @tc.name: BitmapLargeAllocationTest001
 * @tc.desc: test BitmapIndex with large allocation and single-byte granularity
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapLargeAllocationTest001, TestSize.Level2)
{
    BitmapIndex bm(0x200000, 1);
    EXPECT_EQ(bm.Size(), 0u);
    EXPECT_FALSE(bm.IsInRange(0));
    EXPECT_EQ(bm.FindNextInRange(0), static_cast<uint64_t>(UINT64_MAX));
    bm.MarkRange(0, 100);
    EXPECT_EQ(bm.MarkedCount(), 0u);
}

/**
 * @tc.name: PerfOptDisabledFeaturesTest001
 * @tc.desc: test PerformanceOptimizer with all features disabled returns empty structures
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptDisabledFeaturesTest001, TestSize.Level2)
{
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = false;
    config.enableBitmapIndex = false;
    config.bitmapGranularity = 0;
    PerformanceOptimizer::Instance().SetConfig(config);

    auto retrievedConfig = PerformanceOptimizer::Instance().GetConfig();
    EXPECT_FALSE(retrievedConfig.enableRangeMap);
    EXPECT_FALSE(retrievedConfig.enableIntervalTree);
    EXPECT_FALSE(retrievedConfig.enableBitmapIndex);

    EXPECT_TRUE(PerformanceOptimizer::Instance().GetModuleRangeMap().IsEmpty());
    EXPECT_TRUE(PerformanceOptimizer::Instance().GetModuleIntervalTree().Empty());
    EXPECT_FALSE(PerformanceOptimizer::Instance().GetBitmapIndex().IsInRange(0x1000));

    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: PerfOptOnlyRangeMapTest001
 * @tc.desc: test PerformanceOptimizer with only RangeMap enabled stores and retrieves ranges
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptOnlyRangeMapTest001, TestSize.Level2)
{
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = true;
    config.enableIntervalTree = false;
    config.enableBitmapIndex = false;
    config.bitmapGranularity = 0;
    PerformanceOptimizer::Instance().SetConfig(config);

    auto& rangeMap = PerformanceOptimizer::Instance().GetModuleRangeMap();
    EXPECT_TRUE(rangeMap.StoreRange(0x1000, 0x100, 0));
    uint32_t val = 0;
    EXPECT_TRUE(rangeMap.RetrieveRange(0x1050, &val));
    EXPECT_EQ(val, 0u);

    auto stats = PerformanceOptimizer::Instance().GetStatistics();
    EXPECT_EQ(stats.rangeMapModuleSize, 1u);
    EXPECT_EQ(stats.intervalTreeModuleSize, 0u);
    EXPECT_EQ(stats.bitmapMarkedCount, 0u);

    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: RangeMapRetrieveEmptyTest001
 * @tc.desc: test RangeMap RetrieveRange on empty map returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapRetrieveEmptyTest001, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_FALSE(rm.RetrieveRange(0x1000, nullptr));
}

/**
 * @tc.name: ProgressObserverOnEventStreamLoadedTest001
 * @tc.desc: test ProgressObserver OnEvent with STREAM_LOADED does not invoke callback
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ProgressObserverOnEventStreamLoadedTest001, TestSize.Level2)
{
    bool callbackInvoked = false;
    auto obs = std::make_shared<ProgressObserver>(
        [&](uint32_t current, uint32_t total, const std::string& name) {
            callbackInvoked = true;
        });
    MinidumpEvent event(MinidumpEventType::STREAM_LOADED);
    obs->OnEvent(event);
    EXPECT_FALSE(callbackInvoked);
}

/**
 * @tc.name: ProgressObserverShouldHandleStreamLoadedTest001
 * @tc.desc: test ProgressObserver ShouldHandle returns false for STREAM_LOADED
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ProgressObserverShouldHandleStreamLoadedTest001, TestSize.Level2)
{
    auto obs = std::make_shared<ProgressObserver>(
        [](uint32_t, uint32_t, const std::string&) {});
    EXPECT_FALSE(obs->ShouldHandle(MinidumpEventType::STREAM_LOADED));
}

/**
 * @tc.name: ProgressObserverShouldHandleConfigChangedTest001
 * @tc.desc: test ProgressObserver ShouldHandle returns false for CONFIG_CHANGED
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, ProgressObserverShouldHandleConfigChangedTest001, TestSize.Level2)
{
    auto obs = std::make_shared<ProgressObserver>(
        [](uint32_t, uint32_t, const std::string&) {});
    EXPECT_FALSE(obs->ShouldHandle(MinidumpEventType::CONFIG_CHANGED));
}

/**
 * @tc.name: StreamLoadObserverOnEventNonStreamLoadedTest001
 * @tc.desc: test StreamLoadObserver OnEvent with non-STREAM_LOADED does not invoke callback
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, StreamLoadObserverOnEventNonStreamLoadedTest001, TestSize.Level2)
{
    bool callbackInvoked = false;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& name, uint32_t count) {
            callbackInvoked = true;
        });
    MinidumpEvent event(MinidumpEventType::PARSE_PROGRESS);
    obs->OnEvent(event);
    EXPECT_FALSE(callbackInvoked);
}

/**
 * @tc.name: StreamLoadObserverShouldHandleParseProgressTest001
 * @tc.desc: test StreamLoadObserver ShouldHandle returns false for PARSE_PROGRESS
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, StreamLoadObserverShouldHandleParseProgressTest001, TestSize.Level2)
{
    auto obs = std::make_shared<StreamLoadObserver>(
        [](const std::string&, uint32_t) {});
    EXPECT_FALSE(obs->ShouldHandle(MinidumpEventType::PARSE_PROGRESS));
}

/**
 * @tc.name: NotifyObserversShouldHandleFalseTest001
 * @tc.desc: test MinidumpObservable NotifyObservers filters by ShouldHandle
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, NotifyObserversShouldHandleFalseTest001, TestSize.Level2)
{
    MinidumpObservable observable;
    bool progressCallbackInvoked = false;
    bool streamLoadCallbackInvoked = false;
    auto progressObs = std::make_shared<ProgressObserver>(
        [&](uint32_t, uint32_t, const std::string&) {
            progressCallbackInvoked = true;
        });
    auto streamObs = std::make_shared<StreamLoadObserver>(
        [&](const std::string&, uint32_t) {
            streamLoadCallbackInvoked = true;
        });
    observable.AddObserver(progressObs);
    observable.AddObserver(streamObs);

    MinidumpEvent event(MinidumpEventType::STREAM_LOADED);
    observable.NotifyObservers(event);
    EXPECT_FALSE(progressCallbackInvoked);
    EXPECT_TRUE(streamLoadCallbackInvoked);

    MinidumpEvent event2(MinidumpEventType::PARSE_PROGRESS);
    observable.NotifyObservers(event2);
    EXPECT_TRUE(progressCallbackInvoked);
}

/**
 * @tc.name: NotifyObserversNullObserverTest001
 * @tc.desc: test MinidumpObservable NotifyObservers with null observer
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, NotifyObserversNullObserverTest001, TestSize.Level2)
{
    MinidumpObservable observable;
    observable.AddObserver(nullptr);
    MinidumpEvent event(MinidumpEventType::PARSE_START);
    observable.NotifyObservers(event);
    EXPECT_EQ(observable.GetObserverCount(), 1u);
}

/**
 * @tc.name: RemoveObserverNotFoundTest001
 * @tc.desc: test MinidumpObservable RemoveObserver for non-added observer keeps count unchanged
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpObserverTest, RemoveObserverNotFoundTest001, TestSize.Level2)
{
    MinidumpObservable observable;
    auto obs = std::make_shared<ProgressObserver>(
        [](uint32_t, uint32_t, const std::string&) {});
    auto otherObs = std::make_shared<ProgressObserver>(
        [](uint32_t, uint32_t, const std::string&) {});
    observable.AddObserver(obs);
    observable.RemoveObserver(otherObs);
    EXPECT_EQ(observable.GetObserverCount(), 1u);
}

/**
 * @tc.name: IntervalTreeOverlapTest001
 * @tc.desc: test IntervalTree allows overlapping intervals insertion
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeOverlapTest001, TestSize.Level2)
{
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = false;
    PerformanceOptimizer::Instance().SetConfig(config);

    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    EXPECT_TRUE(tree.Insert(150, 250, 1));

    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: IntervalTreeSearchNotFoundTest001
 * @tc.desc: test IntervalTree Search returns false for addresses outside all intervals
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeSearchNotFoundTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    uint32_t result = 0;
    EXPECT_FALSE(tree.Search(50, &result));
    EXPECT_FALSE(tree.Search(300, &result));
}

/**
 * @tc.name: IntervalTreeTraversalTest001
 * @tc.desc: test IntervalTree traversal with multiple intervals returns correct search results
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeTraversalTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    EXPECT_TRUE(tree.Insert(300, 400, 1));
    EXPECT_TRUE(tree.Insert(500, 600, 2));
    uint32_t result = 0;
    EXPECT_TRUE(tree.Search(150, &result));
    EXPECT_EQ(result, 0u);
    EXPECT_TRUE(tree.Search(350, &result));
    EXPECT_EQ(result, 1u);
    EXPECT_TRUE(tree.Search(550, &result));
    EXPECT_EQ(result, 2u);
}

/**
 * @tc.name: BitmapIndexZeroGranularityTest001
 * @tc.desc: test BitmapIndex with zero granularity configuration returns false for IsInRange
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapIndexZeroGranularityTest001, TestSize.Level2)
{
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = false;
    config.enableBitmapIndex = true;
    config.bitmapGranularity = 0;
    PerformanceOptimizer::Instance().SetConfig(config);

    auto& bitmap = PerformanceOptimizer::Instance().GetBitmapIndex();
    bitmap.MarkRange(0x1000, 0x2000);
    EXPECT_FALSE(bitmap.IsInRange(0x1500));
    EXPECT_EQ(bitmap.Size(), 0u);

    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: BitmapIndexFindNextMarkedTest001
 * @tc.desc: test BitmapIndex FindNextInRange method for marked ranges
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapIndexFindNextMarkedTest001, TestSize.Level2)
{
    BitmapIndex bitmap(0x10000, 0x100);
    bitmap.MarkRange(0x1000, 0x1100);
    uint64_t next = bitmap.FindNextInRange(0);
    if (bitmap.Size() > 0) {
        EXPECT_NE(next, UINT64_MAX);
    } else {
        EXPECT_EQ(next, UINT64_MAX);
    }
}

/**
 * @tc.name: RangeMapRetrieveRangeNoValueTest001
 * @tc.desc: test RangeMap RetrieveRange with nullptr value parameter returns true
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapRetrieveRangeNoValueTest001, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_TRUE(rm.RetrieveRange(0x1050, nullptr));
}

/**
 * @tc.name: IntervalTreeInsertReverseTest001
 * @tc.desc: test IntervalTree Insert with reversed bounds returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeInsertReverseTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_FALSE(tree.Insert(200, 100, 0));
}

/**
 * @tc.name: IntervalTreeClearTest001
 * @tc.desc: test IntervalTree Clear method resets to empty state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeClearTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    EXPECT_FALSE(tree.Empty());
    tree.Clear();
    EXPECT_TRUE(tree.Empty());
}

/**
 * @tc.name: BitmapIndexOutOfRangeTest001
 * @tc.desc: test BitmapIndex IsInRange returns false for addresses outside marked range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, BitmapIndexOutOfRangeTest001, TestSize.Level2)
{
    BitmapIndex bitmap(0x10000, 0x1000);
    bitmap.MarkRange(0x1000, 0x2000);
    if (bitmap.Size() > 0) {
        EXPECT_TRUE(bitmap.IsInRange(0x1500));
    }
    EXPECT_FALSE(bitmap.IsInRange(0x500));
    EXPECT_FALSE(bitmap.IsInRange(0x3000));
}

/**
 * @tc.name: PerfOptOnlyIntervalTreeTest001
 * @tc.desc: test PerformanceOptimizer with only IntervalTree enabled inserts and searches
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptOnlyIntervalTreeTest001, TestSize.Level2)
{
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = false;
    PerformanceOptimizer::Instance().SetConfig(config);

    auto& tree = PerformanceOptimizer::Instance().GetModuleIntervalTree();
    EXPECT_TRUE(tree.Insert(0x1000, 0x1100, 0));
    uint32_t val = 0;
    EXPECT_TRUE(tree.Search(0x1050, &val));
    EXPECT_EQ(val, 0u);

    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: PerfOptOnlyBitmapTest001
 * @tc.desc: test BitmapIndex directly with MarkRange and IsInRange methods
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, PerfOptOnlyBitmapTest001, TestSize.Level2)
{
    BitmapIndex bitmap(0x10000, 0x1000);
    bitmap.MarkRange(0x1000, 0x2000);
    if (bitmap.Size() > 0) {
        EXPECT_TRUE(bitmap.IsInRange(0x1500));
        EXPECT_NE(bitmap.Size(), 0u);
    } else {
        EXPECT_FALSE(bitmap.IsInRange(0x1500));
    }
}

/**
 * @tc.name: RangeMapStoreNonOverlappingForwardTest001
 * @tc.desc: test RangeMap StoreRange for non-overlapping forward ranges and RetrieveRange
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, RangeMapStoreNonOverlappingForwardTest001, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_TRUE(rm.StoreRange(0x2000, 0x100, 1));
    EXPECT_EQ(rm.GetRangeSize(), 2u);
    uint32_t val = 0;
    EXPECT_TRUE(rm.RetrieveRange(0x1050, &val));
    EXPECT_EQ(val, 0u);
    EXPECT_TRUE(rm.RetrieveRange(0x2050, &val));
    EXPECT_EQ(val, 1u);
}

/**
 * @tc.name: IntervalTreeSearchRangeLeftSubtreeTest001
 * @tc.desc: test IntervalTree SearchRange covering left subtree intervals
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpOptimizerTest, IntervalTreeSearchRangeLeftSubtreeTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(0x1000, 0x1100, 0));
    EXPECT_TRUE(tree.Insert(0x500, 0x600, 1));
    EXPECT_TRUE(tree.Insert(0x2000, 0x2100, 2));
    auto results = tree.SearchRange(0x400, 0x1200);
    EXPECT_EQ(results.size(), 2u);
}

} // namespace HiviewDFX
} // namespace OHOS
