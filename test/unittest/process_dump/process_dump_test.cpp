/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains process dump module unittest. */

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "dfx_regs.h"
#include "dfx_dump_request.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "process_dumper.h"
#include "dfx_unwind_remote.h"
#include "dfx_util.h"
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ProcessDumpTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};
} // namespace HiviewDFX
} // namespace OHOS

void ProcessDumpTest::SetUpTestCase(void)
{
}

void ProcessDumpTest::TearDownTestCase(void)
{
}

void ProcessDumpTest::SetUp(void)
{
}

void ProcessDumpTest::TearDown(void)
{
}

namespace {
/**
 * @tc.name: ProcessDumpTest029
 * @tc.desc: test if signal info is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest029, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest029: start.";
    int32_t input = 1;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsAvailable();
    EXPECT_EQ(true, ret != true) << "ProcessDumpTest029 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest029: end.";
}

/**
 * @tc.name: ProcessDumpTest030
 * @tc.desc: test if addr is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest030, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest030: start.";
    int32_t input = -100;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsAddrAvailable();
    EXPECT_EQ(true, ret != true) << "ProcessDumpTest030 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest030: end.";
}

/**
 * @tc.name: ProcessDumpTest031
 * @tc.desc: test if pid is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest031, TestSize.Level2)
{
    int32_t input = 100;
    GTEST_LOG_(INFO) << "ProcessDumpTest031: start.";
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsPidAvailable();
    EXPECT_EQ(true, ret != true) << "ProcessDumpTest031 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest031: end.";
}

/**
 * @tc.name: ProcessDumpTest032
 * @tc.desc: test if pid is available
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest032, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest032: start.";
    int32_t input = 1;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    int32_t output = signal->GetSignal();
    EXPECT_EQ(true, output == input) << "ProcessDumpTest032 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest032: end.";
}

/**
 * @tc.name: ProcessDumpTest033
 * @tc.desc: test get process id
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest033, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest033: start.";
    int32_t pid = 1, tid = 1;
    ucontext_t context;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid, context);
    pid_t processID = thread->GetProcessId();
    GTEST_LOG_(INFO) << "ProcessDumpTest033: result = " << processID;
    EXPECT_EQ(true, pid == processID) << "ProcessDumpTest033 failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest033: end.";
}

/**
 * @tc.name: ProcessDumpTest034
 * @tc.desc: test get thread id
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest034, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest034: start.";
    int32_t pid = 243, tid = 243;
    ucontext_t context;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid, context);
    pid_t threadId = thread->GetThreadId();
    GTEST_LOG_(INFO) << "ProcessDumpTest034: result = " << threadId;
    EXPECT_EQ(true, tid == threadId) << "ProcessDumpTest034 failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest034: end.";
}

/**
 * @tc.name: ProcessDumpTest035
 * @tc.desc: test get thread name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest035, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest035: start.";
    int32_t pid = 243, tid = 243;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    pid_t threadId = thread->GetThreadId();
    EXPECT_EQ(true, threadId == tid) << "ProcessDumpTest035 failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest035: end.";
}

/**
 * @tc.name: ProcessDumpTest036
 * @tc.desc: test get thread name
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest036, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest036: start.";
    int32_t pid = 1, tid = 1;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    std::string threadName = thread->GetThreadName();
    EXPECT_EQ(true, threadName != "");
    GTEST_LOG_(INFO) << "ProcessDumpTest036: result = " << threadName;
    GTEST_LOG_(INFO) << "ProcessDumpTest036: end.";
}

/**
 * @tc.name: ProcessDumpTest037
 * @tc.desc: test get DfxRegs
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest037, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest037: start.";
    int32_t pid = 243, tid = 243;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    std::shared_ptr<DfxRegs> inputrefs;
    thread->SetThreadRegs(inputrefs);
    std::shared_ptr<DfxRegs> outputrefs = thread->GetThreadRegs();
    EXPECT_EQ(true, inputrefs == outputrefs) << "ProcessDumpTest037 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest037: end.";
}

/**
 * @tc.name: ProcessDumpTest038
 * @tc.desc: test get DfxFrame
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest038, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest038: start.";
    int32_t pid = 243, tid = 243;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    std::shared_ptr<DfxFrame> outputrefs = thread->GetAvailableFrame();
    EXPECT_EQ(true, outputrefs != nullptr) << "ProcessDumpTest038 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest038: end.";
}

/**
 * @tc.name: ProcessDumpTest039
 * @tc.desc: test UnwindProcess
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest039, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest039: start.";
    std::shared_ptr<DfxProcess> process = std::make_shared<DfxProcess>();
    pid_t pid = GetProcessPid(ACCOUNTMGR_NAME);
    pid_t tid = pid;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    const std::vector<std::shared_ptr<DfxThread>> threads = { thread };
    process->SetThreads(threads);
    thread->Attach();
    bool ret = DfxUnwindRemote::GetInstance().UnwindProcess(process);
    thread->Detach();
    EXPECT_EQ(true, ret) << "ProcessDumpTest039 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest039: end.";
}

/**
 * @tc.name: ProcessDumpTest040
 * @tc.desc: test UnwindThread
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, ProcessDumpTest040, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcessDumpTest040: start.";
    std::shared_ptr<DfxProcess> process = std::make_shared<DfxProcess>();
    pid_t pid = GetProcessPid(ACCOUNTMGR_NAME);
    pid_t tid = pid;
    std::shared_ptr<DfxThread> thread = std::make_shared<DfxThread>(pid, tid, tid);
    thread->Attach();
    bool ret = DfxUnwindRemote::GetInstance().UnwindThread(process, thread);
    thread->Detach();
    EXPECT_EQ(true, ret) << "ProcessDumpTest040 Failed";
    GTEST_LOG_(INFO) << "ProcessDumpTest040: end.";
}
}
