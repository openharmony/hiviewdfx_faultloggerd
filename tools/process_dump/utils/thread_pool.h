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

#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <condition_variable>
#include <functional>
#include <mutex>
#include <nocopyable.h>
#include <deque>
#include <thread>
#include <vector>

namespace OHOS {
namespace HiviewDFX {
class ThreadPool {
public:
    ThreadPool() : completeTaskCount_(0), taskNum_(0) {};
    void Start(size_t threadPoolCapacity);
    void AddTask(const std::function<void()> &task);
    bool StopWithTimeOut(uint64_t waitTimeOutMs = 0);
    void Stop();
    ~ThreadPool();
private:
    DISALLOW_COPY_AND_MOVE(ThreadPool);
    bool TakeTask(std::function<void()> &task);
    std::vector<std::thread> workers_;
    std::deque<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<size_t> completeTaskCount_;
    size_t taskNum_;
    bool stop_ = false;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif