/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <gtest/hwext/gtest-multithread.h>

#include <string>
#include <thread>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "kernel_stack_async_collector.h"

using namespace testing;
using namespace testing::ext;
using namespace testing::mt;

namespace OHOS {
namespace HiviewDFX {
std::atomic<int> g_count = 0;
class KernelStackAsyncCollectorTest : public testing::Test {
public:
    static void SetUpTestCase() {};
    static void TearDownTestCase() {};
    void SetUp() {};
    void TearDown() {};
};

/**
 * @tc.name: KernelStackAsyncCollectorTest001
 * @tc.desc: test KernelStackAsyncCollectorTest NotifyStartCollect interface
 * @tc.type: FUNC
 */
HWTEST_F(KernelStackAsyncCollectorTest, KernelStackAsyncCollectorTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest001: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }
    KernelStackAsyncCollector stackCollector;
    pid_t tid = gettid();
    ASSERT_TRUE(stackCollector.NotifyStartCollect(tid));
    KernelStackAsyncCollector::KernelResult stack = stackCollector.GetCollectedStackResult();
    ASSERT_NE(stack.first, KernelStackAsyncCollector::STACK_SUCCESS);
    ASSERT_TRUE(stack.second.empty());
    sleep(1);
    stack = stackCollector.GetCollectedStackResult();
    ASSERT_EQ(stack.first, KernelStackAsyncCollector::STACK_SUCCESS);
    ASSERT_TRUE(stack.second.find(std::to_string(tid)) != std::string::npos);
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest001: end.";
}

/**
 * @tc.name: KernelStackAsyncCollectorTest002
 * @tc.desc: test KernelStackAsyncCollectorTest NotifyStartCollect interface
 * @tc.type: FUNC
 */
HWTEST_F(KernelStackAsyncCollectorTest, KernelStackAsyncCollectorTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest002: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }
    KernelStackAsyncCollector stackCollector;
    pid_t tid = gettid();
    int waitTime = 0;
    KernelStackAsyncCollector::KernelResult stack = stackCollector.GetProcessStackWithTimeout(tid, waitTime);
    ASSERT_NE(stack.first, KernelStackAsyncCollector::STACK_SUCCESS);
    ASSERT_TRUE(stack.second.empty());

    waitTime = 1000; // 1000 : 1000ms
    stack = stackCollector.GetProcessStackWithTimeout(tid, waitTime);
    ASSERT_EQ(stack.first, KernelStackAsyncCollector::STACK_SUCCESS);
    ASSERT_TRUE(stack.second.find(std::to_string(tid)) != std::string::npos);
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest002: end.";
}

static void NotifyCollectStackTest()
{
    KernelStackAsyncCollector stackCollector;
    pid_t tid = gettid();
    if (stackCollector.NotifyStartCollect(tid)) {
        sleep(1);
        KernelStackAsyncCollector::KernelResult stack = stackCollector.GetCollectedStackResult();
        ASSERT_EQ(stack.first, KernelStackAsyncCollector::STACK_SUCCESS);
        ASSERT_TRUE(stack.second.find(std::to_string(tid)) != std::string::npos);
        g_count++;
    }
}

static void CollectStackWithTimeoutTest()
{
    constexpr int waitTime = 1000; // 1000 : 1000ms
    pid_t tid = gettid();
    KernelStackAsyncCollector stackCollector;
    KernelStackAsyncCollector::KernelResult stack = stackCollector.GetProcessStackWithTimeout(tid, waitTime);
    if (stack.first == KernelStackAsyncCollector::STACK_SUCCESS) {
        ASSERT_TRUE(stack.second.find(std::to_string(tid)) != std::string::npos);
        g_count++;
    }
}

/**
 * @tc.name: KernelStackAsyncCollectorTest003
 * @tc.desc: test KernelStackAsyncCollectorTest NotifyStartCollect interface
 * @tc.type: FUNC
 */
HWTEST_F(KernelStackAsyncCollectorTest, KernelStackAsyncCollectorTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest003: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }
    int threadCount = 3;
    SET_THREAD_NUM(threadCount);
    GTEST_RUN_TASK(NotifyCollectStackTest);
    sleep(2); // 2 : 2s
    ASSERT_EQ(g_count, threadCount);
    threadCount = 10;
    SET_THREAD_NUM(threadCount);
    g_count = 0;
    GTEST_RUN_TASK(NotifyCollectStackTest);
    ASSERT_LT(g_count, threadCount);
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest003: end.";
}

/**
 * @tc.name: KernelStackAsyncCollectorTest004
 * @tc.desc: test KernelStackAsyncCollectorTest GetProcessStackWithTimeout interface
 * @tc.type: FUNC
 */
HWTEST_F(KernelStackAsyncCollectorTest, KernelStackAsyncCollectorTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest004: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }
    g_count = 0;
    int threadCount = 3;
    SET_THREAD_NUM(threadCount);
    GTEST_RUN_TASK(CollectStackWithTimeoutTest);
    sleep(2); // 2 : 2s
    ASSERT_EQ(g_count, threadCount);
    threadCount = 10;
    SET_THREAD_NUM(threadCount);
    g_count = 0;
    GTEST_RUN_TASK(CollectStackWithTimeoutTest);
    ASSERT_LT(g_count, threadCount);
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest004: end.";
}

/**
 * @tc.name: KernelStackAsyncCollectorTest005
 * @tc.desc: test KernelStackAsyncCollectorTest NotifyStartCollect interface in abnormal case
 * @tc.type: FUNC
 */
HWTEST_F(KernelStackAsyncCollectorTest, KernelStackAsyncCollectorTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest005: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }
    KernelStackAsyncCollector stackCollector;
    pid_t tid = 0;
    ASSERT_TRUE(stackCollector.NotifyStartCollect(tid));
    sleep(1); // 1 : 1s
    KernelStackAsyncCollector::KernelResult stack = stackCollector.GetCollectedStackResult();
    ASSERT_EQ(stack.first, KernelStackAsyncCollector::STACK_NO_PROCESS);
    ASSERT_TRUE(stack.second.empty());
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest005: end.";
}

/**
 * @tc.name: KernelStackAsyncCollectorTest006
 * @tc.desc: test KernelStackAsyncCollectorTest GetProcessStackWithTimeout interface in abnormal case
 * @tc.type: FUNC
 */
HWTEST_F(KernelStackAsyncCollectorTest, KernelStackAsyncCollectorTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest006: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }
    KernelStackAsyncCollector stackCollector;
    int waitTime = 1000; // 1000 : 1000ms
    KernelStackAsyncCollector::KernelResult stack = stackCollector.GetProcessStackWithTimeout(0, waitTime);
    ASSERT_EQ(stack.first, KernelStackAsyncCollector::STACK_NO_PROCESS);
    ASSERT_TRUE(stack.second.empty());
    GTEST_LOG_(INFO) << "KernelStackAsyncCollectorTest006: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS