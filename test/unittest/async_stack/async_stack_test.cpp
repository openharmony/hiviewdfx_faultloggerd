/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <securec.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include "async_stack.h"
#include "dfx_test_util.h"
#include "elapsed_time.h"
#include "fp_unwinder.h"
#include "uv.h"

using namespace testing;
using namespace testing::ext;

#if defined(__aarch64__)
namespace {
using CollectFn = uint64_t(*)(uint64_t);
using SetFn = void(*)(uint64_t);

std::atomic<int> g_eventHandlerSetCalls {0};
std::atomic<int> g_eventHandlerClearCalls {0};
CollectFn g_lastEventHandlerCollect = nullptr;
SetFn g_lastEventHandlerSetId = nullptr;

void ResetCallbackProbeState()
{
    g_eventHandlerSetCalls.store(0);
    g_eventHandlerClearCalls.store(0);
    g_lastEventHandlerCollect = nullptr;
    g_lastEventHandlerSetId = nullptr;
}
} // namespace

// Provide fake exported symbols so dlsym(RTLD_DEFAULT, "...") in async_stack.cpp can find them.
extern "C" __attribute__((visibility("default"))) void EventSetAsyncStackFunc(
    CollectFn collectAsyncStackFn, SetFn setStackIdFn)
{
    g_lastEventHandlerCollect = collectAsyncStackFn;
    g_lastEventHandlerSetId = setStackIdFn;
    if (collectAsyncStackFn == nullptr || setStackIdFn == nullptr) {
        g_eventHandlerClearCalls.fetch_add(1);
    } else {
        g_eventHandlerSetCalls.fetch_add(1);
    }
}
#endif

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "AsyncStackTest"
#define LOG_DOMAIN 0xD002D11

class AsyncStackTest : public testing::Test {};

constexpr size_t BUFFER_SIZE = 64 * 1024;
char *g_stackTrace = new (std::nothrow) char[BUFFER_SIZE];
int g_result = -1;
static bool g_done = false;
#if defined(__aarch64__)
bool g_hidebugEnable = false;
#endif

NOINLINE static void WorkCallback(uv_work_t* req)
{
    g_result = DfxGetSubmitterStackLocal(g_stackTrace, BUFFER_SIZE);
}

NOINLINE static void AfterWorkCallback(uv_work_t* req, int status)
{
    if (!g_done) {
        uv_queue_work(req->loop, req, WorkCallback, AfterWorkCallback);
    }
}

static void TimerCallback(uv_timer_t* handle)
{
    g_done = true;
}

#if defined(__aarch64__)
static void TestHidebugCallbackFunc(bool enable)
{
    g_hidebugEnable = enable;
}
#endif
/**
 * @tc.name: AsyncStackTest001
 * @tc.desc: test GetAsyncStackLocal
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest001: start.";
    ASSERT_EQ(DfxGetSubmitterStackLocal(g_stackTrace, BUFFER_SIZE), -1);
    DfxInitAsyncStack();
    uv_timer_t timerHandle;
    uv_work_t work;
    uv_loop_t* loop = uv_default_loop();
    int timeout = 1000;
    uv_timer_init(loop, &timerHandle);
    uv_timer_start(&timerHandle, TimerCallback, timeout, 0);
    uv_queue_work(loop, &work, WorkCallback, AfterWorkCallback);
    uv_run(loop, UV_RUN_DEFAULT);
#if defined(__aarch64__)
    ASSERT_EQ(g_result, 0);
    std::string stackTrace = std::string(g_stackTrace);
    ASSERT_GT(stackTrace.size(), 0) << stackTrace;
#else
    ASSERT_EQ(g_result, -1);
#endif
    ASSERT_EQ(DfxGetSubmitterStackLocal(g_stackTrace, 0), -1);
    GTEST_LOG_(INFO) << "AsyncStackTest001: end.";
}

/**
 * @tc.name: AsyncStackTest002
 * @tc.desc: test DfxInitAsyncStack()
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest002: start.";
    bool res = DfxInitAsyncStack();
    GTEST_LOG_(INFO) << "res: " << res;
#if defined(__arm__)
    ASSERT_FALSE(res);
#elif defined(__aarch64__)
    ASSERT_TRUE(res);
#endif

    GTEST_LOG_(INFO) << "AsyncStackTest002: end.";
}

/**
 * @tc.name: AsyncStackTest003
 * @tc.desc: test DfxInitProfilerAsyncStack
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest003: start.";
    size_t size = 1024;
    void* buffer = malloc(size);
#if defined(__aarch64__)
    DfxSetHiDebugAsyncStackCallback(TestHidebugCallbackFunc);
#endif
    bool initOnce = DfxInitProfilerAsyncStack(buffer, size);
    GTEST_LOG_(INFO) << "initOnce: " << initOnce;
    bool initSecond = DfxInitProfilerAsyncStack(buffer, size);
    GTEST_LOG_(INFO) << "initSecond: " << initSecond;
#if defined(__aarch64__)
    ASSERT_TRUE(initOnce);
    ASSERT_FALSE(initSecond);
#endif
    DfxInitProfilerAsyncStack(nullptr, 0);
    free(buffer);
    GTEST_LOG_(INFO) << "AsyncStackTest003: end.";
}

/**
 * @tc.name: AsyncStackTest004
 * @tc.desc: test DfxSetAsyncStackType
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest004: start.";
#if defined(__aarch64__)
    uint64_t oldType = DfxSetAsyncStackType(ASYNC_TYPE_LIBUV_TIMER);
    GTEST_LOG_(INFO) << "oldType: " << oldType;
    ASSERT_EQ(oldType, DEFAULT_ASYNC_TYPE);
    DfxSetAsyncStackType(oldType);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest004: end.";
}

/**
 * @tc.name: AsyncStackTest005
 * @tc.desc: test DfxCollectStackWithDepth
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest005: start.";
#if defined(__aarch64__)
    size_t depth = 10;
    bool res = DfxInitAsyncStack();
    GTEST_LOG_(INFO) << "res: " << res;
    ASSERT_TRUE(res);
    uint64_t stackIdNotTragetType = DfxCollectStackWithDepth(ASYNC_TYPE_CUSTOMIZE, depth);
    GTEST_LOG_(INFO) << "stackIdNotTragetType: " << stackIdNotTragetType;
    uint64_t oldType = DfxSetAsyncStackType(ASYNC_TYPE_FFRT_POOL);
    uint64_t stackIdTragetType = DfxCollectStackWithDepth(ASYNC_TYPE_FFRT_POOL, depth);
    GTEST_LOG_(INFO) << "stackIdTragetType: " << stackIdTragetType;
    ASSERT_EQ(stackIdNotTragetType, 0);
    ASSERT_NE(stackIdTragetType, 0);
    DfxSetAsyncStackType(oldType);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest005: end.";
}

/**
 * @tc.name: AsyncStackTest006
 * @tc.desc: test incremental (un)register callbacks via DfxSetAsyncStackType
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest006: start.";
#if defined(__aarch64__)
    ResetCallbackProbeState();

    // Enable EVENTHANDLER should register non-null callbacks.
    (void)DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE | ASYNC_TYPE_EVENTHANDLER);
    ASSERT_EQ(g_eventHandlerSetCalls.load(), 1);
    ASSERT_NE(reinterpret_cast<void*>(g_lastEventHandlerCollect), nullptr);
    ASSERT_NE(reinterpret_cast<void*>(g_lastEventHandlerSetId), nullptr);

    // Disable EVENTHANDLER should clear callbacks.
    (void)DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    ASSERT_EQ(g_eventHandlerClearCalls.load(), 1);
    ASSERT_EQ(reinterpret_cast<void*>(g_lastEventHandlerCollect), nullptr);
    ASSERT_EQ(reinterpret_cast<void*>(g_lastEventHandlerSetId), nullptr);

    // Enable ASYNC_TYPE_ARKWEB
    uint64_t lastType = DfxSetAsyncStackType(ASYNC_TYPE_ARKWEB);
    ASSERT_EQ(lastType, DEFAULT_ASYNC_TYPE);

    // Disable ASYNC_TYPE_ARKWEB
    lastType = DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    ASSERT_EQ(lastType, ASYNC_TYPE_ARKWEB);

    // Enable ASYNC_TYPE_JSVM
    lastType = DfxSetAsyncStackType(ASYNC_TYPE_JSVM);
    ASSERT_EQ(lastType, DEFAULT_ASYNC_TYPE);

    // Disable ASYNC_TYPE_JSVM
    lastType = DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    ASSERT_EQ(lastType, ASYNC_TYPE_JSVM);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest006: end.";
}

/**
 * @tc.name: AsyncStackTest007
 * @tc.desc: test incremental (un)register callbacks via DfxSetAsyncStackType
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest007, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "AsyncStackTestWithChainedMode: start.";
    DfxInitAsyncStack();
    GTEST_LOG_(INFO) << "DfxCollectAsyncStack: start.";
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000000; i++) {
        uint64_t stackId = DfxCollectAsyncStack(0);
        ReleaseAsyncContext(stackId);
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    GTEST_LOG_(INFO) << "DfxCollectAsyncStack: end. cost: " << duration.count() << "ns" << std::endl;

    GTEST_LOG_(INFO) << "DfxSetSubmitterStackId: start.";
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000000; i++) {
        uint64_t stackId = DfxCollectAsyncStack(0);
        DfxSetSubmitterStackId(stackId);
        DfxPopSubmitterStackId(stackId);
        ReleaseAsyncContext(stackId);
    }
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    GTEST_LOG_(INFO) << "DfxSetSubmitterStackId: end. cost: " << duration.count() << "ns" << std::endl;

    GTEST_LOG_(INFO) << "GetCurrentChainedAsyncContext: start.";
    DfxCollectAsyncStack(0);
    DfxCollectAsyncStack(0);
    DfxCollectAsyncStack(0);
    DfxCollectAsyncStack(0);
    uint64_t stackId5 = DfxCollectAsyncStack(0);
    DfxSetSubmitterStackId(stackId5);
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000000; i++) {
        DfxAsyncCtx buffer[5];
        GetCurrentChainedAsyncContext(buffer, 5);
    }
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    GTEST_LOG_(INFO) << "GetCurrentChainedAsyncContext: end. cost: " << duration.count() << "ns" << std::endl;
#endif
}

/**
 * @tc.name: AsyncStackTest008
 * @tc.desc: test incremental (un)register callbacks via DfxSetAsyncStackType
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest008, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "AsyncStackTestWithLastStackMode: start.";
    DfxInitAsyncStack();
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    auto curMode = GetAsyncStackMode();
    ASSERT_EQ(MODE_LAST_STACKTRACE, curMode);
    GTEST_LOG_(INFO) << "GetAsyncStackMode: " << curMode << "." << std::endl;
    GTEST_LOG_(INFO) << "DfxCollectAsyncStack: start.";
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000000; i++) {
        uint64_t stackId = DfxCollectAsyncStack(0);
        ReleaseAsyncContext(stackId);
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    GTEST_LOG_(INFO) << "DfxCollectAsyncStack: end. cost: " << duration.count() << "ns" << std::endl;

    GTEST_LOG_(INFO) << "DfxSetSubmitterStackId: start.";
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000000; i++) {
        uint64_t stackId = DfxCollectAsyncStack(0);
        DfxSetSubmitterStackId(stackId);
        DfxSetSubmitterStackId(0);
        ReleaseAsyncContext(stackId);
    }
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    GTEST_LOG_(INFO) << "DfxSetSubmitterStackId: end. cost: " << duration.count() << "ns" << std::endl;

    GTEST_LOG_(INFO) << "DfxGetSubmitterStackId: start.";
    uint64_t stackId = DfxCollectAsyncStack(0);
    DfxSetSubmitterStackId(stackId);
    start = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000000; i++) {
        DfxGetSubmitterStackId();
    }
    end = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    GTEST_LOG_(INFO) << "DfxGetSubmitterStackId: end. cost: " << duration.count() << "ns" << std::endl;
#endif
}

/**
 * @tc.name: AsyncStackTest009
 * @tc.desc: test incremental (un)register callbacks via DfxSetAsyncStackType
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest009, TestSize.Level2)
{
#if defined(__aarch64__)
    DfxAsyncCtx buffer[5];
    uint64_t stackId = 0;
    bool init = DfxInitAsyncStack();
    ASSERT_TRUE(init);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);

    DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    stackId = DfxCollectAsyncStack(ASYNC_TYPE_EVENTHANDLER);
    ASSERT_EQ(stackId, 0);

    stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_EQ(buffer[0].type, ASYNC_TYPE_LIBUV_QUEUE);
    ASSERT_NE(buffer[0].id, 0);

    stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_TIMER);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_EQ(buffer[0].type, ASYNC_TYPE_LIBUV_TIMER);
    ASSERT_NE(buffer[0].id, 0);
    ASSERT_EQ(buffer[1].type, ASYNC_TYPE_LIBUV_QUEUE);
    ASSERT_NE(buffer[1].id, 0);

    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_EQ(buffer[0].type, ASYNC_TYPE_LIBUV_QUEUE);
    ASSERT_NE(buffer[0].id, 0);

    DfxSetAsyncStackType(ASYNC_TYPE_EVENTHANDLER);
    stackId = DfxCollectAsyncStack(ASYNC_TYPE_EVENTHANDLER);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_EQ(buffer[0].type, ASYNC_TYPE_EVENTHANDLER);
    ASSERT_NE(buffer[0].id, 0);
    ASSERT_EQ(buffer[1].type, ASYNC_TYPE_LIBUV_QUEUE);
    ASSERT_NE(buffer[1].id, 0);

    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_EQ(buffer[0].type, 0);
    ASSERT_EQ(buffer[0].id, 0);
    
    DfxAsyncMode premode = SetAsyncStackMode(MODE_LAST_STACKTRACE);
    ASSERT_EQ(premode, MODE_CHAINED_STACKTRACE);
#endif
}

/**
 * @tc.name: AsyncStackTest010
 * @tc.desc: test SetChainedAsyncStackConfig with valid parameters
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest010: start.";
#if defined(__aarch64__)
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(5, 32, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    uint64_t stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    DfxAsyncCtx buffer[5];
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    int count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    ASSERT_EQ(buffer[0].type, ASYNC_TYPE_LIBUV_QUEUE);
    ASSERT_NE(buffer[0].id, 0);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 64 * 1024);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest010: end.";
}

/**
 * @tc.name: AsyncStackTest011
 * @tc.desc: test SetChainedAsyncStackConfig clamps out-of-range maxLayer
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest011: start.";
#if defined(__aarch64__)
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(0, 16, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    uint64_t stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    DfxAsyncCtx buffer[5];
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    int count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);

    SetChainedAsyncStackConfig(4294967295u, 16, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);

    SetChainedAsyncStackConfig(18, 16, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 64 * 1024);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest011: end.";
}

/**
 * @tc.name: AsyncStackTest012
 * @tc.desc: test SetChainedAsyncStackConfig clamps out-of-range maxStackDepth
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest012: start.";
#if defined(__aarch64__)
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 0, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    uint64_t stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    DfxAsyncCtx buffer[5];
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    int count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);

    SetChainedAsyncStackConfig(10, 257, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 64 * 1024);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest012: end.";
}

/**
 * @tc.name: AsyncStackTest013
 * @tc.desc: test SetChainedAsyncStackConfig clamps out-of-range maxChainPoolSize
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest013: start.";
#if defined(__aarch64__)
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 0);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    uint64_t stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    DfxAsyncCtx buffer[5];
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    int count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);

    SetChainedAsyncStackConfig(17, 16, 5);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);

    SetChainedAsyncStackConfig(10, 16, 640 * 1024 + 1);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 64 * 1024);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest013: end.";
}

/**
 * @tc.name: AsyncStackTest014
 * @tc.desc: test manual mode switch triggers DeInit for reconfiguration
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest014: start.";
#if defined(__aarch64__)
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);
    ASSERT_EQ(GetAsyncStackMode(), MODE_CHAINED_STACKTRACE);

    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(5, 16, 32 * 1024);
    ASSERT_EQ(GetAsyncStackMode(), MODE_LAST_STACKTRACE);

    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    uint64_t stackId = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId);
    DfxAsyncCtx buffer[5];
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    int count = GetCurrentChainedAsyncContext(buffer, 5);
    ASSERT_GE(count, 1);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId);

    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 64 * 1024);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest014: end.";
}

/**
 * @tc.name: AsyncStackTest015
 * @tc.desc: test SetChainedAsyncStackConfig maxLayer limit
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest015: start.";
#if defined(__aarch64__)
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(3, 16, 64 * 1024);
    SetAsyncStackMode(MODE_CHAINED_STACKTRACE);
    DfxSetAsyncStackType(DEFAULT_ASYNC_TYPE);

    uint64_t stackId1 = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId1);

    uint64_t stackId2 = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_TIMER);
    DfxSetSubmitterStackId(stackId2);

    uint64_t stackId3 = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_QUEUE);
    DfxSetSubmitterStackId(stackId3);

    uint64_t stackId4 = DfxCollectAsyncStack(ASYNC_TYPE_LIBUV_TIMER);
    DfxSetSubmitterStackId(stackId4);

    DfxAsyncCtx buffer[10];
    (void)memset_s(buffer, sizeof(buffer), 0, sizeof(buffer));
    int count = GetCurrentChainedAsyncContext(buffer, 10);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(buffer[0].type, ASYNC_TYPE_LIBUV_TIMER);
    ASSERT_NE(buffer[0].id, 0);

    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId4);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId3);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId2);
    DfxSetSubmitterStackId(0);
    ReleaseAsyncContext(stackId1);
    SetAsyncStackMode(MODE_LAST_STACKTRACE);
    SetChainedAsyncStackConfig(10, 16, 64 * 1024);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest015: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
