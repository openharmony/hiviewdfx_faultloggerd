/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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
#include <memory>
#include <string>

#include "dfx_elf_parser.h"
#include "dfx_ptrace.h"
#include "dfx_regs.h"
#include "dfx_regs_get.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_dump_request.h"
#include "dfx_thread.h"
#include "lock_parser.h"
#include <pthread.h>
#include "printer.h"
#include "process_dumper.h"
#include "dfx_unwind_async_thread.h"
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
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
void *SleepThread(void *argv)
{
    int threadID = *(int*)argv;
    printf("create MultiThread %d", threadID);

    const int sleepTime = 10;
    sleep(sleepTime);

    return nullptr;
}

/**
 * @tc.name: DfxProcessTest003
 * @tc.desc: test init other threads
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest003, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DfxProcessTest003: start.";
    DfxProcess process(getpid(), getpid());
    auto ret = process.InitOtherThreads();
    EXPECT_EQ(true, ret) << "DfxProcessTest003 Failed";
    auto threads = process.GetOtherThreads();
    EXPECT_GT(threads.size(), 0) << "DfxProcessTest003 Failed";
    process.ClearOtherThreads();
    threads = process.GetOtherThreads();
    EXPECT_EQ(threads.size(), 0) << "DfxProcessTest003 Failed";
    GTEST_LOG_(INFO) << "DfxProcessTest003: end.";
}

/**
 * @tc.name: DfxProcessTest004
 * @tc.desc: test Attach Detach
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest004: start.";
    DfxProcess process(getpid(), getpid());
    auto ret = process.InitOtherThreads();
    EXPECT_EQ(true, ret) << "DfxProcessTest004 Failed";
    process.Attach();
    process.Detach();
    GTEST_LOG_(INFO) << "DfxProcessTest004: end.";
}

/**
 * @tc.name: DfxProcessTest005
 * @tc.desc: test DfxProcess ChangeTid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest005: start.";
    pid_t pid = getpid();
    DfxProcess process1(pid, pid);
    pid_t ret = process1.ChangeTid(pid, false);
    process1.Attach();
    process1.Detach();
    ASSERT_EQ(ret, pid);
    pthread_t tid;
    int threadID[1] = {1};
    pthread_create(&tid, NULL, SleepThread, &threadID[0]);
    DfxProcess process2(pid, pid);
    ret = process2.ChangeTid(pid, false);
    pthread_join(tid, NULL);
    process2.Attach();
    process2.Detach();
    ASSERT_EQ(ret, pid);
    GTEST_LOG_(INFO) << "DfxProcessTest005: end.";
}

/**
 * @tc.name: DfxProcessTest006
 * @tc.desc: test DfxProcess ChangeTid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest005: start.";
    pid_t pid = getpid();
    DfxProcess process(pid, pid);
    process.processInfo_.pid = pid;
    process.processInfo_.nsPid = 0;
    pid_t id = process.ChangeTid(pid, false);
    process.keyThread_ = DfxThread::Create(pid, pid, pid);
    process.Attach(true);
    DfxPtrace::Detach(pid);
    ASSERT_TRUE(id != 0);
    GTEST_LOG_(INFO) << "DfxProcessTest006: end.";
}

/**
 * @tc.name: DfxThreadTest002
 * @tc.desc: test DfxThread GetThreadRegs
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxThreadTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxThreadTest002: start.";
    int32_t pid = 243, tid = 243;
    DfxThread thread(pid, tid, tid);
    std::shared_ptr<DfxRegs> inputrefs;
    thread.SetThreadRegs(inputrefs);
    std::shared_ptr<DfxRegs> outputrefs = thread.GetThreadRegs();
    EXPECT_EQ(true, inputrefs == outputrefs) << "DfxThreadTest002 Failed";
    GTEST_LOG_(INFO) << "DfxThreadTest002: end.";
}

/**
 * @tc.name: DfxUnwindRemoteTest001
 * @tc.desc: test DfxUnwindRemote UnwindProcess
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxUnwindRemoteTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUnwindRemoteTest001: start.";
    pid_t pid = GetProcessPid(POWERMGR_NAME);
    pid_t tid = pid;
    DfxProcess process(pid, pid);
    Unwinder unwinder(pid);
    process.keyThread_ = std::make_shared<DfxThread>(pid, tid, tid);
    process.keyThread_->Attach();
    process.keyThread_->SetThreadRegs(DfxRegs::CreateRemoteRegs(pid));
    struct ProcessDumpRequest request{};
    bool ret = DfxUnwindRemote::GetInstance().UnwindProcess(request, process, unwinder);
    process.keyThread_->Detach();
    EXPECT_EQ(true, ret) << "DfxUnwindRemoteTest001 Failed";
    GTEST_LOG_(INFO) << "DfxUnwindRemoteTest001: end.";
}

#ifdef UNITTEST
/**
 * @tc.name: DfxUnwindRemoteTest002
 * @tc.desc: test DfxUnwindRemote UnwindProcess
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxUnwindRemoteTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUnwindRemoteTest002: start.";
    pid_t pid = GetProcessPid(POWERMGR_NAME);
    pid_t tid = pid;
    DfxProcess process(pid, pid);
    Unwinder unwinder(pid);
    DfxUnwindRemote remote;
    struct ProcessDumpRequest request{};
    bool ret = remote.UnwindProcess(request, process, unwinder, 0);
    ASSERT_EQ(ret, false);
    process.keyThread_ = std::make_shared<DfxThread>(pid, tid, tid);
    ret = remote.UnwindProcess(request, process, unwinder, 0);
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "DfxUnwindRemoteTest002: end.";
}

/**
 * @tc.name: DfxRingBufferWrapperTest001
 * @tc.desc: test DfxRingBufferWrapper SetWriteBufFd
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxRingBufferWrapperTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRingBufferWrapperTest001: start.";
    auto wrapper = std::make_shared<DfxRingBufferWrapper>();
    wrapper->SetWriteBufFd(-1);
    int ret = wrapper->DefaultWrite(0, nullptr, 0);
    ASSERT_EQ(ret, -1);
    const size_t bufsize = 10;
    char buf[bufsize];
    ret = wrapper->DefaultWrite(1, buf, bufsize);
    ASSERT_NE(ret, -1);
    ret = wrapper->DefaultWrite(-1, buf, bufsize);
    ASSERT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "DfxRingBufferWrapperTest001: end.";
}

/**
 * @tc.name: DfxRingBufferWrapperTest002
 * @tc.desc: test DfxRingBufferWrapper SetWriteBufFd
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxRingBufferWrapperTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRingBufferWrapperTest002: start.";
    auto wrapper = std::make_shared<DfxRingBufferWrapper>();
    wrapper->PrintBaseInfo();
    wrapper->SetWriteBufFd(-1);
    int ret = wrapper->DefaultWrite(0, nullptr, 0);
    ASSERT_EQ(ret, -1);
    GTEST_LOG_(INFO) << "DfxRingBufferWrapperTest002: end.";
}

/**
 * @tc.name: DfxUnwindAsyncThreadTest001
 * @tc.desc: test DfxUnwindAsyncThread GetSubmitterStack
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxUnwindAsyncThreadTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUnwindAsyncThreadTest001: start.";
    Printer printer;
    printer.PrintRegsByConfig(nullptr);
    pid_t pid = GetProcessPid(POWERMGR_NAME);
    pid_t tid = pid;
    DfxThread thread(pid, tid, tid);
    Unwinder unwinder(pid);
    DfxUnwindAsyncThread asyncThread1(thread, unwinder, 0);
    std::vector<DfxFrame> submitterFrames;
    asyncThread1.GetSubmitterStack(submitterFrames);
    ASSERT_EQ(asyncThread1.stackId_, 0);
    DfxUnwindAsyncThread asyncThread2(thread, unwinder, 1);
    asyncThread2.GetSubmitterStack(submitterFrames);
    asyncThread2.UnwindThreadFallback();
    asyncThread2.UnwindThreadByParseStackIfNeed();
    ASSERT_EQ(asyncThread2.stackId_, 1);
    GTEST_LOG_(INFO) << "DfxUnwindAsyncThreadTest001: end.";
}

/**
 * @tc.name: DfxUnwindAsyncThreadTest002
 * @tc.desc: test DfxUnwindAsyncThread GetSubmitterStack
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxUnwindAsyncThreadTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUnwindAsyncThreadTest002: start.";
    pid_t pid = GetProcessPid(POWERMGR_NAME);
    Unwinder unwinder(pid);
    DfxThread thread(pid, pid, pid);
    DfxUnwindAsyncThread asyncThread2(thread, unwinder, 0);
    asyncThread2.UnwindStack(0);
    std::vector<DfxFrame> submitterFrames;
    asyncThread2.GetSubmitterStack(submitterFrames);
    ASSERT_EQ(asyncThread2.stackId_, 0);
    GTEST_LOG_(INFO) << "DfxUnwindAsyncThreadTest002: end.";
}
#endif
}
