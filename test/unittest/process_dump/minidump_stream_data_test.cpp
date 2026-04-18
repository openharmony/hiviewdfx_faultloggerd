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

class MinidumpSystemInfoTest : public testing::Test {};
class MinidumpMiscInfoTest : public testing::Test {};
class MinidumpEsrInfoTest : public testing::Test {};
class MinidumpMapListTest : public testing::Test {};

HWTEST_F(MinidumpSystemInfoTest, SysInfoTest001, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    rawInfo.platformId = MINIDUMP_OS_LINUX;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_TRUE(sysInfo.Valid());
    EXPECT_NE(sysInfo.SystemInfo(), nullptr);
    EXPECT_EQ(sysInfo.GetOS(), "linux");
    EXPECT_EQ(sysInfo.GetCPU(), "arm64");
}

HWTEST_F(MinidumpSystemInfoTest, SysInfoTest002, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    rawInfo.platformId = MINIDUMP_OS_HONGMENG;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetOS(), "hongmeng");
}

HWTEST_F(MinidumpSystemInfoTest, SysInfoTest003, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = MD_CPU_ARCH_UNKNOWN;
    rawInfo.platformId = 0x9999;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetOS(), "unknown");
    EXPECT_EQ(sysInfo.GetCPU(), "unknown");
}

HWTEST_F(MinidumpSystemInfoTest, SysInfoTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_FALSE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetOS(), "");
    EXPECT_EQ(sysInfo.GetCPU(), "");
}

HWTEST_F(MinidumpSystemInfoTest, SysInfoTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_FALSE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_FALSE(sysInfo.Valid());
    sysInfo.Print();
}

HWTEST_F(MinidumpSystemInfoTest, SysInfoTest006, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    rawInfo.platformId = MINIDUMP_OS_LINUX;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_TRUE(sysInfo.Valid());
    sysInfo.Print();
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMiscInfo misc(reader);
    EXPECT_FALSE(misc.Valid());
    uint32_t pid = 0;
    EXPECT_FALSE(misc.ProcessId(pid));
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest002, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID;
    rawMisc.processId = 1234;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo misc(reader);
    EXPECT_TRUE(misc.Read(sizeof(MDRawMiscInfo)));
    EXPECT_TRUE(misc.Valid());

    uint32_t pid = 0;
    EXPECT_TRUE(misc.ProcessId(pid));
    EXPECT_EQ(pid, 1234u);
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest003, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = MD_MISCINFO_FLAGS1_PROCESS_TIMES;
    rawMisc.processCreateTime = 100;
    rawMisc.processUserTime = 200;
    rawMisc.processKernelTime = 300;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo misc(reader);
    EXPECT_TRUE(misc.Read(sizeof(MDRawMiscInfo)));

    uint32_t createTime = 0, userTime = 0, kernelTime = 0;
    EXPECT_TRUE(misc.ProcessTimes(createTime, userTime, kernelTime));
    EXPECT_EQ(createTime, 100u);
    EXPECT_EQ(userTime, 200u);
    EXPECT_EQ(kernelTime, 300u);
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest004, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = 0;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo misc(reader);
    EXPECT_TRUE(misc.Read(sizeof(MDRawMiscInfo)));

    uint32_t pid = 0;
    EXPECT_FALSE(misc.ProcessId(pid));
    uint32_t ct = 0, ut = 0, kt = 0;
    EXPECT_FALSE(misc.ProcessTimes(ct, ut, kt));
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMiscInfo misc(reader);
    EXPECT_FALSE(misc.Read(sizeof(MDRawMiscInfo)));
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest006, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID;
    rawMisc.processId = 999;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo misc(reader);
    EXPECT_TRUE(misc.Read(sizeof(MDRawMiscInfo)));
    EXPECT_TRUE(misc.Valid());
    misc.Print();
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMiscInfo misc(reader);
    EXPECT_FALSE(misc.Valid());
    misc.Print();
}

HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest008, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(uint32_t) + sizeof(uint32_t);
    rawMisc.flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID;
    rawMisc.processId = 999;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo misc(reader);
    EXPECT_TRUE(misc.Read(sizeof(MDRawMiscInfo)));
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Valid());
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest002, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID;
    rawEsr.dumpThreadId = 42;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_TRUE(esrInfo.Valid());
    EXPECT_EQ(esrInfo.EsrRegsInfo().validity, MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID);
    EXPECT_EQ(esrInfo.EsrRegsInfo().dumpThreadId, 42u);
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest003, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.esrRegs = 0x9600001;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_TRUE(esrInfo.Valid());
    EXPECT_EQ(esrInfo.EsrRegsInfo().validity, MD_BREAKPAD_INFO_VALID_ESR_REGS);
    EXPECT_EQ(esrInfo.EsrRegsInfo().esrRegs, 0x9600001u);
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest004, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.dumpThreadId = 100;
    rawEsr.esrRegs = 0x9600001;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_TRUE(esrInfo.Valid());
    EXPECT_EQ(esrInfo.EsrRegsInfo().validity,
        MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS);
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest005, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Read(sizeof(uint32_t)));
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest007, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.dumpThreadId = 42;
    rawEsr.esrRegs = 0x9600001;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    esrInfo.Print();
}

HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest008, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Valid());
    esrInfo.Print();
}

HWTEST_F(MinidumpMapListTest, MapListTest001, TestSize.Level2)
{
    std::string mapContent = "1000-2000 r-xp 00000000 00:00 0 test.so\n";
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
    EXPECT_EQ(mapList.GetMapsLength(), static_cast<uint32_t>(mapContent.size() + 1));
    auto contents = mapList.GetContents();
    EXPECT_FALSE(contents.empty());
}

HWTEST_F(MinidumpMapListTest, MapListTest002, TestSize.Level2)
{
    std::string mapContent = "1000-2000 r-xp test.so\n";
    mapContent += std::string(3, '\0');
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
    EXPECT_EQ(mapList.GetMapsLength(), static_cast<uint32_t>(mapContent.size() - 3 + 1));
}

HWTEST_F(MinidumpMapListTest, MapListTest003, TestSize.Level2)
{
    std::string mapContent = "1000-2000 r-xp test.so\n";
    mapContent += std::string(2, static_cast<char>(0xFF));
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
    EXPECT_EQ(mapList.GetMapsLength(), static_cast<uint32_t>(mapContent.size() - 2 + 1));
}

HWTEST_F(MinidumpMapListTest, MapListTest004, TestSize.Level2)
{
    std::string mapContent = "1000-2000 r-xp test.so\n";
    mapContent += std::string(2, static_cast<char>(0xFE));
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
    EXPECT_EQ(mapList.GetMapsLength(), static_cast<uint32_t>(mapContent.size() - 2 + 1));
}

HWTEST_F(MinidumpMapListTest, MapListTest005, TestSize.Level2)
{
    auto reader = MakeReader("test");
    MinidumpMapList mapList(reader);
    EXPECT_FALSE(mapList.Read(0));
}

HWTEST_F(MinidumpMapListTest, MapListTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMapList mapList(reader);
    EXPECT_FALSE(mapList.Valid());
    EXPECT_EQ(mapList.GetMapsLength(), 0u);
    EXPECT_TRUE(mapList.GetContents().empty());
    mapList.Print();
}

} // namespace HiviewDFX
} // namespace OHOS
