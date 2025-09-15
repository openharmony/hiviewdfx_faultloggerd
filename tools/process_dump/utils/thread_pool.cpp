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

#include "thread_pool.h"
#include <unistd.h>
#include "dfx_log.h"
#include "dfx_define.h"
#include "dfx_util.h"
namespace OHOS {
namespace HiviewDFX {
void ThreadPool::Start(size_t threadPoolCapacity)
{
    for (size_t i = 0; i < threadPoolCapacity; ++i) {
        workers_.emplace_back([this] {
            std::function<void()> task;
            while (TakeTask(task)) {
                task();
                completeTaskCount_++;
            }
        });
    }
}

void ThreadPool::AddTask(const std::function<void()> &task)
{
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            return;
        }
        tasks_.emplace_back(task);
        taskNum_++;
    }
    condition_.notify_one();
}

bool ThreadPool::TakeTask(std::function<void()> &task)
{
    std::unique_lock<std::mutex> lock(queueMutex_);
    condition_.wait(lock, [this] {
        return stop_ || !tasks_.empty();
    });
    if (stop_ && tasks_.empty()) {
        return false;
    }
    task = std::move(tasks_.front());
    tasks_.pop_front();
    return true;
}

bool ThreadPool::StopWithTimeOut(uint64_t waitTimeOutMs)
{
    bool completeTask = false;
    uint64_t beginTime = GetAbsTimeMilliSeconds();
    uint64_t curTime = beginTime;
    do {
        if (completeTaskCount_ == taskNum_) {
            completeTask = true;
            break;
        }
        OHOS_TEMP_FAILURE_RETRY(usleep(1000)); // 1000 : 1ms
        curTime = GetAbsTimeMilliSeconds();
    } while (curTime - beginTime < waitTimeOutMs);
    completeTask = completeTaskCount_ == taskNum_;
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        tasks_.clear();
        stop_ = true;
    }
    condition_.notify_all();
    for (auto &worker : workers_) {
        worker.detach();
    }
    return completeTask;
}

void ThreadPool::Stop()
{
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            return;
        }
        stop_ = true;
    }
    condition_.notify_all();
    for (auto &worker : workers_) {
        worker.join();
    }
}

ThreadPool::~ThreadPool()
{
    if (!stop_) {
        Stop();
    }
}
} // namespace HiviewDFX
} // namespace OHOS