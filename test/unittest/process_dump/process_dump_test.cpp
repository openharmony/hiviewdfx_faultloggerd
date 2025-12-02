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
#include "dfx_buffer_writer.h"
#include "dfx_dump_request.h"
#include "dfx_thread.h"
#include "dump_utils.h"
#include <pthread.h>
#include "process_dumper.h"
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
    DfxProcess process;
    process.InitProcessInfo(getpid(), getpid(), getuid(), "");
    pthread_t childThread;
    int threadID[1] = {1};
    pthread_create(&childThread, NULL, SleepThread, &threadID[0]);
    pthread_detach(childThread);
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = gettid(),
        .pid = getpid(),
        .nsPid = getpid(),
    };
    auto ret = process.InitOtherThreads(request);
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
    DfxProcess process;
    process.InitProcessInfo(getpid(), getpid(), getuid(), "");
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = gettid(),
        .pid = getpid(),
        .nsPid = getpid(),
    };
    auto ret = process.InitOtherThreads(request);
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
    DfxProcess process1;
    process1.InitProcessInfo(pid, pid, getuid(), "");
    pid_t ret = process1.ChangeTid(pid, false);
    process1.Attach();
    process1.Detach();
    ASSERT_EQ(ret, pid);
    pthread_t tid;
    int threadID[1] = {1};
    pthread_create(&tid, NULL, SleepThread, &threadID[0]);
    pthread_detach(tid);
    DfxProcess process2;
    process2.InitProcessInfo(pid, pid, getuid(), "");
    ret = process2.ChangeTid(pid, false);
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
    GTEST_LOG_(INFO) << "DfxProcessTest006: start.";
    pid_t pid = getpid();
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "");
    pid_t id = process.ChangeTid(pid, false);
    process.GetKeyThread() = DfxThread::Create(pid, pid, pid);
    process.Attach(true);
    DfxPtrace::Detach(pid);
    ASSERT_TRUE(id != 0);
    GTEST_LOG_(INFO) << "DfxProcessTest006: end.";
}

/**
 * @tc.name: DfxProcessTest007
 * @tc.desc: test DfxProcess ChangeTid
 * @tc.type: FUNC
 */
HWTEST_F (ProcessDumpTest, DfxProcessTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessTest007: start.";
    pid_t curPid = getpid();
    pid_t pid = fork();
    if (pid == 0) {
        sleep(1);
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, WNOHANG);
    DfxProcess process;
    process.InitProcessInfo(curPid, pid, getuid(), "");
    pid_t id = process.ChangeTid(pid, false);
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = pid,
        .pid = pid,
        .nsPid = curPid,
    };
    process.InitKeyThread(request);
    id = process.ChangeTid(pid, false);
    ASSERT_NE(id, 0);
    GTEST_LOG_(INFO) << "DfxProcessTest007: end.";
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
}
