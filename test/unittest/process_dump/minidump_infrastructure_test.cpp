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

HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest001, TestSize.Level2)
{
    MinidumpErrorInfo info;
    EXPECT_EQ(info.GetError(), MinidumpError::SUCCESS);
    EXPECT_TRUE(info.IsSuccess());
    EXPECT_FALSE(info.IsError());
    EXPECT_EQ(info.GetLine(), 0u);
    EXPECT_TRUE(info.GetMessage().empty());
}

HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest002, TestSize.Level2)
{
    MinidumpErrorInfo info(MinidumpError::ERROR_FILE_OPEN, "test error", 42);
    EXPECT_EQ(info.GetError(), MinidumpError::ERROR_FILE_OPEN);
    EXPECT_FALSE(info.IsSuccess());
    EXPECT_TRUE(info.IsError());
    EXPECT_EQ(info.GetLine(), 42u);
    EXPECT_EQ(info.GetMessage(), "test error");
}

HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest003, TestSize.Level2)
{
    MinidumpErrorInfo info(MinidumpError::ERROR_SIGNATURE, "bad sig", 10);
    std::string str = info.ToString();
    EXPECT_TRUE(str.find("Error[5]") != std::string::npos);
    EXPECT_TRUE(str.find("line 10") != std::string::npos);
    EXPECT_TRUE(str.find("bad sig") != std::string::npos);
}

HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest004, TestSize.Level2)
{
    MinidumpErrorInfo info(MinidumpError::ERROR_STREAM_COUNT, "too many", 100);
    info.Clear();
    EXPECT_EQ(info.GetError(), MinidumpError::SUCCESS);
    EXPECT_TRUE(info.IsSuccess());
    EXPECT_TRUE(info.GetMessage().empty());
    EXPECT_EQ(info.GetLine(), 0u);
}

HWTEST_F(MinidumpErrorInfoTest, ErrorInfoTest005, TestSize.Level2)
{
    auto info = MinidumpErrorInfo(MinidumpError::ERROR_NOT_FOUND, "missing", __LINE__);
    EXPECT_EQ(info.GetError(), MinidumpError::ERROR_NOT_FOUND);
    EXPECT_TRUE(info.GetLine() > 0);
}

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

HWTEST_F(MinidumpConfigTest, ConfigTest003, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetEnableChecksumValidation(false);
    EXPECT_FALSE(mgr.GetConfig().enableChecksumValidation);
    mgr.SetEnableChecksumValidation(true);
    EXPECT_TRUE(mgr.GetConfig().enableChecksumValidation);
}

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

HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest002, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.IncrementRead(100);
    stats.IncrementRead(200);
    EXPECT_EQ(stats.totalReadCount.load(), 2u);
    EXPECT_EQ(stats.totalReadBytes.load(), 300u);
}

HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest003, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.IncrementSeek();
    stats.IncrementSeek();
    stats.IncrementSeek();
    EXPECT_EQ(stats.totalSeekCount.load(), 3u);
}

HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest004, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.IncrementSuccess();
    stats.IncrementError();
    EXPECT_EQ(stats.successCount.load(), 1u);
    EXPECT_EQ(stats.errorCount.load(), 1u);
}

HWTEST_F(MinidumpPerfStatsTest, PerfStatsTest005, TestSize.Level2)
{
    MinidumpPerfStats stats;
    stats.Reset();
    stats.RecordParseTime(150);
    stats.RecordParseTime(50);
    EXPECT_EQ(stats.totalParseTimeMs.load(), 200u);
}

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

HWTEST_F(MinidumpStreamFactoryTest, FactoryTest001, TestSize.Level2)
{
    auto& factory = MinidumpStreamFactory::Instance();
    EXPECT_FALSE(factory.HasCreator(9999));
}

HWTEST_F(MinidumpStreamFactoryTest, FactoryTest002, TestSize.Level2)
{
    auto& factory = MinidumpStreamFactory::Instance();
    auto reader = MakeReader("");
    auto stream = factory.CreateStream(9999, reader);
    EXPECT_EQ(stream, nullptr);
}

HWTEST_F(MinidumpStreamFactoryTest, FactoryTest003, TestSize.Level2)
{
    auto& factory = MinidumpStreamFactory::Instance();
    auto reader = MakeReader("");
    uint32_t customType = 0xFF00;
    factory.RegisterCreator(customType, [](std::shared_ptr<MinidumpMemoryReader> mr) {
        return std::make_shared<MinidumpSystemInfo>(mr);
    });
    EXPECT_TRUE(factory.HasCreator(customType));
    auto stream = factory.CreateStream(customType, reader);
    EXPECT_NE(stream, nullptr);
}

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

HWTEST_F(MinidumpOptimizerTest, RangeMapTest001, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.IsEmpty());
    EXPECT_EQ(rm.GetRangeSize(), 0u);
    EXPECT_EQ(rm.GetHighestAddress(), 0u);
    EXPECT_FALSE(rm.StoreRange(0, 0, 1));
}

HWTEST_F(MinidumpOptimizerTest, RangeMapTest002, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_EQ(rm.GetRangeSize(), 1u);
    EXPECT_EQ(rm.GetHighestAddress(), 0x10FFu);
}

HWTEST_F(MinidumpOptimizerTest, RangeMapTest003, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_FALSE(rm.StoreRange(0x1050, 0x100, 1));
}

HWTEST_F(MinidumpOptimizerTest, RangeMapTest004, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_FALSE(rm.StoreRange(0xF00, 0x200, 1));
}

HWTEST_F(MinidumpOptimizerTest, RangeMapTest006, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    uint32_t value = 0;
    EXPECT_TRUE(rm.RetrieveRange(0x1050, &value));
    EXPECT_EQ(value, 0u);
    EXPECT_TRUE(rm.HasRange(0x1050));
}

HWTEST_F(MinidumpOptimizerTest, RangeMapTest007, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    EXPECT_FALSE(rm.RetrieveRange(0x900, nullptr));
    EXPECT_FALSE(rm.RetrieveRange(0x1200, nullptr));
    EXPECT_FALSE(rm.HasRange(0x900));
}

HWTEST_F(MinidumpOptimizerTest, RangeMapTest008, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rm;
    EXPECT_TRUE(rm.StoreRange(0x1000, 0x100, 0));
    rm.Clear();
    EXPECT_TRUE(rm.IsEmpty());
    EXPECT_EQ(rm.GetHighestAddress(), 0u);
}

HWTEST_F(MinidumpOptimizerTest, RangeMapTest009, TestSize.Level2)
{
    RangeMap<uint64_t, uint32_t> rangeMap;
    EXPECT_FALSE(rangeMap.StoreRange(static_cast<uint64_t>(-1) - 10, 20, 0));
    EXPECT_EQ(rangeMap.GetRangeSize(), 0u);
}

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest001, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Empty());
    EXPECT_EQ(tree.Size(), 0u);
    EXPECT_EQ(tree.GetMaxAddress(), 0u);
}

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest002, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_FALSE(tree.Insert(200, 100, 0));
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    EXPECT_EQ(tree.Size(), 1u);
}

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest003, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    EXPECT_TRUE(tree.Insert(100, 200, 0));
    EXPECT_FALSE(tree.Insert(100, 200, 1));
    EXPECT_EQ(tree.Size(), 1u);
}

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest004, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x1100, 0);
    uint32_t result = 0;
    EXPECT_TRUE(tree.Search(0x1050, &result));
    EXPECT_EQ(result, 0u);
}

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest005, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x1100, 0);
    uint32_t result = 0;
    EXPECT_FALSE(tree.Search(0x900, &result));
}

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest006, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x2000, 0);
    tree.Insert(0x3000, 0x4000, 1);
    auto results = tree.SearchRange(0x1500, 0x3500);
    EXPECT_EQ(results.size(), 2u);
}

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

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest008, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    uint32_t value = 0;
    EXPECT_FALSE(tree.Search(0x1000, &value));
    EXPECT_EQ(tree.Size(), 0u);
}

HWTEST_F(MinidumpOptimizerTest, IntervalTreeTest009, TestSize.Level2)
{
    IntervalTree<uint64_t, uint32_t> tree;
    tree.Insert(0x1000, 0x2000, 0);
    auto results = tree.SearchRange(0x5000, 0x6000);
    EXPECT_TRUE(results.empty());
}

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

HWTEST_F(MinidumpOptimizerTest, BitmapTest001, TestSize.Level2)
{
    BitmapIndex bm(0x10000, 4096);
    EXPECT_EQ(bm.Size(), 17u);
    bm.MarkRange(0, 4095);
    EXPECT_TRUE(bm.IsInRange(0));
    EXPECT_TRUE(bm.IsInRange(2000));
    EXPECT_FALSE(bm.IsInRange(4096));
}

HWTEST_F(MinidumpOptimizerTest, BitmapTest002, TestSize.Level2)
{
    BitmapIndex bm(0x10000, 4096);
    bm.MarkRange(0, 8191);
    EXPECT_EQ(bm.MarkedCount(), 2u);
    EXPECT_EQ(bm.FindNextInRange(4096), 4096u);
    EXPECT_EQ(bm.FindNextInRange(8192), UINT64_MAX);
}

HWTEST_F(MinidumpOptimizerTest, BitmapTest003, TestSize.Level2)
{
    BitmapIndex bm(0x10000, 4096);
    bm.MarkRange(0, 4095);
    bm.Clear();
    EXPECT_EQ(bm.MarkedCount(), 0u);
    EXPECT_FALSE(bm.IsInRange(0));
}

HWTEST_F(MinidumpOptimizerTest, BitmapTest004, TestSize.Level2)
{
    BitmapIndex bitmap(100, 0);
    EXPECT_FALSE(bitmap.IsInRange(50));
    EXPECT_EQ(bitmap.FindNextInRange(0), static_cast<uint64_t>(UINT64_MAX));
}

HWTEST_F(MinidumpOptimizerTest, PerfOptTest001, TestSize.Level2)
{
    auto config = PerformanceOptimizer::Instance().GetConfig();
    EXPECT_TRUE(config.enableIntervalTree);
    EXPECT_TRUE(config.enableBitmapIndex);
}

HWTEST_F(MinidumpOptimizerTest, PerfOptTest002, TestSize.Level2)
{
    PerformanceOptimizer::Config config;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = true;
    config.bitmapGranularity = 134217728;
    PerformanceOptimizer::Instance().SetConfig(config);
    EXPECT_TRUE(PerformanceOptimizer::Instance().GetConfig().enableBitmapIndex);
}

HWTEST_F(MinidumpOptimizerTest, PerfOptTest003, TestSize.Level2)
{
    PerformanceOptimizer::Instance().GetModuleIntervalTree().Insert(0x1000, 0x2000, 0);
    PerformanceOptimizer::Instance().GetMemoryIntervalTree().Insert(0x3000, 0x4000, 1);
    auto stats = PerformanceOptimizer::Instance().GetStatistics();
    EXPECT_EQ(stats.intervalTreeMemorySize, 1u);
    EXPECT_EQ(stats.bitmapMarkedCount, 0u);
}

HWTEST_F(MinidumpOptimizerTest, PerfOptTest004, TestSize.Level2)
{
    PerformanceOptimizer::Instance().GetModuleIntervalTree().Insert(0x1000, 0x2000, 0);
    PerformanceOptimizer::Instance().Reset();
    auto stats = PerformanceOptimizer::Instance().GetStatistics();
    EXPECT_EQ(stats.intervalTreeModuleSize, 0u);
    EXPECT_EQ(stats.bitmapMarkedCount, 0u);
}

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

HWTEST_F(MinidumpObserverTest, ObservableTest002, TestSize.Level2)
{
    MinidumpObservable observable;
    auto obs1 = std::make_shared<ProgressObserver>([](uint32_t, uint32_t, const std::string&) {});
    observable.AddObserver(obs1);
    observable.AddObserver(obs1);
    EXPECT_EQ(observable.GetObserverCount(), 1u);
}

HWTEST_F(MinidumpObserverTest, ObservableTest003, TestSize.Level2)
{
    MinidumpObservable observable;
    auto obs = std::make_shared<ProgressObserver>([](uint32_t, uint32_t, const std::string&) {});
    observable.RemoveObserver(obs);
    EXPECT_EQ(observable.GetObserverCount(), 0u);
}

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

HWTEST_F(MinidumpObserverTest, ObserverFilterTest001, TestSize.Level2)
{
    ProgressObserver obs([](uint32_t, uint32_t, const std::string&) {});
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_PROGRESS));
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_START));
    EXPECT_TRUE(obs.ShouldHandle(MinidumpEventType::PARSE_COMPLETE));
}

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

HWTEST_F(MinidumpObserverTest, ObserverFilterTest002, TestSize.Level2)
{
    auto observer = std::make_shared<StreamLoadObserver>(
        [](const std::string&, uint32_t) {});
    EXPECT_FALSE(observer->ShouldHandle(MinidumpEventType::PARSE_START));
    EXPECT_TRUE(observer->ShouldHandle(MinidumpEventType::STREAM_LOADED));
}

} // namespace HiviewDFX
} // namespace OHOS
