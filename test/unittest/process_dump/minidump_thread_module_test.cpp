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

class MinidumpThreadTest : public testing::Test {};
class MinidumpThreadNameTest : public testing::Test {};
class MinidumpThreadNameListTest : public testing::Test {};
class MinidumpModuleTest : public testing::Test {};
class MinidumpModuleListTest : public testing::Test {
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

std::string BuildUTF16LEString(const std::string& asciiStr)
{
    uint32_t byteLength = static_cast<uint32_t>(asciiStr.size() * 2);
    std::string result(reinterpret_cast<const char*>(&byteLength), sizeof(byteLength));
    for (size_t i = 0; i < asciiStr.size(); ++i) {
        uint16_t ch = static_cast<uint16_t>(static_cast<unsigned char>(asciiStr[i]));
        result += std::string(reinterpret_cast<const char*>(&ch), sizeof(ch));
    }
    return result;
}

/**
 * @tc.name: ThreadTest001
 * @tc.desc: test MinidumpThread Read and Valid with valid raw thread data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 100;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawThread.threadContext.rva = 100;
    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());
    EXPECT_TRUE(thread.Valid());

    uint32_t tid = 0;
    EXPECT_TRUE(thread.GetThreadID(tid));
    EXPECT_EQ(tid, 42u);
    EXPECT_EQ(thread.GetStartOfStackMemoryRange(), 0x8000u);
}

/**
 * @tc.name: ThreadTest002
 * @tc.desc: test MinidumpThread Read with zero memory size returns null memory
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadTest002, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 10;
    rawThread.stack.memory.rva = 0;
    rawThread.stack.memory.dataSize = 0;
    rawThread.stack.startOfMemoryRange = 0;
    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());
    EXPECT_EQ(thread.GetMemory(), nullptr);
}

/**
 * @tc.name: ThreadTest003
 * @tc.desc: test MinidumpThread Read with empty data returns false and default values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadTest003, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThread thread(reader);
    EXPECT_FALSE(thread.Read());
    uint32_t tid = 0;
    EXPECT_FALSE(thread.GetThreadID(tid));
    EXPECT_EQ(thread.GetStartOfStackMemoryRange(), 0u);
    EXPECT_EQ(thread.GetMemory(), nullptr);
    EXPECT_EQ(thread.GetContext(), nullptr);
}

/**
 * @tc.name: ThreadTest004
 * @tc.desc: test MinidumpThread Read with memory data returns valid memory and Print
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadTest004, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 99;
    rawThread.stack.startOfMemoryRange = 0x7000;
    rawThread.stack.memory.dataSize = 4;
    rawThread.stack.memory.rva = sizeof(MDRawThread);
    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    data += "ABCD";
    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());
    EXPECT_NE(thread.GetMemory(), nullptr);
    thread.Print();
}

/**
 * @tc.name: ThreadTest005
 * @tc.desc: test MinidumpThread Read with empty data returns false and invalid
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThread thread(reader);
    EXPECT_FALSE(thread.Read());
    EXPECT_FALSE(thread.Valid());
    thread.Print();
}

/**
 * @tc.name: ThreadListTest001
 * @tc.desc: test MinidumpThreadList with empty data returns invalid state with zero count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Valid());
    EXPECT_EQ(list.ThreadCount(), 0u);
    EXPECT_EQ(list.GetThreadAtIndex(0), nullptr);
    EXPECT_EQ(list.GetThreadByID(1), nullptr);
}

/**
 * @tc.name: ThreadListTest002
 * @tc.desc: test MinidumpThreadList Read with empty data and uint32_t offset returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: ThreadListTest003
 * @tc.desc: test MinidumpThreadList Read with zero count data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListTest003, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: ThreadListTest004
 * @tc.desc: test MinidumpThreadList Read with maxThreads config exceeding limit returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListTest004, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxThreads = 1;
    mgr.SetConfig(config);

    uint32_t count = 5;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + count * sizeof(MDRawThread)));
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: ThreadListTest005
 * @tc.desc: test MinidumpThreadList Print with invalid data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Valid());
    list.Print();
}

/**
 * @tc.name: ThreadListTest006
 * @tc.desc: test MinidumpThreadList Read with offset 2 returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(2));
}

/**
 * @tc.name: ThreadNameTest001
 * @tc.desc: test MinidumpThreadName with empty data returns invalid state and default values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadName tn(reader);
    EXPECT_FALSE(tn.Valid());
    EXPECT_FALSE(tn.Read());
    uint32_t tid = 0;
    EXPECT_FALSE(tn.GetThreadID(tid));
    EXPECT_EQ(tn.GetThreadName(), "");
}

/**
 * @tc.name: ThreadNameTest002
 * @tc.desc: test MinidumpThreadName Read succeeds but ReadAuxiliaryData fails with zero rva
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameTest002, TestSize.Level2)
{
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = 0;
    std::string data(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    auto reader = MakeReader(data);
    MinidumpThreadName tn(reader);
    EXPECT_TRUE(tn.Read());
    EXPECT_FALSE(tn.ReadAuxiliaryData());
}

/**
 * @tc.name: ThreadNameTest003
 * @tc.desc: test MinidumpThreadName ReadAuxiliaryData with empty data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameTest003, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadName tn(reader);
    EXPECT_FALSE(tn.ReadAuxiliaryData());
}

/**
 * @tc.name: ThreadNameTest004
 * @tc.desc: test MinidumpThreadName Print with invalid data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadName tn(reader);
    EXPECT_FALSE(tn.Valid());
    tn.Print();
}

/**
 * @tc.name: ThreadNameListTest001
 * @tc.desc: test MinidumpThreadNameList with empty data returns invalid state and default values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameListTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadNameList list(reader);
    EXPECT_FALSE(list.Valid());
    EXPECT_EQ(list.ThreadNameCount(), 0u);
    EXPECT_EQ(list.GetThreadNameAtIndex(0), nullptr);
    EXPECT_EQ(list.GetThreadNameById(1), "");
}

/**
 * @tc.name: ThreadNameListTest002
 * @tc.desc: test MinidumpThreadNameList Read with zero count returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameListTest002, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: ThreadNameListTest003
 * @tc.desc: test MinidumpThreadNameList Read with maxThreads config exceeding limit returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameListTest003, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxThreads = 1;
    mgr.SetConfig(config);

    uint32_t count = 5;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + count * sizeof(MDRawThreadName)));
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: ModuleTest001
 * @tc.desc: test MinidumpModule with empty data returns invalid state with default values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModule mod(reader);
    EXPECT_FALSE(mod.Valid());
    EXPECT_EQ(mod.BaseAddress(), static_cast<uint64_t>(-1));
    EXPECT_EQ(mod.Size(), 0u);
    EXPECT_EQ(mod.CodeFile(), "");
    EXPECT_EQ(mod.DebugFile(), "");
    EXPECT_EQ(mod.Version(), "");
    EXPECT_FALSE(mod.IsUnloaded());
}

/**
 * @tc.name: ModuleTest002
 * @tc.desc: test MinidumpModule Read with empty data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModule mod(reader);
    EXPECT_FALSE(mod.Read());
}

/**
 * @tc.name: ModuleTest003
 * @tc.desc: test MinidumpModule Read succeeds but BaseAddress returns invalid with zero moduleNameRva
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest003, TestSize.Level2)
{
    MDRawModule rawMod = {};
    rawMod.baseOfImage = 0x5000;
    rawMod.sizeOfImage = 0x1000;
    rawMod.checksum = 0xABCD;
    rawMod.timeDataStamp = 0x12345678;
    rawMod.moduleNameRva = 0;
    std::string data(reinterpret_cast<const char*>(&rawMod), sizeof(rawMod));
    auto reader = MakeReader(data);
    MinidumpModule mod(reader);
    EXPECT_TRUE(mod.Read());
    EXPECT_TRUE(mod.moduleValid_);
    EXPECT_EQ(mod.BaseAddress(), static_cast<uint64_t>(-1));
}

/**
 * @tc.name: ModuleTest004
 * @tc.desc: test MinidumpModule Read and ReadAuxiliaryData with valid data returns correct fields
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest004, TestSize.Level2)
{
    MDRawModule rawMod = {};
    rawMod.baseOfImage = 0x5000;
    rawMod.sizeOfImage = 0x1000;
    rawMod.checksum = 0xABCD;
    rawMod.timeDataStamp = 0x12345678;
    rawMod.moduleNameRva = sizeof(MDRawModule);
    std::string modData(reinterpret_cast<const char*>(&rawMod), sizeof(rawMod));

    uint32_t strLen = 8;
    std::string strData(sizeof(uint32_t) + strLen, '\0');
    (void)memcpy_s(&strData[0], strData.length(), &strLen, sizeof(strLen));
    strData[4] = 0x41;
    strData[5] = 0x00;
    strData[6] = 0x42;
    strData[7] = 0x00;
    strData[8] = 0x43;
    strData[9] = 0x00;
    strData[10] = 0x44;
    strData[11] = 0x00;

    std::string data = modData + strData;
    auto reader = MakeReader(data);
    MinidumpModule mod(reader);
    EXPECT_TRUE(mod.Read());
    EXPECT_TRUE(mod.ReadAuxiliaryData());
    EXPECT_TRUE(mod.Valid());
    EXPECT_EQ(mod.BaseAddress(), 0x5000u);
    EXPECT_EQ(mod.Size(), 0x1000u);
    EXPECT_EQ(mod.CodeFile(), "ABCD");
    EXPECT_TRUE(mod.CodeIdentifier().find("12345678") != std::string::npos);
    EXPECT_TRUE(mod.DebugIdentifier().find("123456780000abcd") != std::string::npos);
    EXPECT_EQ(mod.DebugFile(), "");
    EXPECT_EQ(mod.Version(), "");
}

/**
 * @tc.name: ModuleTest005
 * @tc.desc: test MinidumpModule ReadAuxiliaryData with empty data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModule mod(reader);
    EXPECT_FALSE(mod.ReadAuxiliaryData());
}

/**
 * @tc.name: ModuleTest006
 * @tc.desc: test MinidumpModule Read succeeds but ReadAuxiliaryData fails with zero rva
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest006, TestSize.Level2)
{
    MDRawModule rawMod = {};
    rawMod.baseOfImage = 0x5000;
    rawMod.sizeOfImage = 0x1000;
    rawMod.moduleNameRva = 0;
    std::string data(reinterpret_cast<const char*>(&rawMod), sizeof(rawMod));
    auto reader = MakeReader(data);
    MinidumpModule mod(reader);
    EXPECT_TRUE(mod.Read());
    EXPECT_FALSE(mod.ReadAuxiliaryData());
}

/**
 * @tc.name: ModuleTest007
 * @tc.desc: test MinidumpModule Read and ReadAuxiliaryData with short UTF16 string name
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest007, TestSize.Level2)
{
    MDRawModule rawMod = {};
    rawMod.baseOfImage = 0x5000;
    rawMod.sizeOfImage = 0x1000;
    rawMod.moduleNameRva = sizeof(MDRawModule);
    std::string modData(reinterpret_cast<const char*>(&rawMod), sizeof(rawMod));
    uint32_t strLen = 4;
    std::string strData(sizeof(uint32_t) + strLen, '\0');
    (void)memcpy_s(&strData[0], strData.length(), &strLen, sizeof(strLen));
    strData[4] = 0x54;
    strData[5] = 0x00;

    std::string data = modData + strData;
    auto reader = MakeReader(data);
    MinidumpModule mod(reader);
    mod.Read();
    mod.ReadAuxiliaryData();
    EXPECT_EQ(mod.BaseAddress(), 0x5000u);
    EXPECT_EQ(mod.CodeFile(), "T");
}

/**
 * @tc.name: ModuleTest008
 * @tc.desc: test MinidumpModule Read with empty data returns false and prints invalid module
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleTest008, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModule mod(reader);
    EXPECT_FALSE(mod.Read());
    EXPECT_FALSE(mod.Valid());
    mod.Print();
}

/**
 * @tc.name: ModuleListTest001
 * @tc.desc: test MinidumpModuleList with empty data returns invalid state and default values
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Valid());
    EXPECT_EQ(list.ModuleCount(), 0u);
    EXPECT_EQ(list.GetModuleAtIndex(0), nullptr);
    EXPECT_EQ(list.GetMainModule(), nullptr);
    EXPECT_EQ(list.GetModuleForAddress(0x5000), nullptr);
}

/**
 * @tc.name: ModuleListTest002
 * @tc.desc: test MinidumpModuleList Read with empty data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: ModuleListTest003
 * @tc.desc: test MinidumpModuleList Read with zero count returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest003, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: ModuleListTest004
 * @tc.desc: test MinidumpModuleList Read with maxModules config exceeding limit returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest004, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxModules = 1;
    mgr.SetConfig(config);

    uint32_t count = 5;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + count * sizeof(MDRawModule)));
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: ModuleListTest005
 * @tc.desc: test MinidumpModuleList Read with one valid module returns correct fields
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest005, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("AB");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("AB").size());
    EXPECT_TRUE(list.Read(expectedSize));
    EXPECT_TRUE(list.Valid());
    EXPECT_EQ(list.ModuleCount(), 1u);

    auto module = list.GetModuleAtIndex(0);
    EXPECT_NE(module, nullptr);
    EXPECT_EQ(module->BaseAddress(), 0x5000u);
    EXPECT_EQ(module->Size(), 0x1000u);
    EXPECT_EQ(module->CodeFile(), "AB");
}

/**
 * @tc.name: ModuleListTest006
 * @tc.desc: test MinidumpModuleList GetMainModule returns first module and out-of-range returns null
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest006, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test_mod");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("test_mod").size());
    EXPECT_TRUE(list.Read(expectedSize));

    EXPECT_EQ(list.GetModuleAtIndex(1), nullptr);
    EXPECT_NE(list.GetMainModule(), nullptr);
    EXPECT_EQ(list.GetMainModule()->BaseAddress(), 0x5000u);
}

/**
 * @tc.name: ModuleListTest007
 * @tc.desc: test MinidumpModuleList GetModuleForAddress finds module by address
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest007, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("mod");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("mod").size());
    EXPECT_TRUE(list.Read(expectedSize));

    auto foundModule = list.GetModuleForAddress(0x5000);
    EXPECT_NE(foundModule, nullptr);
    EXPECT_EQ(foundModule->BaseAddress(), 0x5000u);
}

/**
 * @tc.name: ModuleListTest008
 * @tc.desc: test MinidumpModuleList GetModuleForAddress returns null for non-matching address
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest008, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("mod");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("mod").size());
    EXPECT_TRUE(list.Read(expectedSize));

    EXPECT_EQ(list.GetModuleForAddress(0x100), nullptr);
}

/**
 * @tc.name: ModuleListTest009
 * @tc.desc: test MinidumpModuleList Print with invalid data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest009, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Valid());
    list.Print();
}

/**
 * @tc.name: ModuleListTest010
 * @tc.desc: test MinidumpModuleList Print with valid module list
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest010, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("AB");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("AB").size());
    EXPECT_TRUE(list.Read(expectedSize));
    list.Print();
}

/**
 * @tc.name: ModuleListTest011
 * @tc.desc: test MinidumpModuleList Read with invalid base address returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest011, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = static_cast<uint64_t>(-1);
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("AB");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("AB").size());
    EXPECT_FALSE(list.Read(expectedSize));
}

/**
 * @tc.name: ModuleListTest012
 * @tc.desc: test MinidumpModuleList Read with zero moduleNameRva returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListTest012, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = 0;

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + sizeof(MDRawModule)));
}

/**
 * @tc.name: ThreadListSubjectNotificationTest001
 * @tc.desc: test MinidumpThreadList notifies observer on successful Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListSubjectNotificationTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 100;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawThread.threadContext.rva = 100;

    uint32_t threadCount = 1;
    std::string data(reinterpret_cast<const char*>(&threadCount), sizeof(threadCount));
    data += std::string(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));

    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);

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

    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThread)));
    EXPECT_EQ(receivedName, "ThreadList");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: ThreadGetContextSeekFailureTest001
 * @tc.desc: test MinidumpThread GetContext returns null on stream seek failure
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadGetContextSeekFailureTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 99;
    rawThread.stack.startOfMemoryRange = 0x7000;
    rawThread.stack.memory.dataSize = 4;
    rawThread.stack.memory.rva = sizeof(MDRawThread);
    rawThread.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawThread.threadContext.rva = 0xFFFF;

    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    data += "ABCD";
    auto ss = std::make_shared<std::stringstream>(data, std::ios::binary | std::ios::in);
    auto reader = std::make_shared<MinidumpMemoryReader>(ss);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());

    ss->setstate(std::ios::badbit);
    auto ctx = thread.GetContext();
    EXPECT_EQ(ctx, nullptr);
}

/**
 * @tc.name: ModuleListSubjectNotificationTest001
 * @tc.desc: test MinidumpModuleList notifies observer on successful Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListSubjectNotificationTest001, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test_mod");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);

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

    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("test_mod").size());
    EXPECT_TRUE(list.Read(expectedSize));
    EXPECT_EQ(receivedName, "ModuleList");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: ModuleListAddressLookupDisabledTest001
 * @tc.desc: test MinidumpModuleList GetModuleForAddress returns null when all lookups disabled
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListAddressLookupDisabledTest001, TestSize.Level2)
{
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = false;
    config.enableBitmapIndex = false;
    config.bitmapGranularity = 0;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x5000;
    rawModule.sizeOfImage = 0x1000;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("mod");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    uint32_t expectedSize = sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("mod").size());
    EXPECT_TRUE(list.Read(expectedSize));
    EXPECT_EQ(list.GetModuleForAddress(0x5000), nullptr);

    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = 134217728;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: ThreadNameListSuccessfulReadTest001
 * @tc.desc: test MinidumpThreadNameList Read with valid data returns correct thread name
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameListSuccessfulReadTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(uint32_t) + sizeof(MDRawThreadName);

    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    data += std::string(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    data += BuildUTF16LEString("main_thread");

    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName) +
        static_cast<uint32_t>(BuildUTF16LEString("main_thread").size())));
    EXPECT_TRUE(list.Valid());
    EXPECT_EQ(list.ThreadNameCount(), 1u);
    EXPECT_EQ(list.GetThreadNameById(55), "main_thread");
}

/**
 * @tc.name: ThreadNameListSubjectNotificationTest001
 * @tc.desc: test MinidumpThreadNameList notifies observer on successful Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameListSubjectNotificationTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(uint32_t) + sizeof(MDRawThreadName);

    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    data += std::string(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    data += BuildUTF16LEString("test");

    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);

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

    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName) +
        static_cast<uint32_t>(BuildUTF16LEString("test").size())));
    EXPECT_EQ(receivedName, "ThreadNameList");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: ThreadStackOverflowTest001
 * @tc.desc: test MinidumpThread with stack overflow range returns null memory
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadStackOverflowTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = std::numeric_limits<uint64_t>::max() - 10;
    rawThread.stack.memory.dataSize = 100;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = 0;
    rawThread.threadContext.rva = 0;
    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());
    EXPECT_TRUE(thread.Valid());
    EXPECT_EQ(thread.GetMemory(), nullptr);
}

/**
 * @tc.name: ThreadGetContextCachedTest001
 * @tc.desc: test MinidumpThread GetContext returns cached context on repeated calls
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadGetContextCachedTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 100;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawThread.threadContext.rva = sizeof(MDRawThread);

    MDRawContextARM64 rawCtx = {};
    rawCtx.contextFlags = MD_CONTEXT_ARM64;
    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    data += std::string(reinterpret_cast<const char*>(&rawCtx), sizeof(rawCtx));

    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());

    auto ctx1 = thread.GetContext();
    EXPECT_NE(ctx1, nullptr);
    EXPECT_TRUE(ctx1->Valid());
    auto ctx2 = thread.GetContext();
    EXPECT_NE(ctx2, nullptr);
    EXPECT_EQ(ctx1, ctx2);
}

/**
 * @tc.name: ThreadContextReadFailureTest001
 * @tc.desc: test MinidumpThread GetContext returns null when context data unavailable
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadContextReadFailureTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 100;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = sizeof(MDRawContextARM64);
    rawThread.threadContext.rva = sizeof(MDRawThread);

    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());

    auto ctx = thread.GetContext();
    EXPECT_EQ(ctx, nullptr);
}

/**
 * @tc.name: ThreadPrintValidTest001
 * @tc.desc: test MinidumpThread Print with valid thread data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadPrintValidTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 100;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = 0;
    rawThread.threadContext.rva = 0;
    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());
    thread.Print();
}

/**
 * @tc.name: ThreadThreadAccessorTest001
 * @tc.desc: test MinidumpThread thread accessor returns raw thread fields
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadThreadAccessorTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 100;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = 0;
    rawThread.threadContext.rva = 0;
    std::string data(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    auto reader = MakeReader(data);
    MinidumpThread thread(reader);
    EXPECT_TRUE(thread.Read());
    EXPECT_EQ(thread.thread().threadId, 42u);
    EXPECT_EQ(thread.thread().stack.startOfMemoryRange, 0x8000u);
}

/**
 * @tc.name: ThreadNamePrintValidWithNameTest001
 * @tc.desc: test MinidumpThreadName Print with valid thread name and auxiliary data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNamePrintValidWithNameTest001, TestSize.Level2)
{
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(MDRawThreadName);
    std::string tnData(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    tnData += BuildUTF16LEString("test_thread");

    auto reader = MakeReader(tnData);
    MinidumpThreadName threadName(reader);
    EXPECT_TRUE(threadName.Read());
    EXPECT_TRUE(threadName.ReadAuxiliaryData());
    EXPECT_TRUE(threadName.Valid());
    threadName.Print();
}

/**
 * @tc.name: ThreadNameListGetThreadNameByIdNotFoundTest001
 * @tc.desc: test MinidumpThreadNameList GetThreadNameById returns empty for non-existing id
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameListTest, ThreadNameListGetThreadNameByIdNotFoundTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(uint32_t) + sizeof(MDRawThreadName);
    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    data += std::string(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    data += BuildUTF16LEString("test_thread");

    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName) +
        static_cast<uint32_t>(BuildUTF16LEString("test_thread").size())));

    std::string name = list.GetThreadNameById(99);
    EXPECT_TRUE(name.empty());
}

/**
 * @tc.name: ThreadNameListPrintInvalidTest001
 * @tc.desc: test MinidumpThreadNameList Print with invalid list
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameListTest, ThreadNameListPrintInvalidTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadNameList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
    list.Print();
}

/**
 * @tc.name: ThreadNameListPrintValidTest001
 * @tc.desc: test MinidumpThreadNameList Print with valid list
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameListTest, ThreadNameListPrintValidTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(uint32_t) + sizeof(MDRawThreadName);
    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    data += std::string(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    data += BuildUTF16LEString("test_thread");

    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName) +
        static_cast<uint32_t>(BuildUTF16LEString("test_thread").size())));
    list.Print();
}

/**
 * @tc.name: ThreadNameListGetThreadNamesTest001
 * @tc.desc: test MinidumpThreadNameList GetThreadNames returns correct size
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameListTest, ThreadNameListGetThreadNamesTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(uint32_t) + sizeof(MDRawThreadName);
    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    data += std::string(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    data += BuildUTF16LEString("test_thread");

    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName) +
        static_cast<uint32_t>(BuildUTF16LEString("test_thread").size())));
    EXPECT_EQ(list.GetThreadNames().size(), 1u);
}

/**
 * @tc.name: ThreadNameListReadWithoutSubjectTest001
 * @tc.desc: test MinidumpThreadNameList Read without subject returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameListTest, ThreadNameListReadWithoutSubjectTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(uint32_t) + sizeof(MDRawThreadName);
    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    data += std::string(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    data += BuildUTF16LEString("test_thread");

    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName) +
        static_cast<uint32_t>(BuildUTF16LEString("test_thread").size())));
    EXPECT_TRUE(list.Valid());
}

/**
 * @tc.name: ModuleAccessorTest001
 * @tc.desc: test MinidumpModule module accessor returns raw module fields
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleAccessorTest001, TestSize.Level2)
{
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x1000;
    rawModule.sizeOfImage = 100;
    rawModule.moduleNameRva = sizeof(MDRawModule);
    rawModule.timeDataStamp = 0x12345678;
    rawModule.checksum = 0xABCD;

    std::string data(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test.so");

    auto reader = MakeReader(data);
    MinidumpModule module(reader);
    EXPECT_TRUE(module.Read());
    EXPECT_TRUE(module.ReadAuxiliaryData());
    EXPECT_TRUE(module.Valid());
    EXPECT_NE(module.module(), nullptr);
    EXPECT_EQ(module.module()->baseOfImage, 0x1000u);
    EXPECT_EQ(module.module()->sizeOfImage, 100u);
}

/**
 * @tc.name: ModuleCodeIdentifierTest001
 * @tc.desc: test MinidumpModule CodeIdentifier and DebugIdentifier with valid data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleTest, ModuleCodeIdentifierTest001, TestSize.Level2)
{
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x1000;
    rawModule.sizeOfImage = 100;
    rawModule.moduleNameRva = sizeof(MDRawModule);
    rawModule.timeDataStamp = 0x12345678;
    rawModule.checksum = 0xABCD;

    std::string data(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test.so");

    auto reader = MakeReader(data);
    MinidumpModule module(reader);
    EXPECT_TRUE(module.Read());
    EXPECT_TRUE(module.ReadAuxiliaryData());
    EXPECT_TRUE(module.Valid());
    EXPECT_FALSE(module.CodeIdentifier().empty());
    EXPECT_FALSE(module.DebugIdentifier().empty());
    EXPECT_TRUE(module.DebugFile().empty());
    EXPECT_TRUE(module.Version().empty());
}

/**
 * @tc.name: ModuleListGetModulesTest001
 * @tc.desc: test MinidumpModuleList GetModules returns correct module count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListGetModulesTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = 134217728;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);

    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x1000;
    rawModule.sizeOfImage = 100;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);
    rawModule.timeDataStamp = 0;
    rawModule.checksum = 0;

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test.so");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("test.so").size())));
    EXPECT_EQ(list.GetModules().size(), 1u);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
}

/**
 * @tc.name: ModuleListReadModuleRawDataFailureTest001
 * @tc.desc: test MinidumpModuleList Read fails when module raw data unavailable
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListReadModuleRawDataFailureTest001, TestSize.Level2)
{
    uint32_t moduleCount = 1;
    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: ModuleListReadWithoutSubjectTest001
 * @tc.desc: test MinidumpModuleList Read without subject returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListReadWithoutSubjectTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = 134217728;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);

    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x1000;
    rawModule.sizeOfImage = 100;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test.so");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("test.so").size())));
    EXPECT_TRUE(list.Valid());

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
}

/**
 * @tc.name: ModuleListGetModuleForAddressBitmapNotInRangeTest001
 * @tc.desc: test MinidumpModuleList GetModuleForAddress returns null when address not in bitmap range
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListGetModuleForAddressBitmapNotInRangeTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = true;
    config.bitmapGranularity = 0x100;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x1000;
    rawModule.sizeOfImage = 100;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test.so");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("test.so").size())));

    auto module = list.GetModuleForAddress(0x500);
    EXPECT_EQ(module, nullptr);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = 134217728;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: ModuleListGetModuleForAddressTreeNotFoundTest001
 * @tc.desc: test MinidumpModuleList GetModuleForAddress returns null when not found in interval tree
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpModuleListTest, ModuleListGetModuleForAddressTreeNotFoundTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config config;
    config.enableRangeMap = false;
    config.enableIntervalTree = true;
    config.enableBitmapIndex = false;
    PerformanceOptimizer::Instance().SetConfig(config);

    uint32_t moduleCount = 1;
    MDRawModule rawModule = {};
    rawModule.baseOfImage = 0x1000;
    rawModule.sizeOfImage = 100;
    rawModule.moduleNameRva = sizeof(uint32_t) + sizeof(MDRawModule);

    std::string data(reinterpret_cast<const char*>(&moduleCount), sizeof(moduleCount));
    data += std::string(reinterpret_cast<const char*>(&rawModule), sizeof(rawModule));
    data += BuildUTF16LEString("test.so");

    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawModule) +
        static_cast<uint32_t>(BuildUTF16LEString("test.so").size())));

    auto module = list.GetModuleForAddress(0x5000);
    EXPECT_EQ(module, nullptr);

    mgr.SetConfig(MinidumpConfig());
    PerformanceOptimizer::Instance().Reset();
    PerformanceOptimizer::Config defaultConfig;
    defaultConfig.enableRangeMap = true;
    defaultConfig.enableIntervalTree = true;
    defaultConfig.enableBitmapIndex = true;
    defaultConfig.bitmapGranularity = 134217728;
    PerformanceOptimizer::Instance().SetConfig(defaultConfig);
}

/**
 * @tc.name: ThreadListReadThreadDataFailureTest001
 * @tc.desc: test MinidumpThreadList Read fails when thread raw data unavailable
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListReadThreadDataFailureTest001, TestSize.Level2)
{
    uint32_t count = 1;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + sizeof(MDRawThread)));
    EXPECT_TRUE(list.GetLastError().IsError());
}

/**
 * @tc.name: ThreadListDuplicateThreadIdTest001
 * @tc.desc: test MinidumpThreadList Read fails with duplicate thread IDs
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListDuplicateThreadIdTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 0;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = 0;
    rawThread.threadContext.rva = 0;

    uint32_t threadCount = 2;
    std::string data(reinterpret_cast<const char*>(&threadCount), sizeof(threadCount));
    MDRawThread rawThread2 = {};
    rawThread2.threadId = 42;
    rawThread2.stack.startOfMemoryRange = 0x7000;
    rawThread2.stack.memory.dataSize = 0;
    rawThread2.stack.memory.rva = 0;
    rawThread2.threadContext.dataSize = 0;
    rawThread2.threadContext.rva = 0;
    data += std::string(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));
    data += std::string(reinterpret_cast<const char*>(&rawThread2), sizeof(rawThread2));

    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + 2 * sizeof(MDRawThread)));
    EXPECT_EQ(list.GetLastError().GetError(), MinidumpError::ERROR_ALREADY_EXISTS);
}

/**
 * @tc.name: ThreadListGetThreadByIDValidTest001
 * @tc.desc: test MinidumpThreadList GetThreadByID returns correct thread
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListGetThreadByIDValidTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 0;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = 0;
    rawThread.threadContext.rva = 0;

    uint32_t threadCount = 1;
    std::string data(reinterpret_cast<const char*>(&threadCount), sizeof(threadCount));
    data += std::string(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));

    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThread)));
    EXPECT_TRUE(list.Valid());

    auto thread = list.GetThreadByID(42);
    EXPECT_NE(thread, nullptr);
    EXPECT_EQ(list.GetThreadByID(99), nullptr);
}

/**
 * @tc.name: ThreadListGetThreadAtIndexValidTest001
 * @tc.desc: test MinidumpThreadList GetThreadAtIndex returns correct thread with tid
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListGetThreadAtIndexValidTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 0;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = 0;
    rawThread.threadContext.rva = 0;

    uint32_t threadCount = 1;
    std::string data(reinterpret_cast<const char*>(&threadCount), sizeof(threadCount));
    data += std::string(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));

    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThread)));
    EXPECT_TRUE(list.Valid());

    auto thread = list.GetThreadAtIndex(0);
    EXPECT_NE(thread, nullptr);
    uint32_t tid = 0;
    EXPECT_TRUE(thread->GetThreadID(tid));
    EXPECT_EQ(tid, 42u);
}

/**
 * @tc.name: ThreadListPrintValidTest001
 * @tc.desc: test MinidumpThreadList Print with valid thread list
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadTest, ThreadListPrintValidTest001, TestSize.Level2)
{
    MDRawThread rawThread = {};
    rawThread.threadId = 42;
    rawThread.stack.startOfMemoryRange = 0x8000;
    rawThread.stack.memory.dataSize = 0;
    rawThread.stack.memory.rva = 0;
    rawThread.threadContext.dataSize = 0;
    rawThread.threadContext.rva = 0;

    uint32_t threadCount = 1;
    std::string data(reinterpret_cast<const char*>(&threadCount), sizeof(threadCount));
    data += std::string(reinterpret_cast<const char*>(&rawThread), sizeof(rawThread));

    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThread)));
    EXPECT_TRUE(list.Valid());
    list.Print();
}

/**
 * @tc.name: ThreadNameListGetThreadNameAtIndexValidTest001
 * @tc.desc: test MinidumpThreadNameList GetThreadNameAtIndex returns valid entry
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameListGetThreadNameAtIndexValidTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = sizeof(uint32_t) + sizeof(MDRawThreadName);
    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    data += std::string(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    data += BuildUTF16LEString("main_thread");

    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_TRUE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName) +
        static_cast<uint32_t>(BuildUTF16LEString("main_thread").size())));
    EXPECT_TRUE(list.Valid());
    EXPECT_NE(list.GetThreadNameAtIndex(0), nullptr);
    EXPECT_EQ(list.GetThreadNameAtIndex(1), nullptr);
}

/**
 * @tc.name: ThreadNameListReadFailureTest001
 * @tc.desc: test MinidumpThreadNameList Read fails when thread name raw data unavailable
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameListReadFailureTest001, TestSize.Level2)
{
    uint32_t threadNameCount = 1;
    std::string data(reinterpret_cast<const char*>(&threadNameCount), sizeof(threadNameCount));
    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t) + sizeof(MDRawThreadName)));
    EXPECT_TRUE(list.GetLastError().IsError());
}

/**
 * @tc.name: ThreadNameReadAuxiliaryDataFailureTest001
 * @tc.desc: test MinidumpThreadName ReadAuxiliaryData fails with zero rva
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpThreadNameTest, ThreadNameReadAuxiliaryDataFailureTest001, TestSize.Level2)
{
    MDRawThreadName rawTN = {};
    rawTN.threadId = 55;
    rawTN.rvaOfThreadName = 0;
    std::string data(reinterpret_cast<const char*>(&rawTN), sizeof(rawTN));
    auto reader = MakeReader(data);
    MinidumpThreadName tn(reader);
    EXPECT_TRUE(tn.Read());
    EXPECT_FALSE(tn.ReadAuxiliaryData());
}

} // namespace HiviewDFX
} // namespace OHOS
