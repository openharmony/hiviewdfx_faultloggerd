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
#include "dfx_test_util.h"
#include "elapsed_time.h"
#include "file_util.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktraceLocalTest"
#define LOG_DOMAIN 0xD002D11

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
    ASSERT_EQ(true, thread.Unwind(0));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    for (const auto& frame : frames) {
        GTEST_LOG_(INFO) << DfxFrameFormatter::GetFrameStr(frame);
    }
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
    ASSERT_EQ(true, thread.Unwind(0));
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
} // namespace HiviewDFX
} // namepsace OHOS
