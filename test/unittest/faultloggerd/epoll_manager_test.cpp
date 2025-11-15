/*
* Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include "epoll_manager.h"

#include <functional>
#include <gtest/gtest.h>
#include <securec.h>
#include <thread>
#include <vector>

#include "faultloggerd_test.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;


class EpollManagerTest : public testing::Test {
public:
    static void SetUpTestCase(void);
};

void EpollManagerTest::SetUpTestCase()
{
    FaultLoggerdTestServer::GetInstance();
}

/**
 * @tc.name: DelayTaskTest01
 * @tc.desc: request a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(EpollManagerTest, DelayTaskTest01, TestSize.Level2)
{
    vector<uint32_t> taskExecuteRecord;
    constexpr uint32_t testDelayTimes[] = {2, 1, 2, 3, 1, 3};
    constexpr auto dealyTaskNum = sizeof(testDelayTimes) / sizeof(testDelayTimes[0]);
    for (uint32_t delayTime : testDelayTimes) {
        auto task = [&taskExecuteRecord, delayTime] {
            auto ret = DelayTaskQueue::GetInstance().AddDelayTask([&taskExecuteRecord, delayTime] {
                        taskExecuteRecord.emplace_back(delayTime);
                    }, delayTime);
            ASSERT_TRUE(ret);
        };
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        ASSERT_TRUE(FaultLoggerdTestServer::AddTask(TestThreadEnum::HELPER, task));
    }
    constexpr uint64_t waitTime = 4;
    std::this_thread::sleep_for(std::chrono::seconds(waitTime));
    ASSERT_EQ(taskExecuteRecord.size(), dealyTaskNum);
    for (size_t i = 1; i < taskExecuteRecord.size(); i++) {
        ASSERT_GE(taskExecuteRecord[i], taskExecuteRecord[i - 1]);
    }
}

/**
 * @tc.name: DelayTaskTest01
 * @tc.desc: request a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(EpollManagerTest, DelayTaskTest02, TestSize.Level2)
{
    auto task = [] {
        ASSERT_FALSE(DelayTaskQueue::GetInstance().AddDelayTask(nullptr, 0));
        ASSERT_FALSE(DelayTaskQueue::GetInstance().AddDelayTask([] {}, 0));
    };
    auto ret = FaultLoggerdTestServer::AddTask(TestThreadEnum::HELPER, task);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(ret);
}


/**
 * @tc.name: TimerTaskTest01
 * @tc.desc: request a pip fd.
 * @tc.type: FUNC
 */
HWTEST_F(EpollManagerTest, TimerTaskTest01, TestSize.Level2)
{
    ASSERT_FALSE(TimerTaskAdapter::CreateInstance(nullptr, 0, 0));
    ASSERT_FALSE(TimerTaskAdapter::CreateInstance([] {}, -1, 0));
    ASSERT_FALSE(TimerTaskAdapter::CreateInstance([] {}, 0, -1));
    ASSERT_TRUE(TimerTaskAdapter::CreateInstance([] {}, 0, 0));
}
