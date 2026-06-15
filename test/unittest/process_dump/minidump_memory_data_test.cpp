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

constexpr uint64_t TEST_MEMORY_REGION_BASE = 0x1000;
constexpr uint32_t DEFAULT_BITMAP_GRANULARITY = 128 * 1024 * 1024;
constexpr uint32_t TEST_BITMAP_GRANULARITY_SMALL = 0x100;
constexpr uint32_t TEST_SMALL_MEMORY_LIMIT = 2;
constexpr uint32_t TEST_SINGLE_REGION_LIMIT = 1;

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpMemoryRegionTest : public testing::Test {};
class MinidumpMemoryListTest : public testing::Test {
public:
    void SetUp() override
    {
        auto& mgr = MinidumpConfigManager::Instance();
        mgr.SetConfig(MinidumpConfig());
        PerformanceOptimizer::Instance().Reset();
    }
    void TearDown() override
    {
        auto& mgr = MinidumpConfigManager::Instance();
        mgr.SetConfig(MinidumpConfig());
        PerformanceOptimizer::Instance().Reset();
    }
};

/**
 * @tc.name: MemRegionTest001
 * @tc.desc: test MinidumpMemoryRegion with empty reader returns invalid region with default base and zero size
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryRegion region(reader);
    EXPECT_FALSE(region.Valid());
    EXPECT_EQ(region.GetBase(), static_cast<uint64_t>(-1));
    EXPECT_EQ(region.GetSize(), 0u);
}

/**
 * @tc.name: MemRegionTest002
 * @tc.desc: test MinidumpMemoryRegion with descriptor returns valid region with correct base and size
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest002, TestSize.Level2)
{
    auto reader = MakeReader("ABCDEF");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());
    EXPECT_EQ(region.GetBase(), TEST_MEMORY_REGION_BASE);
    EXPECT_EQ(region.GetSize(), 6u);
}

/**
 * @tc.name: MemRegionTest003
 * @tc.desc: test MinidumpMemoryRegion GetMemory and GetMemoryAtAddress with uint8 and uint32 values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest003, TestSize.Level2)
{
    uint8_t buf[sizeof(uint32_t)] = {'A', 0, 0, 0};
    std::string memData(reinterpret_cast<const char*>(buf), sizeof(uint32_t));
    auto reader = MakeReader(memData);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = sizeof(uint32_t);
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    auto mem = region.GetMemory();
    EXPECT_NE(mem, nullptr);
    EXPECT_EQ(mem->size(), sizeof(uint32_t));
    EXPECT_EQ(mem->at(0), 'A');
    uint8_t val8 = 0;
    EXPECT_TRUE(region.GetMemoryAtAddress(0x1000, val8));
    EXPECT_EQ(val8, 'A');
    uint32_t val32 = 0;
    EXPECT_TRUE(region.GetMemoryAtAddress(0x1000, val32));
    uint32_t expected32 = 0;
    (void)memcpy_s(&expected32, sizeof(uint32_t), buf, sizeof(uint32_t));
    EXPECT_EQ(val32, expected32);
}

/**
 * @tc.name: MemRegionTest004
 * @tc.desc: test MinidumpMemoryRegion with empty reader returns null memory and fails GetMemoryAtAddress
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryRegion region(reader);
    EXPECT_EQ(region.GetMemory(), nullptr);
    uint8_t val = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0, val));
}

/**
 * @tc.name: MemRegionTest005
 * @tc.desc: test MinidumpMemoryRegion FreeMemory preserves descriptor after memory is freed
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest005, TestSize.Level2)
{
    auto reader = MakeReader("ABCD");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    region.GetMemory();
    region.FreeMemory();
    EXPECT_EQ(region.GetDescriptor().startOfMemoryRange, TEST_MEMORY_REGION_BASE);
}

/**
 * @tc.name: MemRegionTest006
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with uint64 value exceeding region size returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest006, TestSize.Level2)
{
    auto reader = MakeReader("ABC");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 3;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    uint64_t val64 = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0x1000, val64));
}

/**
 * @tc.name: MemRegionTest007
 * @tc.desc: test MinidumpMemoryRegion Print on invalid region
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryRegion region(reader);
    EXPECT_FALSE(region.Valid());
    region.Print();
}

/**
 * @tc.name: MemRegionTest008
 * @tc.desc: test MinidumpMemoryRegion Print on valid region with descriptor
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest008, TestSize.Level2)
{
    std::string memData = "ABCD";
    auto reader = MakeReader(memData);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());
    EXPECT_NE(region.GetMemory(), nullptr);
    region.Print();
}

/**
 * @tc.name: MemRegionTest009
 * @tc.desc: test MinidumpMemoryRegion with maxMemoryBytes config limit returns null memory
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest009, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxMemoryBytes = TEST_SMALL_MEMORY_LIMIT;
    mgr.SetConfig(config);

    auto reader = MakeReader("ABCDEF");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_EQ(region.GetMemory(), nullptr);
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: MemListTest001
 * @tc.desc: test MinidumpMemoryList with empty reader returns invalid list with zero regions
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Valid());
    EXPECT_EQ(list.RegionCount(), 0u);
    EXPECT_EQ(list.GetMemoryRegionAtIndex(0), nullptr);
    EXPECT_EQ(list.GetMemoryRegionForAddress(0x1000), nullptr);
}

/**
 * @tc.name: MemListTest002
 * @tc.desc: test MinidumpMemoryList Read with empty reader returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: MemListTest003
 * @tc.desc: test MinidumpMemoryList Read with zero region count returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest003, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: MemListTest004
 * @tc.desc: test MinidumpMemoryList Read exceeds maxMemoryRegions config limit returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest004, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxMemoryRegions = TEST_SINGLE_REGION_LIMIT;
    mgr.SetConfig(config);

    uint32_t count = 5;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + count * sizeof(MDMemoryDescriptor)));
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: MemListTest005
 * @tc.desc: test MinidumpMemoryList Read with valid single region returns valid list with correct region
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest005, TestSize.Level2)
{
    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCDEF";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 6));
    EXPECT_TRUE(list.Valid());
    EXPECT_EQ(list.RegionCount(), 1u);

    auto region = list.GetMemoryRegionAtIndex(0);
    EXPECT_NE(region, nullptr);
    EXPECT_EQ(region->GetBase(), TEST_MEMORY_REGION_BASE);
    EXPECT_EQ(region->GetSize(), 6u);
}

/**
 * @tc.name: MemListTest006
 * @tc.desc: test MinidumpMemoryList GetMemoryRegionAtIndex with out of bounds index returns null
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest006, TestSize.Level2)
{
    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCDEF";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 6));
    EXPECT_EQ(list.GetMemoryRegionAtIndex(1), nullptr);
}

/**
 * @tc.name: MemListTest007
 * @tc.desc: test MinidumpMemoryList Print on invalid list
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Valid());
    list.Print();
}

/**
 * @tc.name: MemListTest008
 * @tc.desc: test MinidumpMemoryList Print on valid list with single region
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListTest008, TestSize.Level2)
{
    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCDEF";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 6));
    list.Print();
}

/**
 * @tc.name: MemRegionOverflowTest001
 * @tc.desc: test MinidumpMemoryRegion with overflow base address returns invalid region with null memory
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionOverflowTest001, TestSize.Level2)
{
    auto reader = MakeReader("ABCD");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = static_cast<uint64_t>(-1) - 3;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_FALSE(region.Valid());
    EXPECT_EQ(region.GetBase(), static_cast<uint64_t>(-1));
    EXPECT_EQ(region.GetSize(), 0u);
    EXPECT_EQ(region.GetMemory(), nullptr);
}

/**
 * @tc.name: MemRegionSeekFailureTest001
 * @tc.desc: test MinidumpMemoryRegion with seek failure returns null memory and ERROR_FILE_SEEK error
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionSeekFailureTest001, TestSize.Level2)
{
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    auto ss = std::make_shared<std::stringstream>("ABCD", std::ios::binary | std::ios::in);
    auto reader = std::make_shared<MinidumpMemoryReader>(ss);
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());
    ss->setstate(std::ios::badbit);
    EXPECT_EQ(region.GetMemory(), nullptr);
    EXPECT_EQ(region.GetLastError().GetError(), MinidumpError::ERROR_FILE_SEEK);
}

/**
 * @tc.name: MemRegionReadFailureTest001
 * @tc.desc: test MinidumpMemoryRegion with dataSize larger than available data
 *           returns null memory and ERROR_MEMORY_ALLOC
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionReadFailureTest001, TestSize.Level2)
{
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 100;
    desc.memory.rva = 0;
    auto reader = MakeReader("AB");
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());
    EXPECT_EQ(region.GetMemory(), nullptr);
    EXPECT_EQ(region.GetLastError().GetError(), MinidumpError::ERROR_MEMORY_ALLOC);
}

/**
 * @tc.name: MemRegionAddrBelowBaseTest001
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with address below base returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionAddrBelowBaseTest001, TestSize.Level2)
{
    auto reader = MakeReader("ABCD");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    region.GetMemory();

    uint8_t val = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0x500, val));
}

/**
 * @tc.name: MemRegionOffsetTooLargeTest001
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with offset too large for type returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionOffsetTooLargeTest001, TestSize.Level2)
{
    auto reader = MakeReader("ABCD");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    region.GetMemory();

    uint16_t val = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0x1003, val));
}

/**
 * @tc.name: MemListSubjectNotificationTest001
 * @tc.desc: test MinidumpMemoryList notifies subject observer after Read completes with correct name and count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListSubjectNotificationTest001, TestSize.Level2)
{
    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCDEF";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);

    auto subject = std::make_shared<MinidumpSubject>();
    std::string receivedName;
    uint32_t receivedCount = 0;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& name, uint32_t count) {
            receivedName = name;
            receivedCount = count;
        });
    subject->AddObserver(obs);
    list.SetMinidumpSubject(subject);

    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 6));
    EXPECT_EQ(receivedName, "MemoryList");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: MemListAddressLookupDisabledTest001
 * @tc.desc: test MinidumpMemoryList GetMemoryRegionForAddress returns null when all lookup methods disabled
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListAddressLookupDisabledTest001, TestSize.Level2)
{
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = false;
    config.enableBitmapIndex = false;
    config.bitmapGranularity = 0;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCDEF";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 6));
    EXPECT_EQ(list.GetMemoryRegionForAddress(0x1000), nullptr);

    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: MemListRegionCountExceedsMaxTest001
 * @tc.desc: test MinidumpMemoryList Read returns false when region count exceeds maxMemoryRegions with zero limit
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListRegionCountExceedsMaxTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxMemoryRegions = 0;
    mgr.SetConfig(config);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 6;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCDEF";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 6));
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: MemListRegionCountZeroTest001
 * @tc.desc: test MinidumpMemoryList Read returns false with zero region count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListRegionCountZeroTest001, TestSize.Level2)
{
    uint32_t regionCount = 0;
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: MemListGetMemoryCachedTest001
 * @tc.desc: test MinidumpMemoryRegion GetMemory returns same cached pointer on repeated calls
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListGetMemoryCachedTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCD";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 4));

    auto region = list.GetMemoryRegionAtIndex(0);
    EXPECT_NE(region, nullptr);
    auto mem1 = region->GetMemory();
    EXPECT_NE(mem1, nullptr);
    auto mem2 = region->GetMemory();
    EXPECT_NE(mem2, nullptr);
    EXPECT_EQ(mem1, mem2);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
}

/**
 * @tc.name: MemListReadBytesDescriptorFailureTest001
 * @tc.desc: test MinidumpMemoryList Read returns false when descriptor bytes are unavailable
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListReadBytesDescriptorFailureTest001, TestSize.Level2)
{
    uint32_t regionCount = 1;
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: MemListGetMemoryRegionsTest001
 * @tc.desc: test MinidumpMemoryList GetMemoryRegions returns correct region count with two regions
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListGetMemoryRegionsTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);

    uint32_t regionCount = 2;
    MDMemoryDescriptor desc1 = {};
    desc1.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc1.memory.dataSize = 4;
    desc1.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor) * 2;
    MDMemoryDescriptor desc2 = {};
    desc2.startOfMemoryRange = 0x2000;
    desc2.memory.dataSize = 4;
    desc2.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor) * 2 + 4;

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc1), sizeof(desc1));
    data += std::string(reinterpret_cast<const char*>(&desc2), sizeof(desc2));
    data += "ABCD";
    data += "EFGH";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) * 2 + 8));
    EXPECT_EQ(list.GetMemoryRegions().size(), 2u);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
}

/**
 * @tc.name: MemListReadWithoutSubjectTest001
 * @tc.desc: test MinidumpMemoryList Read succeeds without subject attached
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListReadWithoutSubjectTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCD";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 4));
    EXPECT_TRUE(list.Valid());

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
}

/**
 * @tc.name: MemListGetMemoryRegionForAddressBitmapNotInRangeTest001
 * @tc.desc: test MinidumpMemoryList GetMemoryRegionForAddress returns null when bitmap indicates address not in range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListGetMemoryRegionForAddressBitmapNotInRangeTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = true;
    config.bitmapGranularity = TEST_BITMAP_GRANULARITY_SMALL;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCD";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 4));

    auto region = list.GetMemoryRegionForAddress(0x500);
    EXPECT_EQ(region, nullptr);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: MemListGetMemoryRegionForAddressTreeNotFoundTest001
 * @tc.desc: test MinidumpMemoryList GetMemoryRegionForAddress returns null when interval tree lookup fails
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListGetMemoryRegionForAddressTreeNotFoundTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = false;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCD";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 4));

    auto region = list.GetMemoryRegionForAddress(0x5000);
    EXPECT_EQ(region, nullptr);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: MemListBuildMemoryRegionsOnlyRangeMapTest001
 * @tc.desc: test MinidumpMemoryList GetMemoryRegionForAddress with range map only config returns correct region
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListBuildMemoryRegionsOnlyRangeMapTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = true;
    config.enableIntervalTree = false;
    config.enableBitmapIndex = false;
    config.bitmapGranularity = 0;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCD";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 4));

    auto region = list.GetMemoryRegionForAddress(0x1002);
    EXPECT_NE(region, nullptr);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: MemListPrintValidTest001
 * @tc.desc: test MinidumpMemoryList Print on valid list with default config
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListPrintValidTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);

    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor);
    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc), sizeof(desc));
    data += "ABCD";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) + 4));
    list.Print();

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
}

/**
 * @tc.name: MemRegionFreeMemoryTest001
 * @tc.desc: test MinidumpMemoryRegion FreeMemory on valid region with descriptor
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionFreeMemoryTest001, TestSize.Level2)
{
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;

    std::string data = "ABCD";
    auto reader = MakeReader(data);
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());

    auto mem = region.GetMemory();
    region.FreeMemory();
}

/**
 * @tc.name: MemRegionGetMemoryAtAddressUint8Test001
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with uint8 and uint32 types returns correct values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionGetMemoryAtAddressUint8Test001, TestSize.Level2)
{
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 8;
    desc.memory.rva = 0;

    std::string data = "\x01\x02\x03\x04\x05\x06\x07\x08";
    auto reader = MakeReader(data);
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());

    auto mem = region.GetMemory();
    EXPECT_NE(mem, nullptr);

    uint8_t val8 = 0;
    EXPECT_TRUE(region.GetMemoryAtAddress(0x1000, val8));
    EXPECT_EQ(val8, 0x01);

    uint32_t val32 = 0;
    EXPECT_TRUE(region.GetMemoryAtAddress(0x1000, val32));
    EXPECT_EQ(val32, 0x04030201u);
}

/**
 * @tc.name: MemRegionGetMemoryAtAddressUint16Test001
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with uint16 type returns correct value
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionGetMemoryAtAddressUint16Test001, TestSize.Level2)
{
    uint8_t buf[sizeof(uint16_t)] = {0x01, 0x02};
    std::string memData(reinterpret_cast<const char*>(buf), sizeof(uint16_t));
    auto reader = MakeReader(memData);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = sizeof(uint16_t);
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());

    auto mem = region.GetMemory();
    EXPECT_NE(mem, nullptr);

    uint16_t val16 = 0;
    EXPECT_TRUE(region.GetMemoryAtAddress(0x1000, val16));
    uint16_t expected16 = 0;
    (void)memcpy_s(&expected16, sizeof(uint16_t), buf, sizeof(uint16_t));
    EXPECT_EQ(val16, expected16);
}

/**
 * @tc.name: MemRegionGetMemoryAtAddressUint64Test001
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with uint64 type returns correct value
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionGetMemoryAtAddressUint64Test001, TestSize.Level2)
{
    uint8_t buf[sizeof(uint64_t)] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::string memData(reinterpret_cast<const char*>(buf), sizeof(uint64_t));
    auto reader = MakeReader(memData);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = sizeof(uint64_t);
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());

    auto mem = region.GetMemory();
    EXPECT_NE(mem, nullptr);

    uint64_t val64 = 0;
    EXPECT_TRUE(region.GetMemoryAtAddress(0x1000, val64));
    uint64_t expected64 = 0;
    (void)memcpy_s(&expected64, sizeof(uint64_t), buf, sizeof(uint64_t));
    EXPECT_EQ(val64, expected64);
}

/**
 * @tc.name: MemRegionGetMemoryAtAddressUint16OutOfBoundsTest001
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with uint16 out of bounds returns false and zero value
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionGetMemoryAtAddressUint16OutOfBoundsTest001, TestSize.Level2)
{
    std::string data = "\x01\x02\x03";
    auto reader = MakeReader(data);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 3;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    region.GetMemory();

    uint16_t val16 = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0x1002, val16));
    EXPECT_EQ(val16, 0u);
}

/**
 * @tc.name: MemRegionGetMemoryAtAddressUint64OutOfBoundsTest001
 * @tc.desc: test MinidumpMemoryRegion GetMemoryAtAddress with uint64 out of bounds returns false and zero value
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryRegionTest, MemRegionGetMemoryAtAddressUint64OutOfBoundsTest001, TestSize.Level2)
{
    std::string data = "\x01\x02\x03\x04\x05\x06\x07";
    auto reader = MakeReader(data);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc.memory.dataSize = 7;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    region.GetMemory();

    uint64_t val64 = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0x1000, val64));
    EXPECT_EQ(val64, 0u);
}

/**
 * @tc.name: MemListOverlapRangeTest001
 * @tc.desc: test MinidumpMemoryList with overlapping ranges using interval tree returns correct region count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMemoryListTest, MemListOverlapRangeTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = false;
    config.bitmapGranularity = 0;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t regionCount = 2;
    MDMemoryDescriptor desc1 = {};
    desc1.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc1.memory.dataSize = 4;
    desc1.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor) * 2;
    MDMemoryDescriptor desc2 = {};
    desc2.startOfMemoryRange = TEST_MEMORY_REGION_BASE;
    desc2.memory.dataSize = 4;
    desc2.memory.rva = sizeof(uint32_t) + sizeof(MDMemoryDescriptor) * 2 + 4;

    std::string data(reinterpret_cast<const char*>(&regionCount), sizeof(regionCount));
    data += std::string(reinterpret_cast<const char*>(&desc1), sizeof(desc1));
    data += std::string(reinterpret_cast<const char*>(&desc2), sizeof(desc2));
    data += "ABCD";
    data += "EFGH";

    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDMemoryDescriptor) * 2 + 8));
    EXPECT_EQ(list.RegionCount(), 2u);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = DEFAULT_BITMAP_GRANULARITY;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

} // namespace HiviewDFX
} // namespace OHOS
