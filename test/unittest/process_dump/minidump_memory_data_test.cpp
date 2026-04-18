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

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryRegion region(reader);
    EXPECT_FALSE(region.Valid());
    EXPECT_EQ(region.GetBase(), static_cast<uint64_t>(-1));
    EXPECT_EQ(region.GetSize(), 0u);
}

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest002, TestSize.Level2)
{
    auto reader = MakeReader("ABCDEF");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
    desc.memory.dataSize = 6;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());
    EXPECT_EQ(region.GetBase(), 0x1000u);
    EXPECT_EQ(region.GetSize(), 6u);
}

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest003, TestSize.Level2)
{
    uint8_t buf[sizeof(uint32_t)] = {'A', 0, 0, 0};
    std::string memData(reinterpret_cast<const char*>(buf), sizeof(uint32_t));
    auto reader = MakeReader(memData);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
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

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryRegion region(reader);
    EXPECT_EQ(region.GetMemory(), nullptr);
    uint8_t val = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0, val));
}

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest005, TestSize.Level2)
{
    auto reader = MakeReader("ABCD");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    region.GetMemory();
    region.FreeMemory();
    EXPECT_EQ(region.GetDescriptor().startOfMemoryRange, 0x1000u);
}

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest006, TestSize.Level2)
{
    auto reader = MakeReader("ABC");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
    desc.memory.dataSize = 3;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    uint64_t val64 = 0;
    EXPECT_FALSE(region.GetMemoryAtAddress(0x1000, val64));
}

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryRegion region(reader);
    EXPECT_FALSE(region.Valid());
    region.Print();
}

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest008, TestSize.Level2)
{
    std::string memData = "ABCD";
    auto reader = MakeReader(memData);
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
    desc.memory.dataSize = 4;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_TRUE(region.Valid());
    EXPECT_NE(region.GetMemory(), nullptr);
    region.Print();
}

HWTEST_F(MinidumpMemoryRegionTest, MemRegionTest009, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxMemoryBytes = 2;
    mgr.SetConfig(config);

    auto reader = MakeReader("ABCDEF");
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
    desc.memory.dataSize = 6;
    desc.memory.rva = 0;
    MinidumpMemoryRegion region(reader);
    region.SetDescriptor(desc);
    EXPECT_EQ(region.GetMemory(), nullptr);
    mgr.SetConfig(MinidumpConfig());
}

HWTEST_F(MinidumpMemoryListTest, MemListTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Valid());
    EXPECT_EQ(list.RegionCount(), 0u);
    EXPECT_EQ(list.GetMemoryRegionAtIndex(0), nullptr);
    EXPECT_EQ(list.GetMemoryRegionForAddress(0x1000), nullptr);
}

HWTEST_F(MinidumpMemoryListTest, MemListTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

HWTEST_F(MinidumpMemoryListTest, MemListTest003, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

HWTEST_F(MinidumpMemoryListTest, MemListTest004, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxMemoryRegions = 1;
    mgr.SetConfig(config);

    uint32_t count = 5;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + count * sizeof(MDMemoryDescriptor)));
    mgr.SetConfig(MinidumpConfig());
}

HWTEST_F(MinidumpMemoryListTest, MemListTest005, TestSize.Level2)
{
    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
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
    EXPECT_EQ(region->GetBase(), 0x1000u);
    EXPECT_EQ(region->GetSize(), 6u);
}

HWTEST_F(MinidumpMemoryListTest, MemListTest006, TestSize.Level2)
{
    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
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

HWTEST_F(MinidumpMemoryListTest, MemListTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMemoryList list(reader);
    EXPECT_FALSE(list.Valid());
    list.Print();
}

HWTEST_F(MinidumpMemoryListTest, MemListTest008, TestSize.Level2)
{
    uint32_t regionCount = 1;
    MDMemoryDescriptor desc = {};
    desc.startOfMemoryRange = 0x1000;
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

} // namespace HiviewDFX
} // namespace OHOS
