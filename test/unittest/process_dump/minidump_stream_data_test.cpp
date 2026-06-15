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

constexpr uint32_t TEST_UNKNOWN_PLATFORM_ID = 0x9999;
constexpr uint32_t TEST_ESR_INSTRUCTION_ABORT = 0x9600001;
constexpr uint32_t TEST_ESR_DATA_ABORT_LOWER_EL = 0x92000000;
constexpr uint32_t TEST_OLD_ARM64_ARCH_CODE = 0x8003;
constexpr uint32_t TEST_SMALL_MEMORY_BYTE_LIMIT = 10;

namespace OHOS {
namespace HiviewDFX {
using namespace testing::ext;
using namespace std;

class MinidumpSystemInfoTest : public testing::Test {};
class MinidumpMiscInfoTest : public testing::Test {};
class MinidumpEsrInfoTest : public testing::Test {};
class MinidumpMapListTest : public testing::Test {};

/**
 * @tc.name: SysInfoTest001
 * @tc.desc: test MinidumpSystemInfo Read with ARM64 Linux platform returns correct OS and CPU
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: SysInfoTest002
 * @tc.desc: test MinidumpSystemInfo Read with ARM64 HongMeng platform returns hongmeng OS
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: SysInfoTest003
 * @tc.desc: test MinidumpSystemInfo Read with unknown architecture and unknown platform returns unknown OS and CPU
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpSystemInfoTest, SysInfoTest003, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = MD_CPU_ARCH_UNKNOWN;
    rawInfo.platformId = TEST_UNKNOWN_PLATFORM_ID;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetOS(), "unknown");
    EXPECT_EQ(sysInfo.GetCPU(), "unknown");
}

/**
 * @tc.name: SysInfoTest004
 * @tc.desc: test MinidumpSystemInfo Read with empty data returns false and empty OS and CPU strings
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpSystemInfoTest, SysInfoTest004, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_FALSE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetOS(), "");
    EXPECT_EQ(sysInfo.GetCPU(), "");
}

/**
 * @tc.name: SysInfoTest005
 * @tc.desc: test MinidumpSystemInfo Read with empty data returns false and invalid state with Print
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpSystemInfoTest, SysInfoTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_FALSE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_FALSE(sysInfo.Valid());
    sysInfo.Print();
}

/**
 * @tc.name: SysInfoTest006
 * @tc.desc: test MinidumpSystemInfo Read and Print with valid ARM64 Linux system info
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MiscInfoTest001
 * @tc.desc: test MinidumpMiscInfo with empty data returns invalid state and ProcessId fails
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMiscInfo misc(reader);
    EXPECT_FALSE(misc.Valid());
    uint32_t pid = 0;
    EXPECT_FALSE(misc.ProcessId(pid));
}

/**
 * @tc.name: MiscInfoTest002
 * @tc.desc: test MinidumpMiscInfo Read with processId flag returns correct process ID
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MiscInfoTest003
 * @tc.desc: test MinidumpMiscInfo Read with process times flag returns correct process times
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MiscInfoTest004
 * @tc.desc: test MinidumpMiscInfo Read with no flags returns false for ProcessId and ProcessTimes
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MiscInfoTest005
 * @tc.desc: test MinidumpMiscInfo Read with empty data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest005, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMiscInfo misc(reader);
    EXPECT_FALSE(misc.Read(sizeof(MDRawMiscInfo)));
}

/**
 * @tc.name: MiscInfoTest006
 * @tc.desc: test MinidumpMiscInfo Read and Print with valid processId flag
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MiscInfoTest007
 * @tc.desc: test MinidumpMiscInfo Print with invalid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoTest007, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMiscInfo misc(reader);
    EXPECT_FALSE(misc.Valid());
    misc.Print();
}

/**
 * @tc.name: MiscInfoTest008
 * @tc.desc: test MinidumpMiscInfo Read with truncated sizeOfInfo succeeds
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: EsrInfoTest001
 * @tc.desc: test MinidumpEsrInfo with empty data returns invalid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Valid());
}

/**
 * @tc.name: EsrInfoTest002
 * @tc.desc: test MinidumpEsrInfo Read with dump thread ID validity flag returns correct values
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: EsrInfoTest003
 * @tc.desc: test MinidumpEsrInfo Read with ESR regs validity flag returns correct ESR regs value
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest003, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.esrRegs = TEST_ESR_INSTRUCTION_ABORT;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_TRUE(esrInfo.Valid());
    EXPECT_EQ(esrInfo.EsrRegsInfo().validity, MD_BREAKPAD_INFO_VALID_ESR_REGS);
    EXPECT_EQ(esrInfo.EsrRegsInfo().esrRegs, static_cast<uint32_t>(TEST_ESR_INSTRUCTION_ABORT));
}

/**
 * @tc.name: EsrInfoTest004
 * @tc.desc: test MinidumpEsrInfo Read with combined validity flags returns correct validity
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest004, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.dumpThreadId = 100;
    rawEsr.esrRegs = TEST_ESR_INSTRUCTION_ABORT;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_TRUE(esrInfo.Valid());
    EXPECT_EQ(esrInfo.EsrRegsInfo().validity,
        MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS);
}

/**
 * @tc.name: EsrInfoTest005
 * @tc.desc: test MinidumpEsrInfo Read with insufficient data size returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest005, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Read(sizeof(uint32_t)));
}

/**
 * @tc.name: EsrInfoTest006
 * @tc.desc: test MinidumpEsrInfo Read with empty data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
}

/**
 * @tc.name: EsrInfoTest007
 * @tc.desc: test MinidumpEsrInfo Read and Print with both validity flags
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest007, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.dumpThreadId = 42;
    rawEsr.esrRegs = TEST_ESR_INSTRUCTION_ABORT;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    esrInfo.Print();
}

/**
 * @tc.name: EsrInfoTest008
 * @tc.desc: test MinidumpEsrInfo Print with invalid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoTest008, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_FALSE(esrInfo.Valid());
    esrInfo.Print();
}

/**
 * @tc.name: MapListTest001
 * @tc.desc: test MinidumpMapList Read with valid map content returns valid state and correct maps length
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MapListTest002
 * @tc.desc: test MinidumpMapList Read with null bytes in content handles trimming correctly
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MapListTest003
 * @tc.desc: test MinidumpMapList Read with 0xFF bytes in content handles trimming correctly
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MapListTest004
 * @tc.desc: test MinidumpMapList Read with 0xFE bytes in content handles trimming correctly
 * @tc.type: FUNC
 */
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

/**
 * @tc.name: MapListTest005
 * @tc.desc: test MinidumpMapList Read with zero size returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListTest005, TestSize.Level2)
{
    auto reader = MakeReader("test");
    MinidumpMapList mapList(reader);
    EXPECT_FALSE(mapList.Read(0));
}

/**
 * @tc.name: MapListTest006
 * @tc.desc: test MinidumpMapList with empty data returns invalid state and zero maps length
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListTest006, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMapList mapList(reader);
    EXPECT_FALSE(mapList.Valid());
    EXPECT_EQ(mapList.GetMapsLength(), 0u);
    EXPECT_TRUE(mapList.GetContents().empty());
    mapList.Print();
}

/**
 * @tc.name: SysInfoSubjectNotificationTest001
 * @tc.desc: test MinidumpSystemInfo notifies observer with SystemInfo name after Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpSystemInfoTest, SysInfoSubjectNotificationTest001, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    rawInfo.platformId = MINIDUMP_OS_LINUX;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);

    auto subject = std::make_shared<MinidumpSubject>();
    std::string receivedName;
    uint32_t receivedCount = 0;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& name, uint32_t count) {
            receivedName = name;
            receivedCount = count;
        });
    subject->AddObserver(obs);
    sysInfo.SetMinidumpSubject(subject);

    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(receivedName, "SystemInfo");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: SysInfoUnknownPlatformTest001
 * @tc.desc: test MinidumpSystemInfo with unknown platform and unknown architecture returns unknown strings
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpSystemInfoTest, SysInfoUnknownPlatformTest001, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = MD_CPU_ARCH_UNKNOWN;
    rawInfo.platformId = 0x1234;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetOS(), "unknown");
    EXPECT_EQ(sysInfo.GetCPU(), "unknown");
}

/**
 * @tc.name: MiscInfoSubjectNotificationTest001
 * @tc.desc: test MinidumpMiscInfo notifies observer with MiscInfo name after Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoSubjectNotificationTest001, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID;
    rawMisc.processId = 999;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo misc(reader);

    auto subject = std::make_shared<MinidumpSubject>();
    std::string receivedName;
    uint32_t receivedCount = 0;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& name, uint32_t count) {
            receivedName = name;
            receivedCount = count;
        });
    subject->AddObserver(obs);
    misc.SetMinidumpSubject(subject);

    EXPECT_TRUE(misc.Read(sizeof(MDRawMiscInfo)));
    EXPECT_EQ(receivedName, "MiscInfo");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: MiscInfoProcessIdWithoutFlagTest001
 * @tc.desc: test MinidumpMiscInfo ProcessId without processId flag returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoProcessIdWithoutFlagTest001, TestSize.Level2)
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
}

/**
 * @tc.name: MiscInfoProcessTimesWithoutFlagTest001
 * @tc.desc: test MinidumpMiscInfo ProcessTimes without process times flag returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoProcessTimesWithoutFlagTest001, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = 0;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo misc(reader);
    EXPECT_TRUE(misc.Read(sizeof(MDRawMiscInfo)));
    uint32_t createTime = 0, userTime = 0, kernelTime = 0;
    EXPECT_FALSE(misc.ProcessTimes(createTime, userTime, kernelTime));
}

/**
 * @tc.name: EsrInfoSubjectNotificationTest001
 * @tc.desc: test MinidumpEsrInfo notifies observer with EsrInfo name after Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoSubjectNotificationTest001, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID;
    rawEsr.dumpThreadId = 42;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);

    auto subject = std::make_shared<MinidumpSubject>();
    std::string receivedName;
    uint32_t receivedCount = 0;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& name, uint32_t count) {
            receivedName = name;
            receivedCount = count;
        });
    subject->AddObserver(obs);
    esrInfo.SetMinidumpSubject(subject);

    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_EQ(receivedName, "EsrInfo");
    EXPECT_EQ(receivedCount, 1u);
}

/**
 * @tc.name: EsrInfoPrintWithValidityFlagsTest001
 * @tc.desc: test MinidumpEsrInfo Print with both validity flags
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoPrintWithValidityFlagsTest001, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.dumpThreadId = 42;
    rawEsr.esrRegs = TEST_ESR_DATA_ABORT_LOWER_EL;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    esrInfo.Print();
}

/**
 * @tc.name: MapListSubjectNotificationTest001
 * @tc.desc: test MinidumpMapList notifies observer with MapList name after Read
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListSubjectNotificationTest001, TestSize.Level2)
{
    std::string mapContent = "1000-2000 r-xp 00000000 00:00 0 test.so\n";
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);

    auto subject = std::make_shared<MinidumpSubject>();
    std::string receivedName;
    uint32_t receivedCount = 0;
    auto obs = std::make_shared<StreamLoadObserver>(
        [&](const std::string& name, uint32_t count) {
            receivedName = name;
            receivedCount = count;
        });
    subject->AddObserver(obs);
    mapList.SetMinidumpSubject(subject);

    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_EQ(receivedName, "MapList");
    EXPECT_TRUE(receivedCount > 0);
}

/**
 * @tc.name: MapListSizeExceedsMaxTest001
 * @tc.desc: test MinidumpMapList Read with size exceeding max memory limit returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListSizeExceedsMaxTest001, TestSize.Level2)
{
    auto& mgr = MinidumpConfigManager::Instance();
    MinidumpConfig config;
    config.maxMemoryBytes = TEST_SMALL_MEMORY_BYTE_LIMIT;
    mgr.SetConfig(config);

    std::string mapContent = "1000-2000 r-xp 00000000 00:00 0 test.so\n";
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_FALSE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    mgr.SetConfig(MinidumpConfig());
}

/**
 * @tc.name: MapListSizeZeroTest001
 * @tc.desc: test MinidumpMapList Read with zero size returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListSizeZeroTest001, TestSize.Level2)
{
    auto reader = MakeReader("");
    MinidumpMapList mapList(reader);
    EXPECT_FALSE(mapList.Read(0));
}

/**
 * @tc.name: MapListTailTrimmingTest001
 * @tc.desc: test MinidumpMapList Read with mixed trailing bytes handles trimming correctly
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListTailTrimmingTest001, TestSize.Level2)
{
    std::string mapContent = "1000-2000 r-xp 00000000 00:00 0 test.so\n";
    mapContent += static_cast<char>(0xFF);
    mapContent += static_cast<char>(0xFE);
    mapContent += static_cast<char>(0x00);
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
    EXPECT_TRUE(mapList.GetContents().size() > 0);
}

/**
 * @tc.name: EsrInfoReadWithoutSubjectTest001
 * @tc.desc: test MinidumpEsrInfo Read without subject returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoReadWithoutSubjectTest001, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.dumpThreadId = 123;
    rawEsr.esrRegs = TEST_ESR_DATA_ABORT_LOWER_EL;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_TRUE(esrInfo.Valid());
}

/**
 * @tc.name: EsrInfoPrintValidityZeroTest001
 * @tc.desc: test MinidumpEsrInfo Print with zero validity flag
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoPrintValidityZeroTest001, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = 0;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    EXPECT_TRUE(esrInfo.Valid());
    esrInfo.Print();
}

/**
 * @tc.name: EsrInfoPrintOnlyDumpThreadIdTest001
 * @tc.desc: test MinidumpEsrInfo Print with only dump thread ID validity flag
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoPrintOnlyDumpThreadIdTest001, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID;
    rawEsr.dumpThreadId = 999;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    esrInfo.Print();
}

/**
 * @tc.name: EsrInfoPrintOnlyEsrRegsTest001
 * @tc.desc: test MinidumpEsrInfo Print with only ESR regs validity flag
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpEsrInfoTest, EsrInfoPrintOnlyEsrRegsTest001, TestSize.Level2)
{
    MDRawEsrRegsInfo rawEsr = {};
    rawEsr.validity = MD_BREAKPAD_INFO_VALID_ESR_REGS;
    rawEsr.esrRegs = TEST_ESR_DATA_ABORT_LOWER_EL;
    std::string data(reinterpret_cast<const char*>(&rawEsr), sizeof(rawEsr));
    auto reader = MakeReader(data);
    MinidumpEsrInfo esrInfo(reader);
    EXPECT_TRUE(esrInfo.Read(sizeof(MDRawEsrRegsInfo)));
    esrInfo.Print();
}

/**
 * @tc.name: MiscInfoReadWithoutSubjectTest001
 * @tc.desc: test MinidumpMiscInfo Read without subject returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoReadWithoutSubjectTest001, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID | MD_MISCINFO_FLAGS1_PROCESS_TIMES;
    rawMisc.processId = 100;
    rawMisc.processCreateTime = 1000;
    rawMisc.processUserTime = 500;
    rawMisc.processKernelTime = 300;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo miscInfo(reader);
    EXPECT_TRUE(miscInfo.Read(sizeof(MDRawMiscInfo)));
    EXPECT_TRUE(miscInfo.Valid());
}

/**
 * @tc.name: MiscInfoSizeOfInfoZeroTest001
 * @tc.desc: test MinidumpMiscInfo Read with zero sizeOfInfo returns valid but ProcessId fails
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoSizeOfInfoZeroTest001, TestSize.Level2)
{
    uint32_t sizeOfInfo = 0;
    MDRawMiscInfo rawMisc = {};
    std::string data(reinterpret_cast<const char*>(&sizeOfInfo), sizeof(sizeOfInfo));
    data += std::string(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo miscInfo(reader);
    EXPECT_TRUE(miscInfo.Read(sizeof(sizeOfInfo) + sizeof(rawMisc)));
    EXPECT_TRUE(miscInfo.Valid());
    uint32_t pid = 0;
    EXPECT_FALSE(miscInfo.ProcessId(pid));
}

/**
 * @tc.name: MiscInfoSecondReadBytesFailureTest001
 * @tc.desc: test MinidumpMiscInfo Read with insufficient data for second read returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoSecondReadBytesFailureTest001, TestSize.Level2)
{
    uint32_t sizeOfInfo = sizeof(MDRawMiscInfo);
    std::string data(reinterpret_cast<const char*>(&sizeOfInfo), sizeof(sizeOfInfo));
    auto reader = MakeReader(data);
    MinidumpMiscInfo miscInfo(reader);
    EXPECT_FALSE(miscInfo.Read(sizeof(sizeOfInfo) + sizeof(MDRawMiscInfo)));
}

/**
 * @tc.name: MiscInfoPrintValidTest001
 * @tc.desc: test MinidumpMiscInfo Print with valid processId data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMiscInfoTest, MiscInfoPrintValidTest001, TestSize.Level2)
{
    MDRawMiscInfo rawMisc = {};
    rawMisc.sizeOfInfo = sizeof(MDRawMiscInfo);
    rawMisc.flags1 = MD_MISCINFO_FLAGS1_PROCESS_ID;
    rawMisc.processId = 42;
    std::string data(reinterpret_cast<const char*>(&rawMisc), sizeof(rawMisc));
    auto reader = MakeReader(data);
    MinidumpMiscInfo miscInfo(reader);
    EXPECT_TRUE(miscInfo.Read(sizeof(MDRawMiscInfo)));
    miscInfo.Print();
}

/**
 * @tc.name: SysInfoReadWithoutSubjectTest001
 * @tc.desc: test MinidumpSystemInfo Read without subject returns correct OS and CPU
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpSystemInfoTest, SysInfoReadWithoutSubjectTest001, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.platformId = MINIDUMP_OS_LINUX;
    rawInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetOS(), "linux");
    EXPECT_EQ(sysInfo.GetCPU(), "arm64");
}

/**
 * @tc.name: SysInfoGetCPUOldARM64Test001
 * @tc.desc: test MinidumpSystemInfo with old ARM64 architecture code returns unknown CPU
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpSystemInfoTest, SysInfoGetCPUOldARM64Test001, TestSize.Level2)
{
    MDRawSystemInfo rawInfo = {};
    rawInfo.processorArchitecture = TEST_OLD_ARM64_ARCH_CODE;
    std::string data(reinterpret_cast<const char*>(&rawInfo), sizeof(rawInfo));
    auto reader = MakeReader(data);
    MinidumpSystemInfo sysInfo(reader);
    EXPECT_TRUE(sysInfo.Read(sizeof(MDRawSystemInfo)));
    EXPECT_EQ(sysInfo.GetCPU(), "unknown");
}

/**
 * @tc.name: MapListReadBytesFailureTest001
 * @tc.desc: test MinidumpMapList Read with bad stream returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListReadBytesFailureTest001, TestSize.Level2)
{
    std::string data;
    data.resize(100, 'A');
    auto ss = std::make_shared<std::stringstream>(data, std::ios::binary | std::ios::in);
    MinidumpMemoryReader reader(ss);
    ss->setstate(std::ios::badbit);
    MinidumpMapList mapList(std::make_shared<MinidumpMemoryReader>(ss));
    EXPECT_FALSE(mapList.Read(50));
}

/**
 * @tc.name: MapListAllNullBytesTest001
 * @tc.desc: test MinidumpMapList Read with all null bytes returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListAllNullBytesTest001, TestSize.Level2)
{
    std::string mapContent(10, '\0');
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
    EXPECT_TRUE(mapList.GetContents().size() > 0);
}

/**
 * @tc.name: MapListAllFFBytesTest001
 * @tc.desc: test MinidumpMapList Read with all 0xFF bytes returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListAllFFBytesTest001, TestSize.Level2)
{
    std::string mapContent(10, static_cast<char>(0xFF));
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
}

/**
 * @tc.name: MapListAllFEBytesTest001
 * @tc.desc: test MinidumpMapList Read with all 0xFE bytes returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListAllFEBytesTest001, TestSize.Level2)
{
    std::string mapContent(10, static_cast<char>(0xFE));
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
}

/**
 * @tc.name: MapListPrintValidTest001
 * @tc.desc: test MinidumpMapList Print with valid map content
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListPrintValidTest001, TestSize.Level2)
{
    std::string mapContent = "1000-2000 r-xp 00000000 test.so\n";
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
    mapList.Print();
}

/**
 * @tc.name: MapListReadWithoutSubjectTest001
 * @tc.desc: test MinidumpMapList Read without subject returns valid state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpMapListTest, MapListReadWithoutSubjectTest001, TestSize.Level2)
{
    std::string mapContent = "maps content";
    auto reader = MakeReader(mapContent);
    MinidumpMapList mapList(reader);
    EXPECT_TRUE(mapList.Read(static_cast<uint32_t>(mapContent.size())));
    EXPECT_TRUE(mapList.Valid());
}

} // namespace HiviewDFX
} // namespace OHOS
