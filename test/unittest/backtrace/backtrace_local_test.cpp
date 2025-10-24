/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <csignal>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <securec.h>
#include <sys/wait.h>
#include <unistd.h>

#include "backtrace_local.h"
#include "backtrace_local_thread.h"
#include "dfx_frame_formatter.h"
#include "dfx_json_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_test_util.h"
#include "elapsed_time.h"
#include "ffrt_inner.h"
#include "procinfo.h"
#include "unwinder_config.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktraceLocalTest"
#define LOG_DOMAIN 0xD002D11
#define DEFAULT_MAX_FRAME_NUM 256
#define MIN_FRAME_NUM 2

class BacktraceLocalTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    uint32_t fdCount;
    uint32_t mapsCount;
    uint64_t memCount;

    static uint32_t fdCountTotal;
    static uint32_t mapsCountTotal;
    static uint64_t memCountTotal;
};

uint32_t BacktraceLocalTest::fdCountTotal = 0;
uint32_t BacktraceLocalTest::mapsCountTotal = 0;
uint64_t BacktraceLocalTest::memCountTotal = 0;


void BacktraceLocalTest::SetUpTestCase()
{
    BacktraceLocalTest::fdCountTotal = GetSelfFdCount();
    BacktraceLocalTest::mapsCountTotal = GetSelfMapsCount();
    BacktraceLocalTest::memCountTotal = GetSelfMemoryCount();
}

void BacktraceLocalTest::TearDownTestCase()
{
    CheckResourceUsage(fdCountTotal, mapsCountTotal, memCountTotal);
}

void BacktraceLocalTest::SetUp()
{
    fdCount = GetSelfFdCount();
    mapsCount = GetSelfMapsCount();
    memCount = GetSelfMemoryCount();
}

void BacktraceLocalTest::TearDown()
{
    CheckResourceUsage(fdCount, mapsCount, memCount);
}

static void RequestMemory(char* msg)
{
    usleep(2000); // 2000 : sleep 2ms
    const int32_t initAllocSz = 11;
    void* p = malloc(initAllocSz);
    int ret = memcpy_s(p, initAllocSz, msg, initAllocSz - 1);
    if (ret < 0) {
        ASSERT_GT(ret, 0);
    }
    free(p);
}

/**
 * @tc.name: BacktraceLocalTest001
 * @tc.desc: test get backtrace of current thread
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest001: start.";
    ElapsedTime counter;
    Unwinder unwinder;
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, thread.Unwind(unwinder, false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    GTEST_LOG_(INFO) << thread.GetFormattedStr();
    thread.SetFrames(frames);
    GTEST_LOG_(INFO) << "BacktraceLocalTest001: end.";
}

int32_t g_tid = 0;
std::mutex g_mutex;
__attribute__((noinline)) void Test002()
{
    printf("Test002\n");
    g_mutex.lock();
    g_mutex.unlock();
}

__attribute__((noinline)) void Test001()
{
    g_tid = gettid();
    printf("Test001:%d\n", g_tid);
    Test002();
}

__attribute__((noinline)) void Test003()
{
    g_mutex.lock();
    printf("Test003:%d\n", gettid());
    std::thread backtraceThread(Test001);
    sleep(1);
    std::string stacktrace = GetProcessStacktrace();
    g_mutex.unlock();
    ASSERT_GT(stacktrace.size(), 0);
    GTEST_LOG_(INFO) << stacktrace;
    backtraceThread.join();
    return;
}

__attribute__((noinline)) void TestMaskSigDumpLocalInner()
{
    printf("TestMaskSigDumpLocalInner\n");
    g_mutex.lock();
    g_mutex.unlock();
}

__attribute__((noinline)) void TestMaskSigDumpLocal()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGLOCAL_DUMP);
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0) {
        printf("pthread sigmask failed, err(%d)\n", errno);
    }
    g_tid = gettid();
    printf("TestMaskSigDumpLocal:%d\n", g_tid);
    TestMaskSigDumpLocalInner();
}

/**
 * @tc.name: BacktraceLocalTest003
 * @tc.desc: test get backtrace of a child thread
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest003: start.";
    g_mutex.lock();
    std::thread backtraceThread(Test001);
    sleep(1);
    if (g_tid <= 0) {
        FAIL() << "Failed to create child thread.\n";
    }

#if defined(__aarch64__) || defined(__loongarch_lp64)
    ElapsedTime counter;
    Unwinder unwinder;
    BacktraceLocalThread thread(g_tid);
    ASSERT_EQ(true, thread.Unwind(unwinder, false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    auto backtraceStr = thread.GetFormattedStr(false);
    ASSERT_GT(backtraceStr.size(), 0);
    GTEST_LOG_(INFO) << "backtraceStr:\n" << backtraceStr;
#endif

    std::string str;
    auto ret = GetBacktraceStringByTid(str, g_tid, 0, false);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "GetBacktraceStringByTid:\n" << str;
    g_mutex.unlock();
    g_tid = 0;
    if (backtraceThread.joinable()) {
        backtraceThread.join();
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest003: end.";
}

/**
 * @tc.name: BacktraceLocalTest004
 * @tc.desc: test get backtrace of a child thread
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest004, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest004: start.";
    g_mutex.lock();
    std::thread backtraceThread(Test001);
    sleep(1);
    if (g_tid <= 0) {
        FAIL() << "Failed to create child thread.\n";
    }

    std::string str;
    auto ret = GetBacktraceStringByTid(str, g_tid, 0, false);
    ASSERT_TRUE(ret);
    string log[] = {"#00", "backtrace_local", "Tid:", "Name"};
    log[2] = log[2] + std::to_string(g_tid);
    int logSize = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(str, log, logSize);
    EXPECT_EQ(count, logSize) << "BacktraceLocalTest004 Failed";
    GTEST_LOG_(INFO) << "GetBacktraceStringByTid:\n" << str;
    g_mutex.unlock();
    g_tid = 0;
    if (backtraceThread.joinable()) {
        backtraceThread.join();
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest004: end.";
}

/**
 * @tc.name: BacktraceLocalTest005
 * @tc.desc: test get backtrace of current process
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest005: start.";
    g_mutex.lock();
    std::thread backtraceThread(Test001);
    sleep(1);
    if (g_tid <= 0) {
        FAIL() << "Failed to create child thread.\n";
    }

    std::string stacktrace = GetProcessStacktrace();
    ASSERT_GT(stacktrace.size(), 0);
    GTEST_LOG_(INFO) << stacktrace;

    if (stacktrace.find("backtrace_local_test") == std::string::npos) {
        FAIL() << "Failed to find pid key word.\n";
    }
    if (stacktrace.find("#01") == std::string::npos) {
        FAIL() << "Failed to find stack key word.\n";
    }

    g_mutex.unlock();
    g_tid = 0;
    if (backtraceThread.joinable()) {
        backtraceThread.join();
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest005: end.";
}

/**
 * @tc.name: BacktraceLocalTest006
 * @tc.desc: test GetTrace C interface
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest006: start.";
    const char* trace = GetTrace();
    std::string stacktrace(trace);
    GTEST_LOG_(INFO) << stacktrace;
    ASSERT_TRUE(stacktrace.find("#00") != std::string::npos);
    ASSERT_TRUE(stacktrace.find("pc") != std::string::npos);
    ASSERT_TRUE(stacktrace.find("backtrace_local_test") != std::string::npos);
    GTEST_LOG_(INFO) << "BacktraceLocalTest006: end.";
}

/**
 * @tc.name: BacktraceLocalTest007
 * @tc.desc: test skip two stack frames and verify stack frame
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest007: start.";
    ElapsedTime counter;
    Unwinder unwinder;
    BacktraceLocalThread oldthread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, oldthread.Unwind(unwinder, false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& oldframes = oldthread.GetFrames();
    ASSERT_GT(oldframes.size(), MIN_FRAME_NUM);
    std::string oldframe = DfxFrameFormatter::GetFrameStr(oldframes[MIN_FRAME_NUM]);
    GTEST_LOG_(INFO) << oldthread.GetFormattedStr();
    BacktraceLocalThread newthread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, newthread.Unwind(unwinder, false, DEFAULT_MAX_FRAME_NUM, MIN_FRAME_NUM));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& newframes = newthread.GetFrames();
    GTEST_LOG_(INFO) << newthread.GetFormattedStr();
    ASSERT_EQ(oldframes.size(), newframes.size() + MIN_FRAME_NUM);
    std::string newframe = DfxFrameFormatter::GetFrameStr(newframes[0]);
    size_t skip = 3; // skip #0x
    ASSERT_EQ(oldframe.erase(0, skip), newframe.erase(0, skip));
    GTEST_LOG_(INFO) << "BacktraceLocalTest007: end.";
}

/**
 * @tc.name: BacktraceLocalTest008
 * @tc.desc: test skip all stack frames
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest008: start.";
    ElapsedTime counter;
    Unwinder unwinder;
    BacktraceLocalThread oldthread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, oldthread.Unwind(unwinder, false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& oldframes = oldthread.GetFrames();
    ASSERT_GT(oldframes.size(), 0);
    size_t oldsize = oldframes.size() - 1;
    GTEST_LOG_(INFO) << oldthread.GetFormattedStr();
    BacktraceLocalThread newthread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, newthread.Unwind(unwinder, false, DEFAULT_MAX_FRAME_NUM, oldsize));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& newframes = newthread.GetFrames();
    GTEST_LOG_(INFO) << newthread.GetFormattedStr();
    ASSERT_EQ(oldframes.size(), newframes.size() + oldsize);
    GTEST_LOG_(INFO) << "BacktraceLocalTest008: end.";
}


/**
 * @tc.name: BacktraceLocalTest009
 * @tc.desc: test skip stack frames exceeding the length
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest009: start.";
    ElapsedTime counter;
    Unwinder unwinder;
    BacktraceLocalThread oldthread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, oldthread.Unwind(unwinder, false, DEFAULT_MAX_FRAME_NUM, -2));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& oldframes = oldthread.GetFrames();
    ASSERT_GT(oldframes.size(), MIN_FRAME_NUM);
    GTEST_LOG_(INFO) << oldthread.GetFormattedStr();
    BacktraceLocalThread newthread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, newthread.Unwind(unwinder, false, DEFAULT_MAX_FRAME_NUM, DEFAULT_MAX_FRAME_NUM));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& newframes = newthread.GetFrames();
    GTEST_LOG_(INFO) << newthread.GetFormattedStr();
    ASSERT_EQ(oldframes.size(), newframes.size());
    GTEST_LOG_(INFO) << "BacktraceLocalTest009: end.";
}

/**
 * @tc.name: BacktraceLocalTest010
 * @tc.desc: test get backtrace of current thread
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest010: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    std::string frameAll;
    GetBacktrace(frameAll, DEFAULT_MAX_FRAME_NUM, false, DEFAULT_MAX_FRAME_NUM);
    GTEST_LOG_(INFO) << "--------all frames begin-----------";
    GTEST_LOG_(INFO) << frameAll;
    GTEST_LOG_(INFO) << "--------all frames end-----------";
    std::string frame;
    ASSERT_EQ(true, GetBacktrace(frame, 0, false, DEFAULT_MAX_FRAME_NUM));
    int start = frame.find("#00");
    int end = frame.find("#01");
    std::string str = frame.substr(start, end - start);
    GTEST_LOG_(INFO) << "frame" << frame;
    GTEST_LOG_(INFO) << "str" << str;
    ASSERT_TRUE(str.find("OHOS::HiviewDFX::GetBacktrace(") != std::string::npos);
    GTEST_LOG_(INFO) << "BacktraceLocalTest010: end.";
}

/**
 * @tc.name: BacktraceLocalTest011
 * @tc.desc: test get thread kernel stack
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest011: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        std::string kernelStack;
        ASSERT_EQ(DfxGetKernelStack(gettid(), kernelStack), 0);
        DfxThreadStack threadStack;
        ASSERT_TRUE(FormatThreadKernelStack(kernelStack, threadStack));
        ASSERT_GT(threadStack.frames.size(), 0);
        for (const auto& frame : threadStack.frames) {
            auto line = DfxFrameFormatter::GetFrameStr(frame);
            ASSERT_NE(line.find("#"), std::string::npos);
            GTEST_LOG_(INFO) << line;
        }
        std::vector<DfxThreadStack> processStack;
        ASSERT_TRUE(FormatProcessKernelStack(kernelStack, processStack));
        for (const auto& stack : processStack) {
            ASSERT_GT(stack.frames.size(), 0);
        }
        std::string formattedStack;
        ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(kernelStack, formattedStack, false));
        ASSERT_GT(formattedStack.size(), 0);
        ASSERT_TRUE(formattedStack.find("Tid:") != std::string::npos) << formattedStack;
        ASSERT_TRUE(formattedStack.find("backtrace_local_test") != std::string::npos) << formattedStack;

        ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(kernelStack, formattedStack, true));
        ASSERT_TRUE(formattedStack.find("\"tid\":") != std::string::npos) << formattedStack;
        ASSERT_TRUE(formattedStack.find("backtrace_local_test") != std::string::npos) << formattedStack;
        GTEST_LOG_(INFO) << "BacktraceLocalTest011: end.";
    }
}

/**
 * @tc.name: BacktraceLocalTest012
 * @tc.desc: test BacktraceLocal abnormal scenario
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest012: start.";
    const int tid = -2;
    Unwinder unwinder;
    BacktraceLocalThread backtrace(tid);
    ASSERT_EQ(backtrace.Unwind(unwinder, false, 0, 0), false);
    std::string str = backtrace.GetFormattedStr(false);
    ASSERT_EQ(str, "");
    GTEST_LOG_(INFO) << "BacktraceLocalTest012: end.";
}

/**
 * @tc.name: BacktraceLocalTest013
 * @tc.desc: Test async-stacktrace api enable in ffrt backtrace
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest013: start.";
    ElapsedTime counter;
    int x = 1;
    const int num = 100;
    Unwinder unwinder;
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
    ffrt::submit([&]{x = num; thread.Unwind(unwinder, false, DEFAULT_MAX_FRAME_NUM, 0);}, {}, {&x});
    ffrt::wait();
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), MIN_FRAME_NUM);
    GTEST_LOG_(INFO) << thread.GetFormattedStr();
    bool ret = false;
    for (auto const& frame : frames) {
        auto line = DfxFrameFormatter::GetFrameStr(frame);
        if (line.find("libffrt.so") != std::string::npos) {
            ret = true;
            break;
        }
        GTEST_LOG_(INFO) << line;
    }
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "BacktraceLocalTest013: end.";
}

#if defined(__aarch64__) || defined(__loongarch_lp64)
/**
 * @tc.name: BacktraceLocalTest014
 * @tc.desc: Test async-stacktrace api enable in ffrt backtrace
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest014: start.";
    ElapsedTime counter;
    int x = 1;
    const int num = 100;
    Unwinder unwinder;
    int tid = -1;
    ffrt::submit([&]{x = num; tid = gettid();}, {}, {&x});
    ffrt::wait();
    ASSERT_GT(tid, 0);
    BacktraceLocalThread thread(tid);
    thread.Unwind(unwinder, false, DEFAULT_MAX_FRAME_NUM, 0);
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), MIN_FRAME_NUM);
    GTEST_LOG_(INFO) << thread.GetFormattedStr();
    bool ret = false;
    for (auto const& frame : frames) {
        auto line = DfxFrameFormatter::GetFrameStr(frame);
        if (line.find("libffrt.so") != std::string::npos) {
            ret = true;
            break;
        }
        GTEST_LOG_(INFO) << line;
    }
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "BacktraceLocalTest014: end.";
}
#endif

/**
* @tc.name: BacktraceLocalTest015
* @tc.desc: Test lock exit after being loacl
* @tc.type: FUNC
*/
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest015: start.";
    const size_t count = 50;
    const size_t msgSize = 10;
    char msg[msgSize] = {'a', 'b', 'c', 'd'};
    for (size_t i = 0; i < count; i++) {
        std::thread shmThread(RequestMemory, msg);
        std::string stacktrace = GetProcessStacktrace();
        ASSERT_GT(stacktrace.size(), 0);
        GTEST_LOG_(INFO) << stacktrace;
        shmThread.detach();
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest015: end.";
}

std::vector<std::string> GetLastLineAddr(const std::string& inputStr, int colNumber)
{
    std::istringstream iss(inputStr);
    std::string line;
    std::vector<std::string> lines;
    std::vector<std::string> results;
    // get lines
    while (std::getline(iss, line)) {
        if (line.find("backtrace_local_test") != std::string::npos) {
            lines.push_back(line);
        }
    }
    // get column
    for (const auto& stackLine : lines) {
        std::istringstream lineIss(stackLine);
        std::string token;
        int tokenCount = 0;
        while (lineIss >> token) {
            tokenCount++;
            if (tokenCount == colNumber) {
                results.push_back(token);
                break;
            }
        }
    }
    return results;
}

void Compare(const std::string& oldStr, const std::string& mixStr, int colNumber)
{
    std::vector<std::string> oldStrAddrs = GetLastLineAddr(oldStr, colNumber);
    std::vector<std::string> mixStrAddrs = GetLastLineAddr(mixStr, colNumber);
    ASSERT_EQ(oldStrAddrs, mixStrAddrs);
}

void CallMixLast(int tid, bool fast, int colNumber)
{
    std::string oldStr;
    bool ret = GetBacktraceStringByTid(oldStr, tid, 0, fast);
    ASSERT_TRUE(ret) << "GetBacktraceStringByTid failed";
    std::string mixStr;
    ret = GetBacktraceStringByTidWithMix(mixStr, tid, 0, fast);
    ASSERT_TRUE(ret) << "GetBacktraceStringByTidWithMix failed";
    GTEST_LOG_(INFO) << "oldStr:" << oldStr;
    GTEST_LOG_(INFO) << "mixStr:" << mixStr;
    Compare(oldStr, mixStr, colNumber);
}

void CallMixFirst(int tid, bool fast, int colNumber)
{
    std::string mixStr;
    bool ret = GetBacktraceStringByTidWithMix(mixStr, tid, 0, fast);
    ASSERT_TRUE(ret) << "GetBacktraceStringByTidWithMix failed";
    std::string oldStr;
    ret = GetBacktraceStringByTid(oldStr, tid, 0, fast);
    ASSERT_TRUE(ret) << "GetBacktraceStringByTid failed";
    GTEST_LOG_(INFO) << "oldStr:" << oldStr;
    GTEST_LOG_(INFO) << "mixStr:" << mixStr;
    Compare(oldStr, mixStr, colNumber);
}

#if defined(__arm__) || defined(__aarch64__)
/**
 * @tc.name: BacktraceLocalTest016
 * @tc.desc: test backtrace other thread
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest016, TestSize.Level2)
{
    g_mutex.lock();
    std::thread backtraceTread(Test001);
    sleep(1);
    if (g_tid <= 0) {
        FAIL() << "Failed to create child thread.\n";
    }
    CallMixLast(g_tid, false, 3);
    CallMixFirst(g_tid, false, 3);
    CallMixLast(g_tid, true, 3);
    CallMixFirst(g_tid, true, 3);
    g_mutex.unlock();
    g_tid = 0;
    if (backtraceTread.joinable()) {
        backtraceTread.join();
    }
}
#endif

/**
 * @tc.name: BacktraceLocalTest017
 * @tc.desc: test FormatProcessKernelStack
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest017: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        pid_t pid = GetProcessPid(FOUNDATION_NAME);
        std::vector<int> tids;
        std::vector<int> nstids;
        ASSERT_TRUE(GetTidsByPid(pid, tids, nstids));
        std::string processKernelStackInfo;
        for (const auto& tid : tids) {
            std::string kernelStack;
            ASSERT_EQ(DfxGetKernelStack(tid, kernelStack), 0);
            processKernelStackInfo += kernelStack;
        }
        std::vector<DfxThreadStack> processStack;
        ASSERT_TRUE(FormatProcessKernelStack(processKernelStackInfo, processStack));
        for (const auto& threadStack : processStack) {
            ASSERT_GT(threadStack.frames.size(), 0);
            for (auto const& frame : threadStack.frames) {
                auto line = DfxFrameFormatter::GetFrameStr(frame);
                ASSERT_NE(line.find("#"), std::string::npos);
                GTEST_LOG_(INFO) << line;
            }
        }
        GTEST_LOG_(INFO) << "BacktraceLocalTest017: end.";
    }
}

/**
 * @tc.name: BacktraceLocalTest018
 * @tc.desc: test backtrace using kernel stack
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest018: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        g_mutex.lock();
        std::thread backtraceThread(TestMaskSigDumpLocal);
        sleep(1);
        if (g_tid <= 0) {
            FAIL() << "Failed to create child thread.\n";
        }
        std::string str;
        ASSERT_TRUE(GetBacktraceStringByTid(str, g_tid, 0, false)) << str;
        ASSERT_TRUE(GetBacktraceStringByTidWithMix(str, g_tid, 0, false)) << str;
        ASSERT_TRUE(!GetProcessStacktrace().empty());
        g_mutex.unlock();
        g_tid = 0;
        if (backtraceThread.joinable()) {
            backtraceThread.join();
        }
        GTEST_LOG_(INFO) << "BacktraceLocalTest018: end.";
    }
}

/**
 * @tc.name: BacktraceLocalTest019
 * @tc.desc: test get backtrace of current process
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest019, TestSize.Level2)
{
    std::thread backtraceThread(Test003);
    backtraceThread.join();
    ASSERT_GT(g_tid, 0) << "Failed to create child thread.\n";
    g_tid = 0;
}

/**
 * @tc.name: BacktraceLocalTest020
 * @tc.desc: test get thread kernel stack with ark
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest020: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        std::string kernelStack;
        ASSERT_EQ(DfxGetKernelStack(gettid(), kernelStack, true), 0);
        GTEST_LOG_(INFO) << "BacktraceLocalTest020: end.";
    }
}

/**
 * @tc.name: BacktraceLocalTest021
 * @tc.desc: test get FormatThreadKernelStack with parser
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest021: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        std::string kernelStack;
        ASSERT_EQ(DfxGetKernelStack(gettid(), kernelStack), 0);
        DfxThreadStack threadStack;
        auto parser = std::make_unique<DfxOfflineParser>("");
        ASSERT_TRUE(FormatThreadKernelStack(kernelStack, threadStack, parser.get()));
        ASSERT_GT(threadStack.frames.size(), 0);
        for (const auto& frame : threadStack.frames) {
            auto line = DfxFrameFormatter::GetFrameStr(frame);
            ASSERT_NE(line.find("#"), std::string::npos);
            ASSERT_NE(line.find("("), std::string::npos);
            GTEST_LOG_(INFO) << line;
        }
        GTEST_LOG_(INFO) << "BacktraceLocalTest021: end.";
    }
}

/**
 * @tc.name: BacktraceLocalTest022
 * @tc.desc: test FormatProcessKernelStack with parsesymbol
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest022: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        pid_t pid = GetProcessPid(FOUNDATION_NAME);
        std::vector<int> tids;
        std::vector<int> nstids;
        ASSERT_TRUE(GetTidsByPid(pid, tids, nstids));
        std::string processKernelStackInfo;
        for (const auto& tid : tids) {
            std::string kernelStack;
            ASSERT_EQ(DfxGetKernelStack(tid, kernelStack), 0);
            processKernelStackInfo += kernelStack;
        }
        std::vector<DfxThreadStack> processStack;
        ASSERT_TRUE(FormatProcessKernelStack(processKernelStackInfo, processStack, true));
        for (const auto& threadStack : processStack) {
            ASSERT_GT(threadStack.frames.size(), 0);
            for (auto const& frame : threadStack.frames) {
                auto line = DfxFrameFormatter::GetFrameStr(frame);
                ASSERT_NE(line.find("#"), std::string::npos);
                ASSERT_NE(line.find("("), std::string::npos);
                GTEST_LOG_(INFO) << line;
            }
        }
        GTEST_LOG_(INFO) << "BacktraceLocalTest022: end.";
    }
}

/**
 * @tc.name: BacktraceLocalTest023
 * @tc.desc: test FormatKernelStack with parsesymbol
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest023: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        std::string kernelStack;
        ASSERT_EQ(DfxGetKernelStack(gettid(), kernelStack), 0);
        std::string formattedStack;
        ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(kernelStack, formattedStack, false, true));
        ASSERT_GT(formattedStack.size(), 0);
        ASSERT_TRUE(formattedStack.find("Tid:") != std::string::npos) << formattedStack;
        ASSERT_TRUE(formattedStack.find("backtrace_local_test") != std::string::npos) << formattedStack;
        ASSERT_TRUE(formattedStack.find("OHOS::HiviewDFX::DfxGetKernelStack") != std::string::npos) << formattedStack;
        GTEST_LOG_(INFO) << "BacktraceLocalTest023: end.";
    }
}
} // namespace HiviewDFX
} // namepsace OHOS
