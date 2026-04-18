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

HWTEST_F(MinidumpThreadTest, ThreadTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThread thread(reader);
    EXPECT_FALSE(thread.Read());
    EXPECT_FALSE(thread.Valid());
    thread.Print();
}

HWTEST_F(MinidumpThreadTest, ThreadListTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Valid());
    EXPECT_EQ(list.ThreadCount(), 0u);
    EXPECT_EQ(list.GetThreadAtIndex(0), nullptr);
    EXPECT_EQ(list.GetThreadByID(1), nullptr);
}

HWTEST_F(MinidumpThreadTest, ThreadListTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

HWTEST_F(MinidumpThreadTest, ThreadListTest003, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

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

HWTEST_F(MinidumpThreadTest, ThreadListTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Valid());
    list.Print();
}

HWTEST_F(MinidumpThreadTest, ThreadListTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadList list(reader);
    EXPECT_FALSE(list.Read(2));
}

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

HWTEST_F(MinidumpThreadNameTest, ThreadNameTest003, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadName tn(reader);
    EXPECT_FALSE(tn.ReadAuxiliaryData());
}

HWTEST_F(MinidumpThreadNameTest, ThreadNameTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadName tn(reader);
    EXPECT_FALSE(tn.Valid());
    tn.Print();
}

HWTEST_F(MinidumpThreadNameTest, ThreadNameListTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpThreadNameList list(reader);
    EXPECT_FALSE(list.Valid());
    EXPECT_EQ(list.ThreadNameCount(), 0u);
    EXPECT_EQ(list.GetThreadNameAtIndex(0), nullptr);
    EXPECT_EQ(list.GetThreadNameById(1), "");
}

HWTEST_F(MinidumpThreadNameTest, ThreadNameListTest002, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpThreadNameList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

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

HWTEST_F(MinidumpModuleTest, ModuleTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModule mod(reader);
    EXPECT_FALSE(mod.Read());
}

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

HWTEST_F(MinidumpModuleTest, ModuleTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModule mod(reader);
    EXPECT_FALSE(mod.ReadAuxiliaryData());
}

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

HWTEST_F(MinidumpModuleTest, ModuleTest008, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModule mod(reader);
    EXPECT_FALSE(mod.Read());
    EXPECT_FALSE(mod.Valid());
    mod.Print();
}

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

HWTEST_F(MinidumpModuleListTest, ModuleListTest002, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

HWTEST_F(MinidumpModuleListTest, ModuleListTest003, TestSize.Level2)
{
    uint32_t count = 0;
    std::string data(reinterpret_cast<const char*>(&count), sizeof(count));
    auto reader = MakeReader(data);
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Read(sizeof(uint32_t)));
}

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

HWTEST_F(MinidumpModuleListTest, ModuleListTest009, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpModuleList list(reader);
    EXPECT_FALSE(list.Valid());
    list.Print();
}

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

} // namespace HiviewDFX
} // namespace OHOS
