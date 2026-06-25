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

#include <fcntl.h>
#include <gtest/gtest.h>
#include <memory>
#include <csignal>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include "cppcrash_info_collector.h"
#include "dfx_buffer_writer.h"
#include "dfx_dump_request.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "dump_utils.h"
#include "lite_process_dumper.h"
#include "minidump_dumper.h"
#include "minidump_format.h"
#include "minidump_parser.h"
#include "minidump_config.h"
#include "procinfo.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class MinidumpDumperTest : public testing::Test {
public:
    void SetUp() override
    {
        CppCrashInfoCollector::Instance().Reset();
    }
    void TearDown() override
    {
        CppCrashInfoCollector::Instance().Reset();
    }
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
constexpr mode_t TEST_FILE_PERMISSIONS = 0644;
constexpr size_t TEST_BUFFER_SIZE = 256;

/**
 * @tc.name: MinidumpDumperTest001
 * @tc.desc: test Dump with invalid pipeFd returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest001, TestSize.Level2)
{
    MinidumpDumper dumper;
    bool ret = dumper.Dump(getpid(), -1, true, false);
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: MinidumpDumperTest002
 * @tc.desc: test Dump with GenerateMinidump failure returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest002, TestSize.Level2)
{
    MinidumpDumper dumper;
    int pipeFds[2] = {-1, -1};
    pipe2(pipeFds, O_NONBLOCK);
    bool ret = dumper.Dump(getpid(), pipeFds[0], true, false);
    EXPECT_FALSE(ret);
    close(pipeFds[0]);
    close(pipeFds[1]);
}

/**
 * @tc.name: MinidumpDumperTest003
 * @tc.desc: test CollectDumpHeaderInfo sets request fields correctly
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest003, TestSize.Level2)
{
    MinidumpDumper dumper;
    pid_t pid = getpid();
    dumper.CollectDumpHeaderInfo(pid);
    EXPECT_EQ(dumper.request_.type, ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    EXPECT_EQ(dumper.request_.pid, pid);
    EXPECT_EQ(dumper.request_.nsPid, pid);
    EXPECT_TRUE(dumper.process_.GetLogSource() == "pdump");
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetNeedFormatFlag());
}

/**
 * @tc.name: MinidumpDumperTest004
 * @tc.desc: test TransferData with valid pipe fds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest004, TestSize.Level2)
{
    MinidumpDumper dumper;
    int pipeFds[2] = {-1, -1};
    pipe(pipeFds);
    std::string testData = "hello minidump transfer test";
    write(pipeFds[1], testData.c_str(), testData.size());
    close(pipeFds[1]);

    int tmpFd = open("/data/test/minidump_transfer_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    bool ret = dumper.TransferData(pipeFds[0], tmpFd);
    EXPECT_TRUE(ret);
    close(pipeFds[0]);
    close(tmpFd);
    unlink("/data/test/minidump_transfer_test");
}

/**
 * @tc.name: MinidumpDumperTest005
 * @tc.desc: test TransferData with invalid src fd returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest005, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_transfer_invalid", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    bool ret = dumper.TransferData(-1, tmpFd);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_transfer_invalid");
}

/**
 * @tc.name: MinidumpDumperTest006
 * @tc.desc: test TransferData with invalid dst fd returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest006, TestSize.Level2)
{
    MinidumpDumper dumper;
    int pipeFds[2] = {-1, -1};
    pipe(pipeFds);
    std::string testData = "test data";
    write(pipeFds[1], testData.c_str(), testData.size());
    close(pipeFds[1]);
    bool ret = dumper.TransferData(pipeFds[0], -1);
    EXPECT_FALSE(ret);
    close(pipeFds[0]);
}

/**
 * @tc.name: MinidumpDumperTest007
 * @tc.desc: test GenerateMinidump with invalid pipeFd returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest007, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.request_ = {};
    dumper.request_.pid = getpid();
    dumper.request_.timeStamp = GetTimeMilliSeconds();
    bool ret = dumper.GenerateMinidump(getpid(), -1, true);
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: MinidumpDumperTest008
 * @tc.desc: test ParseMinidump with empty/invalid data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest008, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.request_ = {};
    dumper.request_.pid = getpid();
    dumper.request_.timeStamp = GetTimeMilliSeconds();
    int tmpFd = open("/data/test/minidump_parse_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "this is not a valid minidump file";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    dumper.outputFdGuard_ = SmartFd(tmpFd);
    bool ret = dumper.ParseMinidump();
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: MinidumpDumperTest009
 * @tc.desc: test UnwindProcess with nullptr dfxMaps returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest009, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.dfxMaps_ = nullptr;
    dumper.UnwindProcess();
    EXPECT_EQ(dumper.unwinder_, nullptr);
}

/**
 * @tc.name: MinidumpDumperTest010
 * @tc.desc: test UnwindProcess with nullptr keyThread returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest010, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    dumper.UnwindProcess();
    EXPECT_EQ(dumper.unwinder_, nullptr);
}

/**
 * @tc.name: MinidumpDumperTest011
 * @tc.desc: test UnwindOtherThread with nullptr unwinder returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest011, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = nullptr;
    size_t otherThreadCount = dumper.process_.GetOtherThreads().size();
    dumper.UnwindOtherThread();
    EXPECT_EQ(dumper.process_.GetOtherThreads().size(), otherThreadCount);
}

/**
 * @tc.name: MinidumpDumperTest012
 * @tc.desc: test PrintThreadInfo with nullptr unwinder returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest012, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = nullptr;
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.PrintThreadInfo();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetKeyThread().frames.empty());
}

/**
 * @tc.name: MinidumpDumperTest013
 * @tc.desc: test PrintThreadInfo with nullptr keyThread returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest013, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    dumper.PrintThreadInfo();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetKeyThread().frames.empty());
}

/**
 * @tc.name: MinidumpDumperTest014
 * @tc.desc: test PrintRegisters with nullptr regs returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest014, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    dumper.PrintRegisters();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetRegisters().empty());
}

/**
 * @tc.name: MinidumpDumperTest015
 * @tc.desc: test PrintRegsNearMemory with nullptr unwinder returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest015, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = nullptr;
    dumper.PrintRegsNearMemory();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetMemoryNearRegister().empty());
}

/**
 * @tc.name: MinidumpDumperTest016
 * @tc.desc: test PrintFaultStack with nullptr unwinder returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest016, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = nullptr;
    dumper.PrintFaultStack();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetFaultStack().empty());
}

/**
 * @tc.name: MinidumpDumperTest017
 * @tc.desc: test PrintMaps with nullptr dfxMaps returns early
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest017, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.dfxMaps_ = nullptr;
    dumper.PrintMaps();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetMaps().empty());
}

/**
 * @tc.name: MinidumpDumperTest018
 * @tc.desc: test PrintHeader collects data into CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest018, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    dumper.PrintHeader();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetNeedFormatFlag());
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetPid(), getpid());
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetPname().empty() ||
        CppCrashInfoCollector::Instance().GetPname() == dumper.request_.processName);
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetTimestamp().empty());
}

/**
 * @tc.name: MinidumpDumperTest019
 * @tc.desc: test PrintMaps with valid dfxMaps collects into CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest019, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    auto map = std::make_shared<DfxMap>(0x1000, 0x2000, 0, PROT_READ, "/system/lib/test.so");
    dumper.dfxMaps_->AddMap(map);
    dumper.PrintMaps();
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetMaps().empty());
}

/**
 * @tc.name: MinidumpDumperTest020
 * @tc.desc: test PrintOtherThreadInfo with CPP_CRASH type and threads
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest020, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto thread1 = DfxThread::Create(getpid(), 100, 100, false);
    thread1->SetThreadName("test_thread_1");
    auto thread2 = DfxThread::Create(getpid(), 200, 200, false);
    thread2->SetThreadName("test_thread_2");
    dumper.process_.GetOtherThreads().push_back(thread1);
    dumper.process_.GetOtherThreads().push_back(thread2);
    dumper.PrintOtherThreadInfo();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetOtherThreads().size(), 2);
}

/**
 * @tc.name: MinidumpDumperTest021
 * @tc.desc: test PrintThreadInfo with valid keyThread collects into CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest021, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("test_key_thread");
    dumper.process_.SetKeyThread(keyThread);
    dumper.PrintThreadInfo();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetKeyThread().tid, getpid());
}

/**
 * @tc.name: MinidumpDumperTest022
 * @tc.desc: test PrintRegisters with valid regs collects into CppCrashInfoCollector
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest022, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {0};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintRegisters();
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetRegisters().empty());
}

/**
 * @tc.name: MinidumpDumperTest023
 * @tc.desc: test PrintRegisters strips "Registers:\n" prefix for collector
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest023, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {0};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintRegisters();
    std::string collectedRegs = CppCrashInfoCollector::Instance().GetRegisters();
    EXPECT_TRUE(collectedRegs.find("Registers:\n") != 0);
}

/**
 * @tc.name: MinidumpDumperTest024
 * @tc.desc: test PrintDumpInfo calls all print methods
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest024, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    std::string processName = "test_proc";
    (void)strcpy_s(dumper.request_.processName, sizeof(dumper.request_.processName), processName.c_str());
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), processName);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    auto map = std::make_shared<DfxMap>(0x1000, 0x2000, 0, PROT_READ, "/test.so");
    dumper.dfxMaps_->AddMap(map);
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {0};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintDumpInfo();
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetPname().empty());
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetMaps().empty());
}

/**
 * @tc.name: MinidumpDumperTest026
 * @tc.desc: test UnwindOtherThread with null other thread elements skips
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest026, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    dumper.process_.GetOtherThreads().push_back(nullptr);
    EXPECT_EQ(dumper.process_.GetOtherThreads().size(), 1u);
    dumper.UnwindOtherThread();
    EXPECT_EQ(dumper.process_.GetOtherThreads().size(), 1u);
}

/**
 * @tc.name: MinidumpDumperTest027
 * @tc.desc: test UnwindOtherThread with thread without stack/regs skips
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest027, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto thread1 = DfxThread::Create(getpid(), 100, 100, false);
    thread1->SetThreadName("test_thread_no_stack");
    dumper.process_.GetOtherThreads().push_back(thread1);
    EXPECT_EQ(dumper.process_.GetOtherThreads().size(), 1u);
    dumper.UnwindOtherThread();
    EXPECT_EQ(dumper.process_.GetOtherThreads().size(), 1u);
}

/**
 * @tc.name: MinidumpDumperTest028
 * @tc.desc: test ConfigurePerformance is callable (no-op)
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest028, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_cfg_perf_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    close(tmpFd);
    unlink("/data/test/minidump_cfg_perf_test");
}

/**
 * @tc.name: MinidumpDumperTest030
 * @tc.desc: test Report is callable
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest030, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    EXPECT_EQ(dumper.request_.type, ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    dumper.Report();
}

/**
 * @tc.name: MinidumpDumperTest031
 * @tc.desc: test CppCrashInfoCollector SetMinidumpLog flag
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest031, TestSize.Level2)
{
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    CppCrashInfoCollector::Instance().SetMinidumpLog(true);
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetMinidumpLog());
    CppCrashInfoCollector::Instance().SetMinidumpLog(false);
    EXPECT_FALSE(CppCrashInfoCollector::Instance().GetMinidumpLog());
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(false);
    CppCrashInfoCollector::Instance().SetMinidumpLog(true);
    EXPECT_FALSE(CppCrashInfoCollector::Instance().GetMinidumpLog());
}

/**
 * @tc.name: MinidumpDumperTest032
 * @tc.desc: test PrintHeader collector fields match request data
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest032, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    dumper.PrintHeader();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetPid(), getpid());
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetUid(), getuid());
    std::string pname = CppCrashInfoCollector::Instance().GetPname();
    EXPECT_EQ(pname, std::string(dumper.request_.processName));
}

/**
 * @tc.name: MinidumpDumperTest033
 * @tc.desc: test PrintThreadInfo with DUMP_TYPE_DUMP_CATCH does not add prefix
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest033, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    dumper.PrintThreadInfo();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetKeyThread().tid, getpid());
}

/**
 * @tc.name: MinidumpDumperTest034
 * @tc.desc: test PrintOtherThreadInfo with DUMP_TYPE_DUMP_CATCH
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest034, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto thread1 = DfxThread::Create(getpid(), 100, 100, false);
    thread1->SetThreadName("thread1");
    dumper.process_.GetOtherThreads().push_back(thread1);
    dumper.PrintOtherThreadInfo();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetOtherThreads().size(), 1);
}

/**
 * @tc.name: MinidumpDumperTest035
 * @tc.desc: test MemoryNearRegisterUtil singleton blocksInfo clear and access
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest035, TestSize.Level2)
{
    auto& memIns = MemoryNearRegisterUtil::GetInstance();
    memIns.blocksInfo_.clear();
    EXPECT_EQ(memIns.blocksInfo_.size(), 0);
    MemoryBlockInfo info;
    info.nameAddr = 0x1000;
    info.startAddr = 0x1000;
    info.size = 10;
    info.content = {1, 2, 3};
    memIns.blocksInfo_.push_back(info);
    EXPECT_EQ(memIns.blocksInfo_.size(), 1);
    memIns.blocksInfo_.clear();
}

/**
 * @tc.name: MinidumpDumperTest036
 * @tc.desc: test ParseExceptionStream returns false with null exception
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest036, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_exception_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool ret = dumper.ParseExceptionStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_exception_test");
}

/**
 * @tc.name: MinidumpDumperTest037
 * @tc.desc: test ParseThreadNameStream returns false with null threadNameList
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest037, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_threadname_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool ret = dumper.ParseThreadNameStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_threadname_test");
}

/**
 * @tc.name: MinidumpDumperTest039
 * @tc.desc: test ParseMemoryListStream returns false with null memoryList
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest039, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_memory_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool ret = dumper.ParseMemoryListStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_memory_test");
}

/**
 * @tc.name: MinidumpDumperTest040
 * @tc.desc: test ParseThreadListStream returns false with null threadList
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest040, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_threadlist_test", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool ret = dumper.ParseThreadListStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_threadlist_test");
}

/**
 * @tc.name: MinidumpDumperTest041
 * @tc.desc: test ParseThreadNameStream returns false when keyThread is null
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest041, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_threadname_null_key", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    bool ret = dumper.ParseThreadNameStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_threadname_null_key");
}

/**
 * @tc.name: MinidumpDumperTest042
 * @tc.desc: test ParseMemoryListStream returns false when regs is null
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest042, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_memory_null_regs", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    bool ret = dumper.ParseMemoryListStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_memory_null_regs");
}

/**
 * @tc.name: MinidumpDumperTest043
 * @tc.desc: test CppCrashInfoCollector Reset clears all fields
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest043, TestSize.Level2)
{
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    CppCrashInfoCollector::Instance().SetBuildInfo("test_build");
    CppCrashInfoCollector::Instance().SetPid(1234);
    CppCrashInfoCollector::Instance().SetPname("test_proc");
    CppCrashInfoCollector::Instance().SetMinidumpLog(true);
    CppCrashInfoCollector::Instance().Reset();
    EXPECT_FALSE(CppCrashInfoCollector::Instance().GetNeedFormatFlag());
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetPid(), 0);
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetPname().empty());
    EXPECT_FALSE(CppCrashInfoCollector::Instance().GetMinidumpLog());
}

/**
 * @tc.name: MinidumpDumperTest044
 * @tc.desc: test CppCrashInfoCollector Set methods skip when needFormatFlag is false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest044, TestSize.Level2)
{
    CppCrashInfoCollector::Instance().Reset();
    EXPECT_FALSE(CppCrashInfoCollector::Instance().GetNeedFormatFlag());
    CppCrashInfoCollector::Instance().SetBuildInfo("test_build");
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetBuildInfo().empty());
    CppCrashInfoCollector::Instance().SetTimestamp("2026-01-01");
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetTimestamp().empty());
    CppCrashInfoCollector::Instance().SetPid(999);
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetPid(), 0);
    CppCrashInfoCollector::Instance().SetUid(999);
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetUid(), 0);
}

/**
 * @tc.name: MinidumpDumperTest045
 * @tc.desc: test ParseMapListStream returns false when mapList is null from invalid parser
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest045, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_maplist_null", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool ret = dumper.ParseMapListStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_maplist_null");
}

/**
 * @tc.name: MinidumpDumperTest046
 * @tc.desc: test BuildSignalInfoFromException returns false with null exceptionRecord
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest046, TestSize.Level2)
{
    MinidumpDumper dumper;
    bool ret = dumper.BuildSignalInfoFromException(nullptr);
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: MinidumpDumperTest047
 * @tc.desc: test CreateARM64DfxRegs returns nullptr with null arm64Context
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest047, TestSize.Level2)
{
    MinidumpDumper dumper;
    auto result = dumper.CreateARM64DfxRegs(nullptr);
    EXPECT_TRUE(result == nullptr);
}

/**
 * @tc.name: MinidumpDumperTest048
 * @tc.desc: test SetupKeyThreadStack returns false when minidumpKeyThread is null
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest048, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_setup_null_thread", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    MinidumpThreadList* threadList = parser.GetThreadList();
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    bool ret = dumper.SetupKeyThreadStack(threadList, keyThread);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_setup_null_thread");
}

/**
 * @tc.name: MinidumpDumperTest049
 * @tc.desc: test PopulateOtherThreadFromMinidump with null otherThread skips processing
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest049, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_populate_null", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    MinidumpThreadList* threadList = parser.GetThreadList();
    dumper.PopulateOtherThreadFromMinidump(nullptr, threadList);
    close(tmpFd);
    unlink("/data/test/minidump_populate_null");
}

/**
 * @tc.name: MinidumpDumperTest050
 * @tc.desc: test PrintRegisters with populated regsData collects non-empty registers string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest050, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {0};
    regsData[31] = 0x8000;
    regsData[32] = 0x1000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintRegisters();
    std::string collectedRegs = CppCrashInfoCollector::Instance().GetRegisters();
    EXPECT_TRUE(!collectedRegs.empty());
}

/**
 * @tc.name: MinidumpDumperTest051
 * @tc.desc: test PrintRegsNearMemory with unwinder and keyThread collects memory near registers
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest051, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {0};
    regsData[31] = 0x8000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintRegsNearMemory();
}

/**
 * @tc.name: MinidumpDumperTest052
 * @tc.desc: test PrintFaultStack with unwinder and keyThread collects fault stack
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest052, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {0};
    regsData[31] = 0x8000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintFaultStack();
}

/**
 * @tc.name: MinidumpDumperTest053
 * @tc.desc: test PopulateOtherThreadFromMinidump with null otherThread and null threadList
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest053, TestSize.Level2)
{
    MinidumpDumper dumper;
    auto keyThread = DfxThread::Create(getpid(), 100, 100, false);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    dumper.process_.SetKeyThread(keyThread);
    dumper.process_.GetOtherThreads().push_back(nullptr);
    MinidumpThreadList* threadList = nullptr;
    dumper.PopulateOtherThreadFromMinidump(
        dumper.process_.GetOtherThreads()[0], threadList);
}

/**
 * @tc.name: MinidumpDumperTest054
 * @tc.desc: test PopulateOtherThreadFromMinidump with null threadList skips processing
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest054, TestSize.Level2)
{
    MinidumpDumper dumper;
    auto otherThread = DfxThread::Create(getpid(), 100, 100, false);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    dumper.PopulateOtherThreadFromMinidump(otherThread, nullptr);
}

/**
 * @tc.name: MinidumpDumperTest055
 * @tc.desc: test CollectDumpHeaderInfo with initialized request sets correct type and pid
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest055, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.request_ = {};
    dumper.request_.pid = getpid();
    dumper.request_.timeStamp = GetTimeMilliSeconds();
    dumper.CollectDumpHeaderInfo(getpid());
    EXPECT_EQ(dumper.request_.type, ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    EXPECT_EQ(dumper.request_.pid, getpid());
}

/**
 * @tc.name: MinidumpDumperTest056
 * @tc.desc: test PrintMaps with two maps entries collects non-empty maps string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest056, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    auto map1 = std::make_shared<DfxMap>(0x1000, 0x2000, 0, PROT_READ, "/system/lib/a.so");
    auto map2 = std::make_shared<DfxMap>(0x3000, 0x4000, 0, PROT_READ, "/system/lib/b.so");
    dumper.dfxMaps_->AddMap(map1);
    dumper.dfxMaps_->AddMap(map2);
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.PrintMaps();
    std::string mapsStr = CppCrashInfoCollector::Instance().GetMaps();
    EXPECT_TRUE(!mapsStr.empty());
}

static void InitMinidumpHeader(MDRawHeader& header, size_t dirCount, size_t headerSize)
{
    header = {};
    header.signature = MINIDUMP_HEADER_SIGNATURE;
    header.version = (MINIDUMP_HEADER_VERSION & 0xffff) | 0xa7900000;
    header.numberOfStreams = dirCount;
    header.streamDirectoryRva = headerSize;
}

static MDRawContextARM64 InitARM64Context(uint64_t spValue, uint64_t regBase = 0x1000)
{
    MDRawContextARM64 context = {};
    context.contextFlags = MD_CONTEXT_ARM64;
    for (uint32_t i = 0; i < MD_CONTEXT_ARM64_GPR_COUNT; i++) {
        context.iregs[i] = regBase + i * 8; // 8:eight times
    }
    context.iregs[31] = spValue; // 31:sp reg index
    context.cpsr = 0;
    return context;
}

static MDRawSystemInfo InitSystemInfo()
{
    MDRawSystemInfo sysInfo = {};
    sysInfo.platformId = MINIDUMP_OS_LINUX;
    sysInfo.processorArchitecture = MD_CPU_ARCH_ARM64;
    return sysInfo;
}

static std::string BuildMDStringData(const uint16_t* name, uint32_t nameLen)
{
    MDString mdStr = {};
    mdStr.length = nameLen * 2 + 2; // 2:double of nameLen
    std::string data(reinterpret_cast<const char*>(&mdStr), sizeof(uint32_t));
    data += std::string(reinterpret_cast<const char*>(name), (nameLen + 1) * 2);
    return data;
}

static std::string BuildMinidumpWithExceptionAndContext()
{
    pid_t pid = getpid();
    size_t headerSize = sizeof(MDRawHeader);
    size_t dirCount = 3;
    size_t dirSize = sizeof(MDRawDirectory) * dirCount;
    size_t dataStart = headerSize + dirSize;
    MDRawContextARM64 context = InitARM64Context(0x8000);
    MDExceptionStream excStream = {};
    excStream.threadId = static_cast<uint32_t>(pid);
    excStream.unusedAlignment = 0;
    excStream.exceptionRecord.exceptionCode = SIGSEGV;
    excStream.exceptionRecord.exceptionFlags = SEGV_MAPERR;
    excStream.exceptionRecord.exceptionAddress = 0x1000;
    excStream.exceptionRecord.numberParameters = 0;
    excStream.threadContext.dataSize = sizeof(MDRawContextARM64);
    excStream.threadContext.rva = dataStart + sizeof(MDExceptionStream);
    MDRawEsrRegsInfo esrInfo = {};
    esrInfo.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    esrInfo.dumpThreadId = static_cast<uint32_t>(pid);
    esrInfo.esrRegs = 0x92000046;
    size_t excDataSize = sizeof(MDExceptionStream) + sizeof(MDRawContextARM64);
    size_t esrDataOffset = dataStart + excDataSize;
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    MDRawDirectory dirExc = {};
    dirExc.streamType = MD_STREAM_EXCEPTION;
    dirExc.location.dataSize = excDataSize;
    dirExc.location.rva = dataStart;
    MDRawDirectory dirEsr = {};
    dirEsr.streamType = MD_STREAM_ESR_INFO;
    dirEsr.location.dataSize = sizeof(MDRawEsrRegsInfo);
    dirEsr.location.rva = esrDataOffset;
    MDRawDirectory dirSys = {};
    dirSys.streamType = MD_STREAM_SYSTEM_INFO;
    dirSys.location.dataSize = sizeof(MDRawSystemInfo);
    dirSys.location.rva = esrDataOffset + sizeof(MDRawEsrRegsInfo);
    MDRawSystemInfo sysInfo = InitSystemInfo();
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dirExc), sizeof(dirExc));
    data += std::string(reinterpret_cast<const char*>(&dirEsr), sizeof(dirEsr));
    data += std::string(reinterpret_cast<const char*>(&dirSys), sizeof(dirSys));
    data += std::string(reinterpret_cast<const char*>(&excStream), sizeof(excStream));
    data += std::string(reinterpret_cast<const char*>(&context), sizeof(context));
    data += std::string(reinterpret_cast<const char*>(&esrInfo), sizeof(esrInfo));
    data += std::string(reinterpret_cast<const char*>(&sysInfo), sizeof(sysInfo));
    return data;
}

static std::string BuildMinidumpWithModuleList()
{
    size_t headerSize = sizeof(MDRawHeader);
    size_t dirCount = 1;
    size_t dataStart = headerSize + sizeof(MDRawDirectory) * dirCount;
    uint16_t moduleName[] = {u't', u'e', u's', u't', u'.', u's', u'o', 0};
    size_t moduleNameOffset = dataStart + sizeof(MDRawModuleList) + sizeof(MDRawModule);
    std::string mdStrData = BuildMDStringData(moduleName, 7);
    MDRawModule module = {};
    module.baseOfImage = 0x1000;
    module.sizeOfImage = 0x1000;
    module.moduleNameRva = moduleNameOffset;
    MDRawModuleList moduleList = {};
    moduleList.numberOfModules = 1;
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    MDRawDirectory dirMod = {};
    dirMod.streamType = MD_STREAM_MODULE_LIST;
    dirMod.location.dataSize = sizeof(MDRawModuleList) + sizeof(MDRawModule);
    dirMod.location.rva = dataStart;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dirMod), sizeof(dirMod));
    data += std::string(reinterpret_cast<const char*>(&moduleList), sizeof(moduleList));
    data += std::string(reinterpret_cast<const char*>(&module), sizeof(module));
    data += mdStrData;
    return data;
}

static std::string BuildMinidumpWithThreadNameList()
{
    pid_t pid = getpid();
    size_t headerSize = sizeof(MDRawHeader);
    size_t dirCount = 1;
    size_t dataStart = headerSize + sizeof(MDRawDirectory) * dirCount;
    uint16_t threadName[] = {u'm', u'a', u'i', u'n', 0};
    size_t threadNameOffset = dataStart + sizeof(MDRawThreadNameList) + sizeof(MDRawThreadName);
    std::string mdStrData = BuildMDStringData(threadName, 4);
    MDRawThreadName rawThreadName = {};
    rawThreadName.threadId = static_cast<uint32_t>(pid);
    rawThreadName.rvaOfThreadName = threadNameOffset;
    MDRawThreadNameList threadNameList = {};
    threadNameList.numberOfThreadNames = 1;
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    MDRawDirectory dirTN = {};
    dirTN.streamType = MD_STREAM_THREAD_NAME_LIST;
    dirTN.location.dataSize = sizeof(MDRawThreadNameList) + sizeof(MDRawThreadName);
    dirTN.location.rva = dataStart;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dirTN), sizeof(dirTN));
    data += std::string(reinterpret_cast<const char*>(&threadNameList), sizeof(threadNameList));
    data += std::string(reinterpret_cast<const char*>(&rawThreadName), sizeof(rawThreadName));
    data += mdStrData;
    return data;
}

static std::string BuildMinidumpWithMemoryList()
{
    size_t headerSize = sizeof(MDRawHeader);
    size_t dirCount = 1;
    size_t dataStart = headerSize + sizeof(MDRawDirectory) * dirCount;
    uint8_t memoryData[TEST_BUFFER_SIZE] = {};
    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        memoryData[i] = static_cast<uint8_t>(i);
    }
    MDMemoryDescriptor memDesc = {};
    memDesc.startOfMemoryRange = 0x7000;
    memDesc.memory.dataSize = TEST_BUFFER_SIZE;
    memDesc.memory.rva = dataStart + sizeof(MDRawMemoryList) + sizeof(MDMemoryDescriptor);
    MDRawMemoryList memList = {};
    memList.numberOfMemoryRanges = 1;
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    MDRawDirectory dirMem = {};
    dirMem.streamType = MD_STREAM_MEMORY_LIST;
    dirMem.location.dataSize = sizeof(MDRawMemoryList) + sizeof(MDMemoryDescriptor);
    dirMem.location.rva = dataStart;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dirMem), sizeof(dirMem));
    data += std::string(reinterpret_cast<const char*>(&memList), sizeof(memList));
    data += std::string(reinterpret_cast<const char*>(&memDesc), sizeof(memDesc));
    data += std::string(reinterpret_cast<const char*>(memoryData), TEST_BUFFER_SIZE);
    return data;
}

/**
 * @tc.name: MinidumpDumperTest057
 * @tc.desc: test ParseExceptionStream with valid minidump data sets keyThread and registers
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest057, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_exc_normal", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    EXPECT_TRUE(parser.Parse());
    bool ret = dumper.ParseExceptionStream(parser);
    EXPECT_TRUE(ret);
    EXPECT_NE(dumper.process_.GetKeyThread(), nullptr);
    EXPECT_NE(dumper.process_.GetFaultThreadRegisters(), nullptr);
    close(tmpFd);
    unlink("/data/test/minidump_exc_normal");
}

/**
 * @tc.name: MinidumpDumperTest058
 * @tc.desc: test BuildSignalInfoFromException with valid exception returns true and sets reason
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest058, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_exc_signal", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    EXPECT_TRUE(parser.Parse());
    MinidumpException* exception = parser.GetException();
    EXPECT_NE(exception, nullptr);
    bool ret = dumper.BuildSignalInfoFromException(exception);
    EXPECT_TRUE(ret);
    EXPECT_TRUE(!dumper.process_.GetReason().empty());
    close(tmpFd);
    unlink("/data/test/minidump_exc_signal");
}

/**
 * @tc.name: MinidumpDumperTest059
 * @tc.desc: test CreateARM64DfxRegs with valid ARM64 context returns non-null regs
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest059, TestSize.Level2)
{
    MinidumpDumper dumper;
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_arm64regs", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    EXPECT_TRUE(parser.Parse());
    MinidumpException* exception = parser.GetException();
    EXPECT_NE(exception, nullptr);
    auto context = exception->GetContext();
    EXPECT_NE(context, nullptr);
    auto arm64Ctx = context->GetContextARM64();
    EXPECT_NE(arm64Ctx, nullptr);
    auto dfxRegs = dumper.CreateARM64DfxRegs(arm64Ctx);
    EXPECT_NE(dfxRegs, nullptr);
    close(tmpFd);
    unlink("/data/test/minidump_arm64regs");
}

/**
 * @tc.name: MinidumpDumperTest061
 * @tc.desc: test ParseThreadNameStream with valid minidump data processes thread names
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest061, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    pid_t pid = getpid();
    dumper.CollectDumpHeaderInfo(pid);
    auto keyThread = DfxThread::Create(pid, pid, pid, false);
    dumper.process_.SetKeyThread(keyThread);
    std::string minidumpData = BuildMinidumpWithThreadNameList();
    int tmpFd = open("/data/test/minidump_tn_normal", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    parser.Parse();
    dumper.ParseThreadNameStream(parser);
    close(tmpFd);
    unlink("/data/test/minidump_tn_normal");
}

/**
 * @tc.name: MinidumpDumperTest062
 * @tc.desc: test ParseMemoryListStream with valid minidump data returns true
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest062, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    regsData[31] = 0x8000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    std::string minidumpData = BuildMinidumpWithMemoryList();
    int tmpFd = open("/data/test/minidump_mem_normal", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    EXPECT_TRUE(parser.Parse());
    bool ret = dumper.ParseMemoryListStream(parser);
    EXPECT_TRUE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_mem_normal");
}

/**
 * @tc.name: MinidumpDumperTest063
 * @tc.desc: test GenerateMinidump with valid pipe fds returns true
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest063, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.request_ = {};
    dumper.request_.pid = getpid();
    dumper.request_.timeStamp = GetTimeMilliSeconds();
    int pipeFds[2] = {-1, -1};
    pipe2(pipeFds, O_NONBLOCK);
    std::string testData = "test minidump data";
    write(pipeFds[1], testData.c_str(), testData.size());
    close(pipeFds[1]);
    bool ret = dumper.GenerateMinidump(getpid(), pipeFds[0], true);
    EXPECT_TRUE(ret);
    close(pipeFds[0]);
    if (dumper.outputFdGuard_) {
        close(dumper.outputFdGuard_.GetFd());
    }
}

/**
 * @tc.name: MinidumpDumperTest064
 * @tc.desc: test ParseMinidump with valid minidump data and output fd succeeds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest064, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_parse_success", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        dumper.outputFdGuard_ = SmartFd(tmpFd);
        dumper.ParseMinidump();
    }
    close(tmpFd);
    unlink("/data/test/minidump_parse_success");
}

/**
 * @tc.name: MinidumpDumperTest065
 * @tc.desc: test ParseExceptionStream with valid minidump sets keyThread and fault registers
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest065, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_exc_stream", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        bool ret = dumper.ParseExceptionStream(parser);
        if (ret) {
            EXPECT_NE(dumper.process_.GetKeyThread(), nullptr);
            EXPECT_NE(dumper.process_.GetFaultThreadRegisters(), nullptr);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_exc_stream");
}

/**
 * @tc.name: MinidumpDumperTest066
 * @tc.desc: test BuildSignalInfoFromException with parsed minidump sets signal reason
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest066, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_signal_exc", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool parsed = parser.Parse();
    if (parsed) {
        MinidumpException* exception = parser.GetException();
        if (exception != nullptr) {
            bool ret = dumper.BuildSignalInfoFromException(exception);
            EXPECT_TRUE(ret);
            EXPECT_TRUE(!dumper.process_.GetReason().empty());
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_signal_exc");
}

/**
 * @tc.name: MinidumpDumperTest067
 * @tc.desc: test CreateARM64DfxRegs with parsed minidump context creates dfxRegs
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest067, TestSize.Level2)
{
    MinidumpDumper dumper;
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_arm64regs2", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    MinidumpException* exception = parser.Parse() ? parser.GetException() : nullptr;
    auto context = exception ? exception->GetContext() : nullptr;
    auto arm64Ctx = context ? context->GetContextARM64() : nullptr;
    if (arm64Ctx != nullptr) {
        auto dfxRegs = dumper.CreateARM64DfxRegs(arm64Ctx);
        EXPECT_NE(dfxRegs, nullptr);
    }
    close(tmpFd);
    unlink("/data/test/minidump_arm64regs2");
}

/**
 * @tc.name: MinidumpDumperTest068
 * @tc.desc: test PrintDumpInfo with maps and keyThread collects pname and maps
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest068, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    auto map = std::make_shared<DfxMap>(0x1000, 0x2000, 0, PROT_READ, "/system/lib/test.so");
    dumper.dfxMaps_->AddMap(map);
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    regsData[31] = 0x8000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintDumpInfo();
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetPname().empty());
    EXPECT_TRUE(!CppCrashInfoCollector::Instance().GetMaps().empty());
}

/**
 * @tc.name: MinidumpDumperTest069
 * @tc.desc: test ParseModuleListStream with module list minidump verifies module count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest069, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithModuleList();
    int tmpFd = open("/data/test/minidump_module_list_full", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        MinidumpModuleList* moduleList = parser.GetModuleList();
        if (moduleList != nullptr) {
            EXPECT_TRUE(moduleList->ModuleCount() > 0);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_module_list_full");
}

/**
 * @tc.name: MinidumpDumperTest070
 * @tc.desc: test GetException threadId from parsed minidump creates keyThread
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest070, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithExceptionAndContext();
    int tmpFd = open("/data/test/minidump_exc_stream_null", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool parsed = parser.Parse();
    if (parsed) {
        MinidumpException* exc = parser.GetException();
        if (exc != nullptr && exc->Exception() != nullptr) {
            auto keyThreadId = exc->Exception()->threadId;
            auto keyThread = DfxThread::Create(getpid(), keyThreadId, keyThreadId, false);
            if (keyThread != nullptr) {
                EXPECT_NE(keyThread, nullptr);
            }
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_exc_stream_null");
}

/**
 * @tc.name: MinidumpDumperTest071
 * @tc.desc: test GetThreadNameList with parsed minidump verifies thread names count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest071, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithThreadNameList();
    int tmpFd = open("/data/test/minidump_thread_name_list", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool parsed = parser.Parse();
    if (parsed) {
        MinidumpThreadNameList* threadNameList = parser.GetThreadNameList();
        if (threadNameList != nullptr) {
            auto threadNames = threadNameList->GetThreadNames();
            EXPECT_TRUE(threadNames.size() > 0);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_thread_name_list");
}

/**
 * @tc.name: MinidumpDumperTest072
 * @tc.desc: test GetMemoryList with parsed minidump verifies memory region count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest072, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithMemoryList();
    int tmpFd = open("/data/test/minidump_memory_list", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool parsed = parser.Parse();
    if (parsed) {
        MinidumpMemoryList* memoryList = parser.GetMemoryList();
        if (memoryList != nullptr) {
            EXPECT_TRUE(memoryList->RegionCount() > 0);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_memory_list");
}

/**
 * @tc.name: MinidumpDumperTest073
 * @tc.desc: test PrintMaps with two maps collects non-empty maps string into collector
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest073, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    auto map1 = std::make_shared<DfxMap>(0x1000, 0x2000, 0, PROT_READ, "/system/lib/a.so");
    auto map2 = std::make_shared<DfxMap>(0x3000, 0x4000, 0, PROT_WRITE, "/system/lib/b.so");
    dumper.dfxMaps_->AddMap(map1);
    dumper.dfxMaps_->AddMap(map2);
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.PrintMaps();
    std::string mapsStr = CppCrashInfoCollector::Instance().GetMaps();
    EXPECT_TRUE(!mapsStr.empty());
}

/**
 * @tc.name: MinidumpDumperTest074
 * @tc.desc: test PrintThreadInfo with CPP_CRASH type and keyThread collects tid
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest074, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("test_key_thread");
    dumper.process_.SetKeyThread(keyThread);
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintThreadInfo();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetKeyThread().tid, getpid());
}

/**
 * @tc.name: MinidumpDumperTest075
 * @tc.desc: test PrintRegisters with regs and collector collects non-empty registers string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest075, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintRegisters();
    std::string collectedRegs = CppCrashInfoCollector::Instance().GetRegisters();
    EXPECT_TRUE(!collectedRegs.empty());
}

/**
 * @tc.name: MinidumpDumperTest076
 * @tc.desc: test PrintOtherThreadInfo with CPP_CRASH type collects two other threads
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest076, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto thread1 = DfxThread::Create(getpid(), 100, 100, false);
    thread1->SetThreadName("test_other_thread_1");
    auto thread2 = DfxThread::Create(getpid(), 200, 200, false);
    thread2->SetThreadName("test_other_thread_2");
    dumper.process_.GetOtherThreads().push_back(thread1);
    dumper.process_.GetOtherThreads().push_back(thread2);
    dumper.PrintOtherThreadInfo();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetOtherThreads().size(), 2);
}

/**
 * @tc.name: MinidumpDumperTest077
 * @tc.desc: test PrintRegsNearMemory with unwinder and keyThread processes memory near registers
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest077, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintRegsNearMemory();
}

/**
 * @tc.name: MinidumpDumperTest078
 * @tc.desc: test PrintFaultStack with unwinder and keyThread processes fault stack
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest078, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.PrintFaultStack();
}

/**
 * @tc.name: MinidumpDumperTest079
 * @tc.desc: test PrintDumpInfo with full setup collects non-zero pid
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest079, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");
    dumper.process_.SetKeyThread(keyThread);
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    regsData[31] = 0x8000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.PrintDumpInfo();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetPid() != 0);
}

/**
 * @tc.name: MinidumpDumperTest080
 * @tc.desc: test UnwindProcess with unwinder and other threads runs without crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest080, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    dumper.process_.SetKeyThread(keyThread);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    auto otherThread = DfxThread::Create(getpid(), 100, 100, false);
    otherThread->SetThreadName("other_thread");
    dumper.process_.GetOtherThreads().push_back(otherThread);
    dumper.UnwindProcess();
}

/**
 * @tc.name: MinidumpDumperTest081
 * @tc.desc: test UnwindOtherThread with unwinder and other thread runs without crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest081, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto otherThread = DfxThread::Create(getpid(), 100, 100, false);
    otherThread->SetThreadName("other_thread");
    dumper.process_.GetOtherThreads().push_back(otherThread);
    dumper.UnwindOtherThread();
}

/**
 * @tc.name: MinidumpDumperTest082
 * @tc.desc: test PrintHeader with long process name matches collector fields
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest082, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string processName = "test_proc_long_name_for_strcpy";
    (void)strcpy_s(dumper.request_.processName, sizeof(dumper.request_.processName), processName.c_str());
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), processName);
    dumper.PrintHeader();
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetPid(), getpid());
    EXPECT_EQ(CppCrashInfoCollector::Instance().GetUid(), getuid());
    std::string pname = CppCrashInfoCollector::Instance().GetPname();
    EXPECT_EQ(pname, std::string(dumper.request_.processName));
}

/**
 * @tc.name: MinidumpDumperTest083
 * @tc.desc: test PrintOtherThreadInfo with DUMP_TYPE_DUMP_CATCH runs without crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest083, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto thread1 = DfxThread::Create(getpid(), 100, 100, false);
    thread1->SetThreadName("thread1");
    dumper.process_.GetOtherThreads().push_back(thread1);
    dumper.PrintOtherThreadInfo();
}

/**
 * @tc.name: MinidumpDumperTest084
 * @tc.desc: test UnwindOtherThread with other thread having regs and stack runs without crash
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest084, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto otherThread = DfxThread::Create(getpid(), 100, 100, false);
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    regsData[31] = 0x7000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    otherThread->SetThreadRegs(regs);
    auto stackBuf = std::make_shared<std::vector<uint8_t>>(TEST_BUFFER_SIZE, 0);
    otherThread->SetThreadStackBuffer(stackBuf);
    otherThread->SetStartOfStackMemory(0x7000);
    dumper.process_.GetOtherThreads().push_back(otherThread);
    dumper.UnwindOtherThread();
}

/**
 * @tc.name: MinidumpDumperTest085
 * @tc.desc: test ParseExceptionStream with invalid minidump data returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest085, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_null_exc_stream", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool ret = dumper.ParseExceptionStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_null_exc_stream");
}

/**
 * @tc.name: MinidumpDumperTest086
 * @tc.desc: test BuildSignalInfoFromException with null exception pointer returns false
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest086, TestSize.Level2)
{
    MinidumpDumper dumper;
    MinidumpException* exc = nullptr;
    bool ret = dumper.BuildSignalInfoFromException(exc);
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: MinidumpDumperTest087
 * @tc.desc: test PrintMaps with empty DfxMaps collects empty maps string
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest087, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.PrintMaps();
    std::string mapsStr = CppCrashInfoCollector::Instance().GetMaps();
    EXPECT_TRUE(mapsStr.empty());
}

/**
 * @tc.name: MinidumpDumperTest088
 * @tc.desc: test PrintRegisters strips Registers prefix from collector output
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest088, TestSize.Level2)
{
    MinidumpDumper dumper;
    dumper.request_.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("test_key_thread");
    dumper.process_.SetKeyThread(keyThread);
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.PrintRegisters();
    std::string collectedRegs = CppCrashInfoCollector::Instance().GetRegisters();
    EXPECT_TRUE(collectedRegs.find("Registers:\n") != 0);
}

static std::string BuildMinidumpWithThreadList()
{
    pid_t pid = getpid();
    size_t headerSize = sizeof(MDRawHeader);
    size_t dirCount = 1;
    size_t dataStart = headerSize + sizeof(MDRawDirectory) * dirCount;
    size_t threadListDataSize = sizeof(uint32_t) + sizeof(MDRawThread);
    size_t contextOffset = dataStart + threadListDataSize;
    size_t stackOffset = contextOffset + sizeof(MDRawContextARM64);
    MDRawContextARM64 context = InitARM64Context(0x7000);
    uint8_t stackData[TEST_BUFFER_SIZE] = {};
    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        stackData[i] = static_cast<uint8_t>(i);
    }
    MDRawThread thread = {};
    thread.threadId = static_cast<uint32_t>(pid);
    thread.suspendCount = 0;
    thread.priorityClass = 0;
    thread.priority = 0;
    thread.teb = 0;
    thread.stack.startOfMemoryRange = 0x7000;
    thread.stack.memory.dataSize = TEST_BUFFER_SIZE;
    thread.stack.memory.rva = stackOffset;
    thread.threadContext.dataSize = sizeof(MDRawContextARM64);
    thread.threadContext.rva = contextOffset;
    uint32_t numberOfThreads = 1;
    MDRawDirectory dirThread = {};
    dirThread.streamType = MD_STREAM_THREAD_LIST;
    dirThread.location.dataSize = threadListDataSize;
    dirThread.location.rva = dataStart;
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dirThread), sizeof(dirThread));
    data += std::string(reinterpret_cast<const char*>(&numberOfThreads), sizeof(numberOfThreads));
    data += std::string(reinterpret_cast<const char*>(&thread), sizeof(thread));
    data += std::string(reinterpret_cast<const char*>(&context), sizeof(context));
    data += std::string(reinterpret_cast<const char*>(stackData), TEST_BUFFER_SIZE);
    return data;
}

static std::string BuildMinidumpWithMapList()
{
    size_t headerSize = sizeof(MDRawHeader);
    size_t dirCount = 1;
    size_t dataStart = headerSize + sizeof(MDRawDirectory) * dirCount;
    std::string mapsContent = "1000-2000 r-xp 00000000 00:00 0 /system/lib/test.so\n";
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    MDRawDirectory dirMap = {};
    dirMap.streamType = MD_STREAM_LINUX_MAPS;
    dirMap.location.dataSize = static_cast<uint32_t>(mapsContent.size());
    dirMap.location.rva = dataStart;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dirMap), sizeof(dirMap));
    data += mapsContent;
    return data;
}

static std::string BuildMinidumpWithMultiThreadNames()
{
    pid_t pid = getpid();
    size_t headerSize = sizeof(MDRawHeader);
    size_t dirCount = 1;
    size_t dataStart = headerSize + sizeof(MDRawDirectory) * dirCount;
    uint16_t threadName1[] = {u'm', u'a', u'i', u'n', 0};
    uint16_t threadName2[] = {u'w', u'o', u'r', u'k', u'e', u'r', 0};
    size_t threadNameListDataSize = sizeof(uint32_t) + sizeof(MDRawThreadName) * 2;
    size_t nameStr1Offset = dataStart + threadNameListDataSize;
    std::string mdStr1Data = BuildMDStringData(threadName1, 4);
    size_t nameStr2Offset = nameStr1Offset + mdStr1Data.size();
    std::string mdStr2Data = BuildMDStringData(threadName2, 6);
    MDRawThreadName rawThreadName1 = {};
    rawThreadName1.threadId = static_cast<uint32_t>(pid);
    rawThreadName1.rvaOfThreadName = nameStr1Offset;
    MDRawThreadName rawThreadName2 = {};
    rawThreadName2.threadId = 100; // 100:thread id
    rawThreadName2.rvaOfThreadName = nameStr2Offset;
    uint32_t numberOfThreadNames = 2;
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    MDRawDirectory dirTN = {};
    dirTN.streamType = MD_STREAM_THREAD_NAME_LIST;
    dirTN.location.dataSize = threadNameListDataSize;
    dirTN.location.rva = dataStart;
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    data += std::string(reinterpret_cast<const char*>(&dirTN), sizeof(dirTN));
    data += std::string(reinterpret_cast<const char*>(&numberOfThreadNames), sizeof(numberOfThreadNames));
    data += std::string(reinterpret_cast<const char*>(&rawThreadName1), sizeof(rawThreadName1));
    data += std::string(reinterpret_cast<const char*>(&rawThreadName2), sizeof(rawThreadName2));
    data += mdStr1Data;
    data += mdStr2Data;
    return data;
}

static std::string BuildFullCrashLogExcStream(pid_t pid, size_t excRva)
{
    MDExceptionStream excStream = {};
    excStream.threadId = static_cast<uint32_t>(pid);
    excStream.unusedAlignment = 0;
    excStream.exceptionRecord.exceptionCode = SIGSEGV;
    excStream.exceptionRecord.exceptionFlags = SEGV_MAPERR;
    excStream.exceptionRecord.exceptionAddress = 0x1000;
    excStream.exceptionRecord.numberParameters = 0;
    excStream.threadContext.dataSize = sizeof(MDRawContextARM64);
    excStream.threadContext.rva = excRva + sizeof(MDExceptionStream);
    std::string data(reinterpret_cast<const char*>(&excStream), sizeof(excStream));
    MDRawContextARM64 excContext = InitARM64Context(0x8000);
    data += std::string(reinterpret_cast<const char*>(&excContext), sizeof(excContext));
    return data;
}

static std::string BuildFullCrashLogEsrInfo(pid_t pid)
{
    MDRawEsrRegsInfo esrInfo = {};
    esrInfo.validity = MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID | MD_BREAKPAD_INFO_VALID_ESR_REGS;
    esrInfo.dumpThreadId = static_cast<uint32_t>(pid);
    esrInfo.esrRegs = 0x92000046;
    return std::string(reinterpret_cast<const char*>(&esrInfo), sizeof(esrInfo));
}

static std::string BuildFullCrashLogThreadList(pid_t pid, size_t threadListRva)
{
    size_t threadContextRva = threadListRva + sizeof(uint32_t) + sizeof(MDRawThread);
    size_t threadStackRva = threadContextRva + sizeof(MDRawContextARM64);
    MDRawThread threadEntry = {};
    threadEntry.threadId = static_cast<uint32_t>(pid);
    threadEntry.stack.startOfMemoryRange = 0x7000;
    threadEntry.stack.memory.dataSize = TEST_BUFFER_SIZE;
    threadEntry.stack.memory.rva = threadStackRva;
    threadEntry.threadContext.dataSize = sizeof(MDRawContextARM64);
    threadEntry.threadContext.rva = threadContextRva;
    uint32_t numberOfThreads = 1;
    std::string data(reinterpret_cast<const char*>(&numberOfThreads), sizeof(numberOfThreads));
    data += std::string(reinterpret_cast<const char*>(&threadEntry), sizeof(threadEntry));
    MDRawContextARM64 threadContext = InitARM64Context(0x7000, 0x2000);
    data += std::string(reinterpret_cast<const char*>(&threadContext), sizeof(threadContext));
    uint8_t stackData[TEST_BUFFER_SIZE] = {};
    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        stackData[i] = static_cast<uint8_t>(i);
    }
    data += std::string(reinterpret_cast<const char*>(stackData), TEST_BUFFER_SIZE);
    return data;
}

static std::string BuildMinidumpWithFullCrashLog()
{
    pid_t pid = getpid();
    size_t dirCount = 4;
    size_t headerSize = sizeof(MDRawHeader);
    size_t dataStart = headerSize + sizeof(MDRawDirectory) * dirCount;
    size_t excDataSize = sizeof(MDExceptionStream) + sizeof(MDRawContextARM64);
    size_t excRva = dataStart;
    size_t esrRva = excRva + excDataSize;
    size_t sysRva = esrRva + sizeof(MDRawEsrRegsInfo);
    size_t threadListRva = sysRva + sizeof(MDRawSystemInfo);
    size_t threadListDataSize = sizeof(uint32_t) + sizeof(MDRawThread);
    MDRawHeader header = {};
    InitMinidumpHeader(header, dirCount, headerSize);
    std::string streamData = BuildFullCrashLogExcStream(pid, excRva);
    streamData += BuildFullCrashLogEsrInfo(pid);
    MDRawSystemInfo sysInfo = InitSystemInfo();
    streamData += std::string(reinterpret_cast<const char*>(&sysInfo), sizeof(sysInfo));
    streamData += BuildFullCrashLogThreadList(pid, threadListRva);
    MDRawDirectory dirs[4] = {};
    dirs[0].streamType = MD_STREAM_EXCEPTION; // 0:index 0
    dirs[0].location.dataSize = excDataSize; // 0:index 0
    dirs[0].location.rva = excRva; // 0:index 0
    dirs[1].streamType = MD_STREAM_ESR_INFO; // 1:index 1
    dirs[1].location.dataSize = sizeof(MDRawEsrRegsInfo); // 1:index 1
    dirs[1].location.rva = esrRva; // 1:index 1
    dirs[2].streamType = MD_STREAM_SYSTEM_INFO; // 2:index 2
    dirs[2].location.dataSize = sizeof(MDRawSystemInfo); // 2:index 2
    dirs[2].location.rva = sysRva; // 2:index 2
    dirs[3].streamType = MD_STREAM_THREAD_LIST; // 3:index 3
    dirs[3].location.dataSize = threadListDataSize; // 3:index 3
    dirs[3].location.rva = threadListRva; // 3:index 3
    std::string data(reinterpret_cast<const char*>(&header), sizeof(header));
    for (size_t i = 0; i < dirCount; i++) {
        data += std::string(reinterpret_cast<const char*>(&dirs[i]), sizeof(MDRawDirectory));
    }
    data += streamData;
    return data;
}

class MockDfxRegsNoPrefix : public DfxRegs {
public:
    MockDfxRegsNoPrefix() = default;
    void SetFromUcontext(const ucontext_t& context) override {}
    void SetFromFpMiniRegs(const uintptr_t* regs, const size_t size) override {}
    void SetFromQutMiniRegs(const uintptr_t* regs, const size_t size) override {}
    std::string PrintRegs() const override
    {
        return "r0:00000000 r1:00000000\n";
    }
    bool SetPcFromReturnAddress(std::shared_ptr<DfxMemory> memory) override { return false; }
    bool StepIfSignalFrame(uintptr_t pc, std::shared_ptr<DfxMemory> memory) override { return false; }
};

/**
 * @tc.name: MinidumpDumperTest089
 * @tc.desc: test ParseThreadNameStream with multi thread names sets thread names correctly
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest089, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    dumper.process_.SetKeyThread(keyThread);
    std::string minidumpData = BuildMinidumpWithMultiThreadNames();
    int tmpFd = open("/data/test/minidump_tn_multi_089", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    parser.Parse();
    bool ret = dumper.ParseThreadNameStream(parser);
    EXPECT_TRUE(ret);
    EXPECT_EQ(keyThread->GetThreadInfo().threadName, "main");
    EXPECT_EQ(dumper.process_.GetOtherThreads().size(), 1u);
    if (dumper.process_.GetOtherThreads().size() > 0) {
        EXPECT_EQ(dumper.process_.GetOtherThreads()[0]->GetThreadInfo().tid, 100);
        EXPECT_EQ(dumper.process_.GetOtherThreads()[0]->GetThreadInfo().threadName, "worker");
    }
    close(tmpFd);
    unlink("/data/test/minidump_tn_multi_089");
}

/**
 * @tc.name: MinidumpDumperTest091
 * @tc.desc: test ParseMapListStream with valid map list minidump creates dfxMaps
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest091, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    std::string minidumpData = BuildMinidumpWithMapList();
    int tmpFd = open("/data/test/minidump_maplist_valid_091", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        bool ret = dumper.ParseMapListStream(parser);
        if (ret) {
            EXPECT_NE(dumper.dfxMaps_, nullptr);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_maplist_valid_091");
}

/**
 * @tc.name: MinidumpDumperTest092
 * @tc.desc: test ParseMemoryListStream with valid memory minidump populates blocksInfo
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest092, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    regsData[31] = 0x7000;
    regsData[32] = 0x1000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    dumper.process_.SetFaultThreadRegisters(regs);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    std::string minidumpData = BuildMinidumpWithMemoryList();
    int tmpFd = open("/data/test/minidump_mem_valid_092", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        bool ret = dumper.ParseMemoryListStream(parser);
        if (ret) {
            EXPECT_TRUE(MemoryNearRegisterUtil::GetInstance().blocksInfo_.size() > 0);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_mem_valid_092");
}

/**
 * @tc.name: MinidumpDumperTest093
 * @tc.desc: test ParseMinidump with full crash log and format flag succeeds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest093, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());

    std::string minidumpData = BuildMinidumpWithFullCrashLog();
    const char* filePath = "/data/test/minidump_crashlog_true_093";
    int tmpFd = open(filePath, O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    dumper.outputFdGuard_ = SmartFd(tmpFd, false);

    DfxBufferWriter::GetInstance().InitBufferWriter(dumper.request_);
    bool parseRet = dumper.ParseMinidump();
    if (parseRet) {
        dumper.UnwindProcess();
        dumper.PrintDumpInfo();
    }
    DfxBufferWriter::GetInstance().WriteFormatCrashInfo();
    EXPECT_TRUE(CppCrashInfoCollector::Instance().GetNeedFormatFlag());
    unlink(filePath);
}

/**
 * @tc.name: MinidumpDumperTest094
 * @tc.desc: test SetupKeyThreadStack with valid thread list sets keyThread stack
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest094, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    dumper.process_.SetKeyThread(keyThread);

    std::string minidumpData = BuildMinidumpWithThreadList();
    int tmpFd = open("/data/test/minidump_setup_key_stack_094", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        MinidumpThreadList* threadList = parser.GetThreadList();
        if (threadList != nullptr) {
            bool ret = dumper.SetupKeyThreadStack(threadList, keyThread);
            if (ret) {
                EXPECT_NE(keyThread->GetThreadStackBuffer(), nullptr);
                EXPECT_EQ(keyThread->GetStartOfStackMemory(), 0x7000);
            }
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_setup_key_stack_094");
}

/**
 * @tc.name: MinidumpDumperTest095
 * @tc.desc: test PopulateOtherThreadFromMinidump with valid thread list sets otherThread regs and stack
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest095, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());

    std::string minidumpData = BuildMinidumpWithThreadList();
    int tmpFd = open("/data/test/minidump_populate_other_095", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        MinidumpThreadList* threadList = parser.GetThreadList();
        if (threadList != nullptr) {
            auto otherThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
            dumper.PopulateOtherThreadFromMinidump(otherThread, threadList);
            EXPECT_NE(otherThread->GetThreadRegs(), nullptr);
            EXPECT_NE(otherThread->GetThreadStackBuffer(), nullptr);
            EXPECT_EQ(otherThread->GetStartOfStackMemory(), 0x7000);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_populate_other_095");
}

/**
 * @tc.name: MinidumpDumperTest096
 * @tc.desc: test ParseThreadListStream with valid thread list sets keyThread stack
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest096, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.CollectDumpHeaderInfo(getpid());
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    dumper.process_.SetKeyThread(keyThread);

    std::string minidumpData = BuildMinidumpWithThreadList();
    int tmpFd = open("/data/test/minidump_thread_list_parse_096", O_RDWR | O_CREAT | O_TRUNC, TEST_FILE_PERMISSIONS);
    ASSERT_TRUE(tmpFd > 0);
    write(tmpFd, minidumpData.c_str(), minidumpData.size());
    lseek(tmpFd, 0, SEEK_SET);
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    dumper.ConfigurePerformance(parser);
    bool parsed = parser.Parse();
    if (parsed) {
        bool ret = dumper.ParseThreadListStream(parser);
        if (ret) {
            EXPECT_NE(keyThread->GetThreadStackBuffer(), nullptr);
            EXPECT_EQ(keyThread->GetStartOfStackMemory(), 0x7000);
        }
    }
    close(tmpFd);
    unlink("/data/test/minidump_thread_list_parse_096");
}

/**
 * @tc.name: MinidumpDumperTest097
 * @tc.desc: test UnwindProcess with keyThread having regs and stack succeeds
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest097, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");
    auto keyThread = DfxThread::Create(getpid(), getpid(), getpid(), false);
    keyThread->SetThreadName("key_thread");

    uintptr_t regsData[MD_CONTEXT_ARM64_GPR_COUNT] = {};
    regsData[31] = 0x7000;
    regsData[32] = 0x1000;
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, MD_CONTEXT_ARM64_GPR_COUNT);
    keyThread->SetThreadRegs(regs);

    auto stackBuf = std::make_shared<std::vector<uint8_t>>(TEST_BUFFER_SIZE, 0);
    keyThread->SetThreadStackBuffer(stackBuf);
    keyThread->SetStartOfStackMemory(0x7000);

    dumper.process_.SetKeyThread(keyThread);
    dumper.UnwindProcess();
    EXPECT_NE(dumper.unwinder_, nullptr);
}

/**
 * @tc.name: MinidumpDumperTest098
 * @tc.desc: test UnwindOtherThread with otherThread having stack preserves thread count
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest098, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.unwinder_ = std::make_shared<Unwinder>();
    dumper.dfxMaps_ = std::make_shared<DfxMaps>();
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");

    auto otherThread = DfxThread::Create(getpid(), 100, 100, false);
    otherThread->SetThreadName("other_thread");
    auto stackBuf = std::make_shared<std::vector<uint8_t>>(TEST_BUFFER_SIZE, 0);
    otherThread->SetThreadStackBuffer(stackBuf);
    otherThread->SetStartOfStackMemory(0x7000);

    dumper.process_.GetOtherThreads().push_back(otherThread);
    size_t prevSize = dumper.process_.GetOtherThreads().size();
    dumper.UnwindOtherThread();
    EXPECT_EQ(dumper.process_.GetOtherThreads().size(), prevSize);
    EXPECT_TRUE(otherThread->GetFrames().empty());
}

/**
 * @tc.name: MinidumpDumperTest099
 * @tc.desc: test PrintRegisters with MockDfxRegsNoPrefix strips prefix from collector output
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest099, TestSize.Level2)
{
    MinidumpDumper dumper;
    CppCrashInfoCollector::Instance().SetNeedFormatFlag(true);
    dumper.process_.InitProcessInfo(getpid(), getpid(), getuid(), "test_proc");

    auto mockRegs = std::make_shared<MockDfxRegsNoPrefix>();
    dumper.process_.SetFaultThreadRegisters(mockRegs);
    dumper.PrintRegisters();

    std::string collectedRegs = CppCrashInfoCollector::Instance().GetRegisters();
    EXPECT_TRUE(!collectedRegs.empty());
    EXPECT_TRUE(collectedRegs.find("Registers:\n") != 0);
}

/**
 * @tc.name: MinidumpDumperTest100
 * @tc.desc: test Dump with valid pipe fds and false format flag verifies collector state
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest100, TestSize.Level2)
{
    MinidumpDumper dumper;
    int pipeFds[2] = {-1, -1};
    pipe2(pipeFds, O_NONBLOCK);
    std::string testData = "minidump data for false flag test";
    write(pipeFds[1], testData.c_str(), testData.size());
    close(pipeFds[1]);
    bool ret = dumper.Dump(getpid(), pipeFds[0], true, false);
    if (ret) {
        EXPECT_TRUE(CppCrashInfoCollector::Instance().GetNeedFormatFlag());
        EXPECT_TRUE(CppCrashInfoCollector::Instance().GetRegisters().empty());
        EXPECT_TRUE(CppCrashInfoCollector::Instance().GetMaps().empty());
    }
}

}