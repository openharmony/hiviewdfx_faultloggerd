/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <file_ex.h>
#include <libunwind_i-ohos.h>
#include <libunwind.h>
#include <securec.h>

#include "backtrace.h"
#include "backtrace_local_static.h"
#include "backtrace_local_thread.h"
#include "dfx_symbols_cache.h"
#include "elapsed_time.h"
#include "test_utils.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "BacktraceLocalTest"
#define LOG_DOMAIN 0xD002D11
namespace {
static void CheckResourceUsage(uint32_t fdCount, uint32_t mapsCount, uint64_t memCount)
{
    // check memory/fd/maps
    auto curFdCount = GetSelfFdCount();
    constexpr uint32_t extraVal = 10;
    ASSERT_LE(curFdCount, fdCount + extraVal);
    GTEST_LOG_(INFO) << "AfterTest Fd New:" << std::to_string(curFdCount);
    GTEST_LOG_(INFO) << "Fd Old:" << std::to_string(fdCount) << "\n";

    auto curMapsCount = GetSelfMapsCount();
    ASSERT_LE(curMapsCount, mapsCount + extraVal);
    GTEST_LOG_(INFO) << "AfterTest Maps New:" << std::to_string(curMapsCount);
    GTEST_LOG_(INFO) << "Maps Old:" << std::to_string(mapsCount) << "\n";

    auto curMemSize = GetSelfMemoryCount();
    constexpr double ratio = 1.5;
    ASSERT_LE(curMemSize, (memCount * ratio));
    GTEST_LOG_(INFO) << "AfterTest Memory New(KB):" << std::to_string(curMemSize);
    GTEST_LOG_(INFO) << "Memory Old(KB):" << std::to_string(memCount) << "\n";
}
}
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

static std::string GetNativeFrameStr(const NativeFrame& frame)
{
    char buf[1024] = "\0"; // 1024 buffer length
#ifdef __LP64__
    char format[] = "#%02zu pc %016" PRIx64 " %s";
#else
    char format[] = "#%02zu pc %08" PRIx64 " %s";
#endif
    if (snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format,
        frame.index,
        frame.relativePc,
        frame.binaryName.empty() ? "Unknown" : frame.binaryName.c_str()) <= 0) {
        return "[Unknown]";
    }

    std::ostringstream ss;
    ss << std::string(buf, strlen(buf));
    if (frame.funcName.empty()) {
        ss << std::endl;
    } else {
        ss << "(";
        ss << frame.funcName.c_str();
        ss << "+" << frame.funcOffset << ")" << std::endl;
    }
    return ss.str();
}

/**
 * @tc.name: BacktraceLocalTest000
 * @tc.desc: collect resource usage without perform any test
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest000, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest001: start.";
    // Do Nothing and collect resource usage
    GTEST_LOG_(INFO) << "BacktraceLocalTest001: end.";
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

    auto cache = std::make_shared<DfxSymbolsCache>();
    ElapsedTime counter;
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
    ASSERT_EQ(true, thread.Unwind(as, cache, 0));

    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed() << "\n";
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    for (const auto& frame : frames) {
        GTEST_LOG_(INFO) << GetNativeFrameStr(frame) << "\n";
    }

    unw_destroy_local_address_space(as);
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

    auto cache = std::make_shared<DfxSymbolsCache>();
    ElapsedTime counter;
    BacktraceLocalThread thread(g_tid);
    ASSERT_EQ(true, thread.Unwind(as, cache, 0));
    GTEST_LOG_(INFO) << "UnwindCurrentCost:" << counter.Elapsed() << "\n";
    BacktraceLocalStatic::GetInstance().CleanUp();
    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    for (const auto& frame : frames) {
        GTEST_LOG_(INFO) << GetNativeFrameStr(frame) << "\n";
    }
    g_mutex.unlock();
    unw_destroy_local_address_space(as);
    g_tid = 0;
    if (backtraceThread.joinable()) {
        backtraceThread.join();
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest003: end.";
}

/**
 * @tc.name: BacktraceLocalTest004
 * @tc.desc: getcontext in caller thread and unwind in forked process (~0.7ms@rk3568)
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest004: start.";
    ElapsedTime counter;
    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);
    pid_t child = fork();
    if (child == 0) {
        unw_addr_space_t as;
        unw_init_local_address_space(&as);
        if (as == nullptr) {
            FAIL() << "Failed to init address space.\n";
            return;
        }

        auto cache = std::make_shared<DfxSymbolsCache>();
        ElapsedTime counter2;
        BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
        ASSERT_EQ(true, thread.UnwindWithContext(as, context, cache, 0));
        GTEST_LOG_(INFO) << "ChildProcessElapse:" << counter2.Elapsed() << "\n";
        const auto& frames = thread.GetFrames();
        ASSERT_GT(frames.size(), 0);
        for (const auto& frame : frames) {
            GTEST_LOG_(INFO) << GetNativeFrameStr(frame) << "\n";
        }
        unw_destroy_local_address_space(as);
        _exit(0);
    }
    GTEST_LOG_(INFO) << "CurrentThreadElapse:" << counter.Elapsed() << "\n";

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret << "\n";
    GTEST_LOG_(INFO) << "BacktraceLocalTest004: end.";
}

#ifdef __aarch64__
/**
 * @tc.name: BacktraceLocalTest005
 * @tc.desc: getcontext in caller thread and unwind by frame pointer, parse binary info asynchronously
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest005: start.";
    ElapsedTime counter;
    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);
    GTEST_LOG_(INFO) << "GetContext Elapse:" << counter.Elapsed() << "\n";

    counter.Reset();
    BacktraceLocalThread thread(BACKTRACE_CURRENT_THREAD);
    bool ret = thread.UnwindWithContextByFramePointer(context, 0);
    GTEST_LOG_(INFO) << "Unwind Elapse:" << counter.Elapsed() << "\n";

    counter.Reset();
    thread.UpdateFrameInfo();
    GTEST_LOG_(INFO) << "UpdateFrameInfo Elapse:" << counter.Elapsed() << "\n";
    ASSERT_EQ(true, ret);

    const auto& frames = thread.GetFrames();
    ASSERT_GT(frames.size(), 0);
    for (const auto& frame : frames) {
        GTEST_LOG_(INFO) << GetNativeFrameStr(frame) << "\n";
    }
    GTEST_LOG_(INFO) << "BacktraceLocalTest005: end.";
}
#endif

using GetMap = void (*)(void);
/**
 * @tc.name: BacktraceLocalTest006
 * @tc.desc: test whether crash log is generated if we call a dlsym func after dlclose
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceLocalTest, BacktraceLocalTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceLocalTest006: start.";
    pid_t child = fork();
    if (child == 0) {
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
    std::string path;
    sleep(1);
    ASSERT_EQ(true, CheckLogFileExist(child, path));
    GTEST_LOG_(INFO) << "LogFile: " << path << "\n";

    std::string content;
    if (!OHOS::LoadStringFromFile(path, content)) {
        FAIL();
    }

    // both dlclose debug enabled and disabled cases
    if (content.find("Not mapped pc") == std::string::npos &&
        content.find("libunwind") == std::string::npos) {
        FAIL();
    }

    GTEST_LOG_(INFO) << "BacktraceLocalTest006: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS