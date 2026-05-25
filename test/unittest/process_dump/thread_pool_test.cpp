/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "thread_pool.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class ThreadPoolTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: ThreadPoolTest001
 * @tc.desc: test ThreadPool starting and adding tasks
 * @tc.type: FUNC
 */
HWTEST_F(ThreadPoolTest, ThreadPool_StartAddTask_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadPool_StartAddTask_001: start.";
    ThreadPool pool;
    std::atomic<int> counter{0};
    pool.Start(2);
    for (int i = 0; i < 4; i++) {
        pool.AddTask([&counter]() { counter++; });
    }
    pool.Stop();
    ASSERT_EQ(counter.load(), 4);
    GTEST_LOG_(INFO) << "ThreadPool_StartAddTask_001: end.";
}

/**
 * @tc.name: ThreadPoolTest002
 * @tc.desc: test ThreadPool normal stop behavior
 * @tc.type: FUNC
 */
HWTEST_F(ThreadPoolTest, ThreadPool_StopNormal_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadPool_StopNormal_002: start.";
    ThreadPool pool;
    pool.Start(2);
    std::atomic<int> counter{0};
    pool.AddTask([&counter]() { counter++; });
    pool.Stop();
    ASSERT_EQ(counter.load(), 1);
    GTEST_LOG_(INFO) << "ThreadPool_StopNormal_002: end.";
}

/**
 * @tc.name: ThreadPoolTest003
 * @tc.desc: test ThreadPool stop when already stopped
 * @tc.type: FUNC
 */
HWTEST_F(ThreadPoolTest, ThreadPool_StopAlreadyStopped_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadPool_StopAlreadyStopped_003: start.";
    ThreadPool pool;
    std::atomic<int> counter{0};
    pool.Start(2);
    pool.AddTask([&counter]() { counter++; });
    pool.Stop();
    ASSERT_EQ(counter.load(), 1);
    pool.Stop();
    ASSERT_EQ(counter.load(), 1);
    GTEST_LOG_(INFO) << "ThreadPool_StopAlreadyStopped_003: end.";
}

/**
 * @tc.name: ThreadPoolTest004
 * @tc.desc: test ThreadPool stop with timeout succeeding
 * @tc.type: FUNC
 */
HWTEST_F(ThreadPoolTest, ThreadPool_StopWithTimeOut_Success_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadPool_StopWithTimeOut_Success_004: start.";
    ThreadPool pool;
    std::atomic<int> counter{0};
    pool.Start(2);
    for (int i = 0; i < 4; i++) {
        pool.AddTask([&counter]() { counter++; });
    }
    bool result = pool.StopWithTimeOut(5000);
    ASSERT_TRUE(result);
    ASSERT_EQ(counter.load(), 4);
    GTEST_LOG_(INFO) << "ThreadPool_StopWithTimeOut_Success_004: end.";
}

/**
 * @tc.name: ThreadPoolTest005
 * @tc.desc: test ThreadPool adding task after stop is ignored
 * @tc.type: FUNC
 */
HWTEST_F(ThreadPoolTest, ThreadPool_AddTaskAfterStop_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadPool_AddTaskAfterStop_006: start.";
    ThreadPool pool;
    std::atomic<int> counter{0};
    pool.Start(2);
    pool.AddTask([&counter]() { counter++; });
    pool.Stop();
    ASSERT_EQ(counter.load(), 1);
    pool.AddTask([&counter]() { counter++; });
    ASSERT_EQ(counter.load(), 1);
    GTEST_LOG_(INFO) << "ThreadPool_AddTaskAfterStop_006: end.";
}

/**
 * @tc.name: ThreadPoolTest006
 * @tc.desc: test ThreadPool taking task with stop signal
 * @tc.type: FUNC
 */
HWTEST_F(ThreadPoolTest, ThreadPool_TakeTask_StopSignal_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadPool_TakeTask_StopSignal_007: start.";
    ThreadPool pool;
    std::atomic<int> counter{0};
    pool.Start(2);
    pool.AddTask([&counter]() { counter++; });
    pool.Stop();
    ASSERT_EQ(counter.load(), 1);
    GTEST_LOG_(INFO) << "ThreadPool_TakeTask_StopSignal_007: end.";
}

/**
 * @tc.name: ThreadPoolTest007
 * @tc.desc: test ThreadPool destructor auto-stopping threads
 * @tc.type: FUNC
 */
HWTEST_F(ThreadPoolTest, ThreadPool_DestructorAutoStop_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ThreadPool_DestructorAutoStop_008: start.";
    std::atomic<int> counter{0};
    {
        ThreadPool pool;
        pool.Start(2);
        pool.AddTask([&counter]() { counter++; });
    }
    ASSERT_EQ(counter.load(), 1);
    GTEST_LOG_(INFO) << "ThreadPool_DestructorAutoStop_008: end.";
}
}