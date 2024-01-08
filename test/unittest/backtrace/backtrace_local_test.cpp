/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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
#include <sys/wait.h>
#include <unistd.h>

#include "file_util.h"
#include <libunwind_i-ohos.h>
#include <libunwind.h>
#include <securec.h>

#include "dfx_frame_format.h"
#include "backtrace_local.h"
#include "backtrace_local_context.h"
#include "backtrace_local_thread.h"
#include "dfx_symbols.h"
#include "elapsed_time.h"
#include "dfx_test_util.h"

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

size_t PrintBacktraceByLibunwind()
{
    unw_cursor_t cursor;
    unw_word_t ip;
    size_t n = 0;
    unw_context_t uc;
    unw_getcontext (&uc);
    if (unlikely (unw_init_local (&cursor, &uc) < 0)) {
        return 0;
    }

    while (unw_step (&cursor) > 0) {
        if (unw_get_reg (&cursor, UNW_REG_IP, &ip) || ip == 0) {
            return n;
        }
        n++;
        printf("current pc:%llx \n", static_cast<unsigned long long>(ip));
    }
    return n;
}

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
    ASSERT_GT(PrintBacktraceByLibunwind(), 0);
    fdCount = GetSelfFdCount();
    mapsCount = GetSelfMapsCount();
    memCount = GetSelfMemoryCount();
}

void BacktraceLocalTest::TearDown()
{
    ASSERT_GT(PrintBacktraceByLibunwind(), 0);
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
    unw_addr_space_t as;
    unw_init_local_address_space(&as);
    if (as == nullptr) {
        FAIL() << "Failed to init address space.\n";
        return;
    }

    auto symbol = std::make_shared<DfxSymbols>();
    ElapsedTime counter;
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, thread.Unwind(as, symbol, 0));

    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    for (const auto& frame : frames) {
        GTEST_LOG_(INFO) << DfxFrameFormat::GetFrameStr(frame);
    }

    unw_destroy_local_address_space(as);
    ASSERT_GE(frames.size(), PrintBacktraceByLibunwind());
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
    unw_addr_space_t as;
    unw_init_local_address_space(&as);
    if (as == nullptr) {
        FAIL() << "Failed to init address space.\n";
        return;
    }
    g_mutex.lock();
    std::thread backtraceThread(Test001);
    sleep(1);
    if (g_tid <= 0) {
        FAIL() << "Failed to create child thread.\n";
    }

    auto symbol = std::make_shared<DfxSymbols>();
    ElapsedTime counter;
    BacktraceLocalThread thread(g_tid);
    ASSERT_EQ(true, thread.Unwind(as, symbol, 0));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed();
    BacktraceLocalContext::GetInstance().CleanUp();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    auto backtraceStr = thread.GetFormattedStr(false);
    ASSERT_GT(backtraceStr.size(), 0);
    GTEST_LOG_(INFO) << backtraceStr;
    g_mutex.unlock();
    unw_destroy_local_address_space(as);
    g_tid = 0;
    if (backtraceThread.joinable()) {
        backtraceThread.join();
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest003: end.";
}

using GetMap = void (*)(void);
/**
 * @tc.name: BacktraceLocalTest004
 * @tc.desc: test whether crash log is generated if we call a dlsym func after dlclose
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest004: start.";
    pid_t childPid = fork();
    if (childPid == 0) {
        void* handle = dlopen("libunwind.z.so", RTLD_LAZY);
        if (handle == nullptr) {
            FAIL();
        }

        auto getMap = reinterpret_cast<GetMap>(dlsym(handle, "unw_get_map"));
        if (getMap == nullptr) {
            dlclose(handle);
            FAIL();
        }
        // close before call functrion
        dlclose(handle);
        getMap();
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    sleep(1);
    std::string path = GetCppCrashFileName(childPid);
    if (path.empty()) {
        FAIL();
    }
    GTEST_LOG_(INFO) << "LogFile: " << path;

    std::string content;
    if (!OHOS::HiviewDFX::LoadStringFromFile(path, content)) {
        FAIL();
    }

    // both dlclose debug enabled and disabled cases
    if (content.find("Not mapped pc") == std::string::npos &&
        content.find("libunwind") == std::string::npos) {
        FAIL();
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
