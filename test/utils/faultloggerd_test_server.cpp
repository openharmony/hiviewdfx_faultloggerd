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

#include "faultloggerd_test_server.h"

#include <sys/eventfd.h>

#include "fault_logger_daemon.h"
#include "dfx_log.h"

using namespace OHOS::HiviewDFX;

namespace {
constexpr const char *const FAULTLOGGERD_TEST_TAG = "FAULTLOGGERD_TEST";
}

SmartFd CreateEventFd()
{
    auto ret = SmartFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    if (!ret) {
        DFXLOGE("%{public}s :: failed to create eventFd and errno is %{public}d.", FAULTLOGGERD_TEST_TAG, errno);
    }
    return ret;
}

TaskQueue::Executor::Executor(TaskQueue& taskQueue) : EpollListener(CreateEventFd(), true), taskQueue_(taskQueue) {}

TaskQueue::Executor::~Executor()
{
    std::lock_guard<std::mutex> lock(taskQueue_.mutex);
    taskQueue_.executor_ = nullptr;
    taskQueue_.tasks_.clear();
}

bool TaskQueue::AddTask(const std::function<void()> &task)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (task == nullptr || executor_ == nullptr) {
        DFXLOGE("%{public}s :: failed to add task.", FAULTLOGGERD_TEST_TAG);
        return false;
    }
    tasks_.emplace_back(task);
    uint64_t value = 3;
    if (write(executor_->GetFd(), &value, sizeof(uint64_t)) != sizeof(uint64_t)) {
        DFXLOGE("%{public}s :: failed to write msg to event fd for %{public}d.", FAULTLOGGERD_TEST_TAG, errno);
    }
    return true;
}

bool TaskQueue::InitExecutor()
{
    auto executor = std::unique_ptr<Executor>(new Executor(*this));
    executor_ = executor.get();
    return EpollManager::GetInstance().AddListener(std::move(executor));
}

void TaskQueue::Executor::OnEventPoll()
{
    uint64_t buf;
    if (read(GetFd(), &buf, sizeof(uint64_t)) != sizeof(uint64_t)) {
        DFXLOGE("%{public}s :: failed to recv msg from event fd for %{public}d.", FAULTLOGGERD_TEST_TAG, errno);
    }
    while (!taskQueue_.tasks_.empty()) {
        taskQueue_.tasks_.front()();
        std::lock_guard<std::mutex> lock(taskQueue_.mutex);
        taskQueue_.tasks_.pop_front();
    }
}

TaskQueue &TaskQueue::GetInstance(TestThreadEnum testThreadEnum)
{
    switch (testThreadEnum) {
        case TestThreadEnum::HELPER: {
            static TaskQueue helper;
            return helper;
        }
        default: {
            static TaskQueue main;
            return main;
        }
    }
}

FaultLoggerdTestServer &FaultLoggerdTestServer::GetInstance()
{
    static FaultLoggerdTestServer faultLoggerdTestServer;
    return faultLoggerdTestServer;
}

FaultLoggerdTestServer::FaultLoggerdTestServer()
{
    constexpr int32_t maxConnection = 30;
    constexpr int32_t maxEpollEvent = 1024;
    helperServer_ = std::thread([] {
        auto& helper = EpollManager::GetInstance();
        helper.Init(maxEpollEvent);
        TaskQueue::GetInstance(TestThreadEnum::HELPER).InitExecutor();
        FaultLoggerDaemon::GetInstance().InitHelperServer();
        helper.StartEpoll(maxConnection);
    });
    mainServer_ = std::thread([] {
        auto& main = EpollManager::GetInstance();
        main.Init(maxEpollEvent);
        TaskQueue::GetInstance(TestThreadEnum::MAIN).InitExecutor();
        FaultLoggerDaemon::GetInstance().InitMainServer();
        constexpr auto epollTimeoutInMilliseconds = 3 * 1000;
        main.StartEpoll(maxConnection, epollTimeoutInMilliseconds);
    });
    constexpr int32_t faultLoggerdInitTime = 2;
    // Pause for two seconds to wait for the server to initialize.
    std::this_thread::sleep_for(std::chrono::seconds(faultLoggerdInitTime));
}

bool FaultLoggerdTestServer::AddTask(TestThreadEnum type, const std::function<void()>& task)
{
    return TaskQueue::GetInstance(type).AddTask(task);
}
FaultLoggerdTestServer::~FaultLoggerdTestServer()
{
    auto stopTask = [] {
        EpollManager::GetInstance().StopEpoll();
    };
    if (mainServer_.joinable()) {
        TaskQueue::GetInstance(TestThreadEnum::MAIN).AddTask(stopTask);
        mainServer_.join();
    }
    if (helperServer_.joinable()) {
        TaskQueue::GetInstance(TestThreadEnum::HELPER).AddTask(stopTask);
        helperServer_.join();
    }
}
