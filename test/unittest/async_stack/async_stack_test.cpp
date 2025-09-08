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
#include <memory>
#include <mutex>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <securec.h>
#include <sys/wait.h>
#include <unistd.h>

#include "async_stack.h"
#include "dfx_test_util.h"
#include "elapsed_time.h"
#include "fp_unwinder.h"
#include "uv.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "AsyncStackTest"
#define LOG_DOMAIN 0xD002D11

class AsyncStackTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void AsyncStackTest::SetUpTestCase()
{}

void AsyncStackTest::TearDownTestCase()
{}

void AsyncStackTest::SetUp()
{}

void AsyncStackTest::TearDown()
{}

constexpr size_t BUFFER_SIZE = 64 * 1024;
char *g_stackTrace = new (std::nothrow) char[BUFFER_SIZE];
int g_result = -1;
static bool g_done = false;

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
} // namespace HiviewDFX
} // namepsace OHOS
