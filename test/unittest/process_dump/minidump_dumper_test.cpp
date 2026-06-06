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
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include "cppcrash_info_collector.h"
#include "dfx_buffer_writer.h"
#include "dfx_dump_request.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#include "dump_utils.h"
#include "lite_process_dumper.h"
#include "minidump_dumper.h"
#include "minidump_parser.h"
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

    int tmpFd = open("/data/test/minidump_transfer_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_transfer_invalid", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_parse_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    uintptr_t regsData[33] = {0};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, 33);
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
    uintptr_t regsData[33] = {0};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, 33);
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
    uintptr_t regsData[33] = {0};
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regsData, 33);
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
    int tmpFd = open("/data/test/minidump_cfg_perf_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_exception_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_threadname_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
 * @tc.name: MinidumpDumperTest038
 * @tc.desc: test ParseModuleListStream returns false with null moduleList
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest038, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_module_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
    ASSERT_TRUE(tmpFd > 0);
    std::string invalidData = "not a valid minidump";
    write(tmpFd, invalidData.c_str(), invalidData.size());
    auto input = std::make_shared<std::ifstream>("/proc/self/fd/" + std::to_string(tmpFd), std::ios::binary);
    MinidumpParser parser(input);
    bool ret = dumper.ParseModuleListStream(parser);
    EXPECT_FALSE(ret);
    close(tmpFd);
    unlink("/data/test/minidump_module_test");
}

/**
 * @tc.name: MinidumpDumperTest039
 * @tc.desc: test ParseMemoryListStream returns false with null memoryList
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest039, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_memory_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_threadlist_test", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_threadname_null_key", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_memory_null_regs", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_maplist_null", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
    int tmpFd = open("/data/test/minidump_setup_null_thread", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
 * @tc.desc: test PopulateOtherThreadFromMinidump returns early when dfxThread is null
 * @tc.type: FUNC
 */
HWTEST_F(MinidumpDumperTest, MinidumpDumperTest049, TestSize.Level2)
{
    MinidumpDumper dumper;
    int tmpFd = open("/data/test/minidump_populate_null", O_RDWR | O_CREAT | O_TRUNC, 0644);
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
}