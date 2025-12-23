/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef FAULTLOGGERD_TEST_SERVER_H
#define FAULTLOGGERD_TEST_SERVER_H

#include <functional>
#include <thread>

#include "epoll_manager.h"
enum class ExecutorThreadType {
    MAIN,
    HELPER,
};

class TaskQueue {
public:
    static TaskQueue& GetInstance(ExecutorThreadType type);
    bool AddTask(const std::function<void()>& task);
    bool InitExecutor();
private:
    TaskQueue() = default;

    class Executor final : public OHOS::HiviewDFX::EpollListener {
    public:
        ~Executor() override;
        explicit Executor(TaskQueue& taskQueue);
        void OnEventPoll() override;
    private:
        TaskQueue& taskQueue_;
    };
    const Executor* executor_{};
    std::list<std::function<void()>> tasks_;
    std::mutex mutex_;
};

class FaultLoggerdTestServer {
public:
    static FaultLoggerdTestServer& GetInstance();
    FaultLoggerdTestServer(const FaultLoggerdTestServer &) = delete;
    FaultLoggerdTestServer(FaultLoggerdTestServer &&) = delete;
    FaultLoggerdTestServer &operator=(const FaultLoggerdTestServer &) = delete;
    FaultLoggerdTestServer &operator=(FaultLoggerdTestServer &&) = delete;
    static bool AddTask(ExecutorThreadType type, const std::function<void()>& task);
private:
    std::thread mainServer_;
    std::thread helperServer_;
    FaultLoggerdTestServer();
    ~FaultLoggerdTestServer();
};

#endif