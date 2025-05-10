/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <vector>

#include "dfx_buffer_writer.h"
#include "decorative_dump_info.h"
#include "dfx_process.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "key_thread_dump_info.h"
#include "procinfo.h"
#include "multithread_constructor.h"


using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ThreadDumpInfoTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
    static int WriteLogFunc(int32_t fd, const char *buf, int len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string ThreadDumpInfoTest::result = "";

void ThreadDumpInfoTest::SetUpTestCase(void)
{
    result = "";
}

void ThreadDumpInfoTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(ThreadDumpInfoTest::WriteLogFunc);
}

int ThreadDumpInfoTest::WriteLogFunc(int32_t fd, const char *buf, int len)
{
    ThreadDumpInfoTest::result.append(std::string(buf, len));
    return 0;
}

namespace {
pid_t g_childTid = 0;
void *SleepThread(void *argv)
{
    int threadID = *(int*)argv;
    printf("create MultiThread %d", threadID);

    const int sleepTime = 10;
    g_childTid = gettid();
    sleep(sleepTime);
    return nullptr;
}

/**
 * @tc.name: ThreadDumpInfoTest001
 * @tc.desc: test KeyThreadDumpInfo dumpcatch
 * @tc.type: FUNC
 */
HWTEST_F(ThreadDumpInfoTest, ThreadDumpInfoTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest001: start.";
    // dumpcatch -p
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    pid_t tid = pid;
    pid_t nsPid = pid;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
    };
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.SetVmPid(pid);
    process.InitKeyThread(request);
    process.GetKeyThread()->SetThreadRegs(DfxRegs::CreateRemoteRegs(pid)); // can not get context, so create regs self
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    unwinder.EnableFillFrames(false);
    KeyThreadDumpInfo dumpInfo;
    EXPECT_GT(dumpInfo.UnwindStack(process, unwinder), 0);
    dumpInfo.Symbolize(process, unwinder);
    dumpInfo.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Tid:",
        to_string(tid),
        "Name:",
        "#00",
        "#01",
    };
    for (const std::string& keyWord : keyWords) {
        EXPECT_TRUE(CheckContent(result, keyWord, true));
    }
    process.Detach();
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest001: end.";
}

/**
 * @tc.name: ThreadDumpInfoTest002
 * @tc.desc: test KeyThreadDumpInfo dumpcatch single thread
 * @tc.type: FUNC
 */
HWTEST_F(ThreadDumpInfoTest, ThreadDumpInfoTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest002: start.";
    // dumpcatch -p -t
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        pthread_t childThread;
        int threadID[1] = {1};
        pthread_create(&childThread, NULL, SleepThread, &threadID[0]);
        pthread_detach(childThread);
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    sleep(1);
    pid_t tid = pid;
    pid_t nsPid = pid;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
    };
    std::vector<int> tids;
    std::vector<int> nstids;
    EXPECT_TRUE(GetTidsByPid(pid, tids, nstids));
    request.siginfo.si_value.sival_int = nstids[nstids.size() - 1];
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.SetVmPid(pid);
    process.InitKeyThread(request);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    unwinder.EnableFillFrames(false);
    KeyThreadDumpInfo dumpInfo;
    EXPECT_GT(dumpInfo.UnwindStack(process, unwinder), 0);
    dumpInfo.Symbolize(process, unwinder);
    dumpInfo.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Tid:",
        to_string(g_childTid),
        "Name:",
        "#00",
        "#01",
    };
    for (const std::string& keyWord : keyWords) {
        EXPECT_TRUE(CheckContent(result, keyWord, true));
    }
    process.Detach();
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest002: end.";
}

/**
 * @tc.name: ThreadDumpInfoTest003
 * @tc.desc: test KeyThreadDumpInfo cppcrash
 * @tc.type: FUNC
 */
HWTEST_F(ThreadDumpInfoTest, ThreadDumpInfoTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest003: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    pid_t tid = pid;
    pid_t nsPid = pid;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_CPP_CRASH,
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
    };
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.SetVmPid(pid);
    process.InitKeyThread(request);
    process.GetKeyThread()->SetThreadRegs(DfxRegs::CreateRemoteRegs(pid)); // can not get context, so create regs self
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    unwinder.EnableFillFrames(false);
    KeyThreadDumpInfo dumpInfo;
    EXPECT_GT(dumpInfo.UnwindStack(process, unwinder), 0);
    dumpInfo.Symbolize(process, unwinder);
    dumpInfo.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Fault thread info:",
        "Tid:",
        to_string(tid),
        "#00",
        "#01",
    };
    for (const std::string& keyWord : keyWords) {
        EXPECT_TRUE(CheckContent(result, keyWord, true));
    }
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest003: end.";
}

/**
 * @tc.name: ThreadDumpInfoTest004
 * @tc.desc: test other thread dump info in cppcrash type
 * @tc.type: FUNC
 */
HWTEST_F(ThreadDumpInfoTest, ThreadDumpInfoTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest004: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    pid_t pid = testProcess;
    pid_t tid = testProcess;
    pid_t nsPid = testProcess;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_CPP_CRASH,
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
    };
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.SetVmPid(pid);
    process.InitOtherThreads(0);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    unwinder.EnableFillFrames(false);
    OtherThreadDumpInfo dumpInfo;
    EXPECT_GT(dumpInfo.UnwindStack(process, unwinder), 0);
    dumpInfo.Symbolize(process, unwinder);
    dumpInfo.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Other thread info:",
        "Tid:",
        "#00",
        "#01",
    };
    for (const std::string& keyWord : keyWords) {
        EXPECT_TRUE(CheckContent(result, keyWord, true));
    }
    process.Detach();
    kill(pid, SIGKILL);
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest004: end.";
}

/**
 * @tc.name: ThreadDumpInfoTest005
 * @tc.desc: test other thread dump info in dumpcatch type
 * @tc.type: FUNC
 */
HWTEST_F(ThreadDumpInfoTest, ThreadDumpInfoTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest005: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    pid_t pid = testProcess;
    pid_t tid = testProcess;
    pid_t nsPid = testProcess;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
    };
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.SetVmPid(pid);
    process.InitOtherThreads(0);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    unwinder.EnableFillFrames(false);
    OtherThreadDumpInfo dumpInfo;
    EXPECT_GT(dumpInfo.UnwindStack(process, unwinder), 0);
    dumpInfo.Symbolize(process, unwinder);
    dumpInfo.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Tid:",
        "#00",
        "#01",
    };
    for (const std::string& keyWord : keyWords) {
        EXPECT_TRUE(CheckContent(result, keyWord, true));
    }
    process.Detach();
    kill(pid, SIGKILL);
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest005: end.";
}

/**
 * @tc.name: ThreadDumpInfoTest006
 * @tc.desc: test all thread dump info
 * @tc.type: FUNC
 */
HWTEST_F(ThreadDumpInfoTest, ThreadDumpInfoTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest006: start.";
    int threadCount = 10;
    pid_t pid = CreateMultiThreadProcess(threadCount);
    sleep(1);
    pid_t nsPid = pid;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = pid,
        .pid = pid,
        .nsPid = pid,
    };
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.InitKeyThread(request);
    process.SetVmPid(pid);
    process.GetKeyThread()->SetThreadRegs(DfxRegs::CreateRemoteRegs(pid)); // can not get context, so create regs self
    process.InitOtherThreads(pid);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    unwinder.EnableFillFrames(false);
    std::shared_ptr<DumpInfo> keyThreadDumpInfo = std::make_shared<KeyThreadDumpInfo>();
    OtherThreadDumpInfo dumpInfo;
    dumpInfo.SetDumpInfo(keyThreadDumpInfo);
    EXPECT_GT(dumpInfo.UnwindStack(process, unwinder), threadCount);
    dumpInfo.Symbolize(process, unwinder);
    dumpInfo.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Tid:",
        to_string(pid),
        "#00",
        "#01",
    };
    for (const std::string& keyWord : keyWords) {
        EXPECT_TRUE(CheckContent(result, keyWord, true));
    }
    process.Detach();
    kill(pid, SIGKILL);
    GTEST_LOG_(INFO) << "ThreadDumpInfoTest006: end.";
}
}
