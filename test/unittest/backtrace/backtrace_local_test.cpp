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
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <securec.h>
#include <sys/wait.h>
#include <unistd.h>

#include "backtrace_local.h"
#include "backtrace_local_thread.h"
#include "dfx_frame_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_test_util.h"
#include "elapsed_time.h"
#include "ffrt_inner.h"

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

/**
 * @tc.name: BacktraceLocalTest001
 * @tc.desc: test get backtrace of current thread
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest001: start.";
    ElapsedTime counter;
    auto unwinder = std::make_shared<Unwinder>();
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD, unwinder);
    ASSERT_EQ(true, thread.Unwind(false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    GTEST_LOG_(INFO) << thread.GetFormattedStr();
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

    ElapsedTime counter;
    auto unwinder = std::make_shared<Unwinder>();
    BacktraceLocalThread thread(g_tid, unwinder);
    ASSERT_EQ(true, thread.Unwind(false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    auto backtraceStr = thread.GetFormattedStr(false);
    ASSERT_GT(backtraceStr.size(), 0);
    GTEST_LOG_(INFO) << "backtraceStr:\n" << backtraceStr;

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
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest004, TestSize.Level2)
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
    auto unwinder = std::make_shared<Unwinder>();
    BacktraceLocalThread oldthread(BACKTRACE_CURRENT_THREAD, unwinder);
    ASSERT_EQ(true, oldthread.Unwind(false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& oldframes = oldthread.GetFrames();
    ASSERT_GT(oldframes.size(), MIN_FRAME_NUM);
    std::string oldframe = DfxFrameFormatter::GetFrameStr(oldframes[MIN_FRAME_NUM]);
    GTEST_LOG_(INFO) << oldthread.GetFormattedStr();
    BacktraceLocalThread newthread(BACKTRACE_CURRENT_THREAD, unwinder);
    ASSERT_EQ(true, newthread.Unwind(false, DEFAULT_MAX_FRAME_NUM, MIN_FRAME_NUM));
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
    auto unwinder = std::make_shared<Unwinder>();
    BacktraceLocalThread oldthread(BACKTRACE_CURRENT_THREAD, unwinder);
    ASSERT_EQ(true, oldthread.Unwind(false));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& oldframes = oldthread.GetFrames();
    ASSERT_GT(oldframes.size(), 0);
    size_t oldsize = oldframes.size() - 1;
    GTEST_LOG_(INFO) << oldthread.GetFormattedStr();
    BacktraceLocalThread newthread(BACKTRACE_CURRENT_THREAD, unwinder);
    ASSERT_EQ(true, newthread.Unwind(false, DEFAULT_MAX_FRAME_NUM, oldsize));
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
    auto unwinder = std::make_shared<Unwinder>();
    BacktraceLocalThread oldthread(BACKTRACE_CURRENT_THREAD, unwinder);
    ASSERT_EQ(true, oldthread.Unwind(false, DEFAULT_MAX_FRAME_NUM, -1));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& oldframes = oldthread.GetFrames();
    ASSERT_GT(oldframes.size(), MIN_FRAME_NUM);
    GTEST_LOG_(INFO) << oldthread.GetFormattedStr();
    BacktraceLocalThread newthread(BACKTRACE_CURRENT_THREAD, unwinder);
    ASSERT_EQ(true, newthread.Unwind(false, DEFAULT_MAX_FRAME_NUM, DEFAULT_MAX_FRAME_NUM));
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
    std::string frame;
    ASSERT_EQ(true, GetBacktrace(frame, 0, false, DEFAULT_MAX_FRAME_NUM));
    int start = frame.find("#00");
    int end = frame.find("#01");
    std::string str = frame.substr(start, end - start);
    GTEST_LOG_(INFO) << "frame" << frame;
    GTEST_LOG_(INFO) << "str" << str;
    std::string keyword = "libbacktrace_local.so";
#if defined(BACKTRACE_LOCAL_TEST_STATIC)
    keyword = "backtrace_local_test_static";
#endif
    ASSERT_TRUE(str.find(keyword) != std::string::npos);
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
        return;
    }
    std::string kernelStack;
    ASSERT_EQ(DfxGetKernelStack(gettid(), kernelStack), 0);
    DfxThreadStack threadStack;
    ASSERT_TRUE(FormatThreadKernelStack(kernelStack, threadStack));
    ASSERT_GT(threadStack.frames.size(), 0);
    for (auto const& frame : threadStack.frames) {
        auto line = DfxFrameFormatter::GetFrameStr(frame);
        ASSERT_NE(line.find("#"), std::string::npos);
        GTEST_LOG_(INFO) << line;
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest011: end.";
}

/**
 * @tc.name: BacktraceLocalTest012
 * @tc.desc: test BacktraceLocal abnormal scenario
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest012: start.";
    std::shared_ptr<Unwinder> unwinder1 = nullptr;
    const int tid = -2;
    BacktraceLocalThread backtrace1(tid, unwinder1);
    bool ret = backtrace1.Unwind(false, 0, 0);
    ASSERT_EQ(ret, false);
    std::shared_ptr<Unwinder> unwinder2 = std::make_shared<Unwinder>();
    BacktraceLocalThread backtrace2(tid, unwinder2);
    ret = backtrace2.Unwind(false, 0, 0);
    ASSERT_EQ(ret, false);
    std::string str = backtrace2.GetFormattedStr(false);
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
    auto unwinder = std::make_shared<Unwinder>();
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD, unwinder);
    ffrt::submit([&]{x = num; thread.Unwind(false, DEFAULT_MAX_FRAME_NUM, 0);}, {}, {&x});
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
    auto unwinder = std::make_shared<Unwinder>();
    int tid = -1;
    ffrt::submit([&]{x = num; tid = gettid();}, {}, {&x});
    ffrt::wait();
    ASSERT_GT(tid, 0);
    BacktraceLocalThread thread(tid, unwinder);
    thread.Unwind(false, DEFAULT_MAX_FRAME_NUM, 0);
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
} // namespace HiviewDFX
} // namepsace OHOS
