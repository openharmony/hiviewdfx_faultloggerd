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
#include <map>
#include <set>
#include <string>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_ptrace.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_thread.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxProcessTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: DfxProcessSetGetReason
 * @tc.desc: test DfxProcess setting and getting crash reason
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetReason_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_SetGetReason_001: start.";
    DfxProcess process;
    process.SetReason("SIGSEGV");
    ASSERT_EQ(process.GetReason(), "SIGSEGV");
    process.SetReason("");
    ASSERT_EQ(process.GetReason(), "");
    GTEST_LOG_(INFO) << "DfxProcess_SetGetReason_001: end.";
}

/**
 * @tc.name: DfxProcessSetGetLogSource
 * @tc.desc: test DfxProcess setting and getting log source
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetLogSource_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_SetGetLogSource_002: start.";
    DfxProcess process;
    process.SetLogSource("processdump");
    ASSERT_EQ(process.GetLogSource(), "processdump");
    process.SetLogSource("");
    ASSERT_EQ(process.GetLogSource(), "");
    GTEST_LOG_(INFO) << "DfxProcess_SetGetLogSource_002: end.";
}

/**
 * @tc.name: DfxProcessSetGetVmPid
 * @tc.desc: test DfxProcess setting and getting VM pid
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetVmPid_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_SetGetVmPid_003: start.";
    DfxProcess process;
    process.SetVmPid(1234);
    ASSERT_EQ(process.GetVmPid(), 1234);
    process.SetVmPid(0);
    ASSERT_EQ(process.GetVmPid(), 0);
    GTEST_LOG_(INFO) << "DfxProcess_SetGetVmPid_003: end.";
}

/**
 * @tc.name: DfxProcessSetGetLifeTime
 * @tc.desc: test DfxProcess setting and getting process lifetime
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetLifeTime_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_SetGetLifeTime_004: start.";
    DfxProcess process;
    process.SetLifeTime(5000);
    ASSERT_EQ(process.GetLifeTime(), 5000);
    process.SetLifeTime(0);
    ASSERT_EQ(process.GetLifeTime(), 0);
    GTEST_LOG_(INFO) << "DfxProcess_SetGetLifeTime_004: end.";
}

/**
 * @tc.name: DfxProcessSetGetRss
 * @tc.desc: test DfxProcess setting and getting resident set size
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetRss_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_SetGetRss_005: start.";
    DfxProcess process;
    process.SetRss(1024);
    ASSERT_EQ(process.GetRss(), 1024);
    process.SetRss(0);
    ASSERT_EQ(process.GetRss(), 0);
    GTEST_LOG_(INFO) << "DfxProcess_SetGetRss_005: end.";
}

/**
 * @tc.name: DfxProcessSetGetCrashLogConfig
 * @tc.desc: test DfxProcess setting and getting crash log config
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetCrashLogConfig_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_SetGetCrashLogConfig_006: start.";
    DfxProcess process;
    CrashLogConfig config;
    config.extendPcLrPrinting = true;
    config.simplifyVmaPrinting = true;
    config.logFileCutoffSizeBytes = 100;
    config.mergeAppLog = true;
    config.minidumpLog = true;
    process.SetCrashLogConfig(config);
    const auto& retConfig = process.GetCrashLogConfig();
    ASSERT_EQ(retConfig.extendPcLrPrinting, true);
    ASSERT_EQ(retConfig.simplifyVmaPrinting, true);
    ASSERT_EQ(retConfig.logFileCutoffSizeBytes, 100);
    ASSERT_EQ(retConfig.mergeAppLog, true);
    ASSERT_EQ(retConfig.minidumpLog, true);
    GTEST_LOG_(INFO) << "DfxProcess_SetGetCrashLogConfig_006: end.";
}

/**
 * @tc.name: DfxProcessAppendGetFatalMessage
 * @tc.desc: test DfxProcess appending and getting fatal message
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_AppendGetFatalMessage_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_AppendGetFatalMessage_007: start.";
    DfxProcess process;
    process.AppendFatalMessage("msg1");
    ASSERT_EQ(process.GetFatalMessage(), "msg1");
    process.AppendFatalMessage("msg2");
    ASSERT_EQ(process.GetFatalMessage(), "msg1msg2");
    GTEST_LOG_(INFO) << "DfxProcess_AppendGetFatalMessage_007: end.";
}

/**
 * @tc.name: DfxProcessNativeFramesTable
 * @tc.desc: test DfxProcess adding and getting native frames table
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_NativeFramesTable_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_NativeFramesTable_008: start.";
    DfxProcess process;
    DfxFrame frame;
    frame.pc = 0x1000;
    frame.funcName = "testFunc";
    process.AddNativeFramesTable({0x1000, frame});
    auto& table = process.GetNativeFramesTable();
    ASSERT_EQ(table.size(), 1);
    ASSERT_TRUE(table.find(0x1000) != table.end());
    ASSERT_EQ(table[0x1000].funcName, "testFunc");
    GTEST_LOG_(INFO) << "DfxProcess_NativeFramesTable_008: end.";
}

/**
 * @tc.name: DfxProcessMemoryValues
 * @tc.desc: test DfxProcess setting and getting memory values
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_MemoryValues_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_MemoryValues_009: start.";
    DfxProcess process;
    set<uintptr_t> values = {0x100, 0x200, 0x300};
    process.SetMemoryValues(values);
    const auto& retValues = process.GetMemoryValues();
    ASSERT_EQ(retValues.size(), 3);
    ASSERT_TRUE(retValues.find(0x100) != retValues.end());
    ASSERT_TRUE(retValues.find(0x200) != retValues.end());
    ASSERT_TRUE(retValues.find(0x300) != retValues.end());
    GTEST_LOG_(INFO) << "DfxProcess_MemoryValues_009: end.";
}

/**
 * @tc.name: DfxProcessSetGetFaultThreadRegs
 * @tc.desc: test DfxProcess setting and getting fault thread registers
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetFaultThreadRegs_010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcess_SetGetFaultThreadRegs_010: start.";
    DfxProcess process;
    ASSERT_TRUE(process.GetFaultThreadRegisters() == nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        exit(0);
    }
    ASSERT_TRUE(DfxPtrace::Attach(pid));
    auto regs = DfxRegs::CreateRemoteRegs(pid);
    process.SetFaultThreadRegisters(regs);
    ASSERT_TRUE(process.GetFaultThreadRegisters() != nullptr);
    GTEST_LOG_(INFO) << "DfxProcess_SetGetFaultThreadRegs_010: end.";
}

/**
 * @tc.name: DfxProcessSetGetKeyThread
 * @tc.desc: test DfxProcess setting and getting key thread
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessTest, DfxProcess_SetGetKeyThread_011, TestSize.Level2)
{
    DfxProcess process;
    ASSERT_TRUE(process.GetKeyThread() == nullptr);
    pid_t pid = getpid();
    auto keyThread = DfxThread::Create(pid, pid, pid, false);
    keyThread->SetThreadName("test_key_thread");
    process.SetKeyThread(keyThread);
    ASSERT_TRUE(process.GetKeyThread() != nullptr);
    ASSERT_EQ(process.GetKeyThread()->GetThreadInfo().tid, pid);
    process.SetKeyThread(nullptr);
    ASSERT_TRUE(process.GetKeyThread() == nullptr);
}
}