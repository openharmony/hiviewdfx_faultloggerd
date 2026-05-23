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

#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "reporter.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ReporterTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: ReporterTest001
 * @tc.desc: test SysEventReporter reporting cpp crash event
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, SysEventReporter_CppCrash_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SysEventReporter_CppCrash_001: start.";
    SysEventReporter reporter(ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_reporter_cpp");
    ASSERT_EQ(process.GetProcessInfo().pid, pid);
    ASSERT_EQ(process.GetProcessInfo().processName, "test_reporter_cpp");
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    request.pid = pid;
    request.uid = getuid();
    reporter.Report(process, request);
    ASSERT_EQ(process.GetProcessInfo().pid, pid);
    GTEST_LOG_(INFO) << "SysEventReporter_CppCrash_001: end.";
}

/**
 * @tc.name: ReporterTest002
 * @tc.desc: test SysEventReporter reporting fdsan event
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, SysEventReporter_Fdsan_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SysEventReporter_Fdsan_002: start.";
    SysEventReporter reporter(ProcessDumpType::DUMP_TYPE_FDSAN);
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_reporter_fdsan");
    process.SetReason("FDSAN");
    ASSERT_EQ(process.GetReason(), "FDSAN");
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_FDSAN;
    request.pid = pid;
    request.uid = getuid();
    reporter.Report(process, request);
    ASSERT_EQ(process.GetReason(), "FDSAN");
    GTEST_LOG_(INFO) << "SysEventReporter_Fdsan_002: end.";
}

/**
 * @tc.name: ReporterTest003
 * @tc.desc: test SysEventReporter reporting jemalloc event
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, SysEventReporter_Jemalloc_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SysEventReporter_Jemalloc_003: start.";
    SysEventReporter reporter(ProcessDumpType::DUMP_TYPE_JEMALLOC);
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_reporter_jemalloc");
    process.SetReason("JEMALLOC");
    ASSERT_EQ(process.GetReason(), "JEMALLOC");
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_JEMALLOC;
    request.pid = pid;
    request.uid = getuid();
    reporter.Report(process, request);
    ASSERT_EQ(process.GetReason(), "JEMALLOC");
    GTEST_LOG_(INFO) << "SysEventReporter_Jemalloc_003: end.";
}

/**
 * @tc.name: ReporterTest004
 * @tc.desc: test SysEventReporter reporting bad file descriptor event
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, SysEventReporter_BadFd_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SysEventReporter_BadFd_004: start.";
    SysEventReporter reporter(ProcessDumpType::DUMP_TYPE_BADFD);
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_reporter_badfd");
    process.SetReason("BADFD");
    ASSERT_EQ(process.GetReason(), "BADFD");
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_BADFD;
    request.pid = pid;
    request.uid = getuid();
    reporter.Report(process, request);
    ASSERT_EQ(process.GetReason(), "BADFD");
    GTEST_LOG_(INFO) << "SysEventReporter_BadFd_004: end.";
}

/**
 * @tc.name: ReporterTest005
 * @tc.desc: test SysEventReporter reporting memory leak event
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, SysEventReporter_MemLeak_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SysEventReporter_MemLeak_005: start.";
    SysEventReporter reporter(ProcessDumpType::DUMP_TYPE_MEM_LEAK);
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_reporter_memleak");
    ASSERT_EQ(process.GetProcessInfo().pid, pid);
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_MEM_LEAK;
    request.pid = pid;
    request.uid = getuid();
    reporter.Report(process, request);
    ASSERT_EQ(process.GetProcessInfo().pid, pid);
    GTEST_LOG_(INFO) << "SysEventReporter_MemLeak_005: end.";
}

/**
 * @tc.name: ReporterTest006
 * @tc.desc: test CppCrashReporter reporting non-hiview process crash
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, CppCrashReporter_NonHiview_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CppCrashReporter_NonHiview_006: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_app");
    ASSERT_EQ(process.GetProcessInfo().processName, "test_app");
    ProcessDumpRequest request = {};
    request.pid = pid;
    request.uid = getuid();
    request.timeStamp = 0;
    CppCrashReporter cppReporter;
    cppReporter.Report(process, request);
    ASSERT_EQ(process.GetProcessInfo().processName, "test_app");
    GTEST_LOG_(INFO) << "CppCrashReporter_NonHiview_006: end.";
}

/**
 * @tc.name: ReporterTest007
 * @tc.desc: test AddrSanitizerReporter with BADFD reason
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, AddrSanitizerReporter_BadFdReason_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_BadFdReason_007: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_addr_badfd");
    process.SetReason("BADFD");
    ASSERT_TRUE(process.GetReason().find("BADFD") != string::npos);
    ProcessDumpRequest request = {};
    request.pid = pid;
    request.uid = getuid();
    request.timeStamp = 0;
    AddrSanitizerReporter addrReporter;
    addrReporter.Report(process, request);
    ASSERT_TRUE(process.GetReason().find("BADFD") != string::npos);
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_BadFdReason_007: end.";
}

/**
 * @tc.name: ReporterTest008
 * @tc.desc: test AddrSanitizerReporter with JEMALLOC reason
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, AddrSanitizerReporter_JemallocReason_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_JemallocReason_008: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_addr_jemalloc");
    process.SetReason("JEMALLOC");
    ASSERT_TRUE(process.GetReason().find("JEMALLOC") != string::npos);
    ProcessDumpRequest request = {};
    request.pid = pid;
    request.uid = getuid();
    request.timeStamp = 0;
    AddrSanitizerReporter addrReporter;
    addrReporter.Report(process, request);
    ASSERT_TRUE(process.GetReason().find("JEMALLOC") != string::npos);
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_JemallocReason_008: end.";
}

/**
 * @tc.name: ReporterTest009
 * @tc.desc: test AddrSanitizerReporter with SIGSEGV reason
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, AddrSanitizerReporter_OtherReason_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_OtherReason_009: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_addr_other");
    process.SetReason("SIGSEGV");
    ASSERT_TRUE(process.GetReason().find("SIGSEGV") != string::npos);
    ProcessDumpRequest request = {};
    request.pid = pid;
    request.uid = getuid();
    request.timeStamp = 0;
    AddrSanitizerReporter addrReporter;
    addrReporter.Report(process, request);
    ASSERT_TRUE(process.GetReason().find("SIGSEGV") != string::npos);
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_OtherReason_009: end.";
}

/**
 * @tc.name: ReporterTest010
 * @tc.desc: test AddrSanitizerReporter with empty reason
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, AddrSanitizerReporter_EmptyReason_010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_EmptyReason_010: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_addr_empty");
    process.SetReason("");
    ASSERT_TRUE(process.GetReason().empty());
    ProcessDumpRequest request = {};
    request.pid = pid;
    request.uid = getuid();
    request.timeStamp = 0;
    AddrSanitizerReporter addrReporter;
    addrReporter.Report(process, request);
    ASSERT_TRUE(process.GetReason().empty());
    GTEST_LOG_(INFO) << "AddrSanitizerReporter_EmptyReason_010: end.";
}

/**
 * @tc.name: ReporterTest011
 * @tc.desc: test CppCrashReporter generating crash summary string
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, CppCrashReporter_GetSummary_011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CppCrashReporter_GetSummary_011: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_summary");
    process.AppendFatalMessage("test fatal msg");
    ProcessDumpRequest request = {};
    request.pid = pid;
    process.InitKeyThread(request);
    Unwinder unwinder;
    unwinder.UnwindLocal();
    process.GetKeyThread()->SetFrames(unwinder.GetFrames());

    CppCrashReporter cppReporter;
    std::string summary = cppReporter.GetSummary(process);
    GTEST_LOG_(INFO) << summary;
    ASSERT_FALSE(summary.empty());
    GTEST_LOG_(INFO) << "CppCrashReporter_GetSummary_011: end.";
}

/**
 * @tc.name: ReporterTest012
 * @tc.desc: test CppCrashReporter formatting registers as string
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, CppCrashReporter_GetRegsString_012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CppCrashReporter_GetRegsString_012: start.";
    CppCrashReporter cppReporter;
    string regsNull = cppReporter.GetRegsString(nullptr);
    ASSERT_TRUE(regsNull.empty());

    pid_t pid = getpid();
    auto regs = DfxRegs::CreateRemoteRegs(pid);
    if (regs != nullptr) {
        string regsStr = cppReporter.GetRegsString(regs);
        ASSERT_FALSE(regsStr.empty());
        ASSERT_TRUE(regsStr.find("Registers:\n") == string::npos);
    }
    GTEST_LOG_(INFO) << "CppCrashReporter_GetRegsString_012: end.";
}

/**
 * @tc.name: ReporterTest013
 * @tc.desc: test CppCrashReporter reporting hiview process crash
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, CppCrashReporter_HiviewCrash_013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CppCrashReporter_HiviewCrash_013: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "/system/bin/hiview");
    ASSERT_EQ(process.GetProcessInfo().processName, "/system/bin/hiview");
    ProcessDumpRequest request = {};
    request.pid = pid;
    request.uid = getuid();
    CppCrashReporter cppReporter;
    cppReporter.Report(process, request);
    ASSERT_EQ(process.GetProcessInfo().processName, "/system/bin/hiview");
    GTEST_LOG_(INFO) << "CppCrashReporter_HiviewCrash_013: end.";
}

/**
 * @tc.name: ReporterTest014
 * @tc.desc: test reporting crash to ability manager service for native process
 * @tc.type: FUNC
 */
HWTEST_F(ReporterTest, ReportToAMS_NativeProcess_014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ReportToAMS_NativeProcess_014: start.";
    ProcessDumpRequest request = {};
    request.uid = 0;
    request.pid = getpid();
    ASSERT_TRUE(request.uid < MIN_HAP_UID);
    DfxProcess process;
    ReportToAbilityManagerService(process, request);
    ASSERT_EQ(request.uid, 0);
    GTEST_LOG_(INFO) << "ReportToAMS_NativeProcess_014: end.";
}
}