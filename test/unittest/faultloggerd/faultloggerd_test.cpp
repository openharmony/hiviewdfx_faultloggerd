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

#include "faultloggerd_test.h"

#include <string>
#include <thread>
#include <vector>

#include "directory_ex.h"
#include "fault_logger_daemon.h"
#include <sys/eventfd.h>
#include "dfx_log.h"

namespace {
constexpr const char *const FAULTLOGGERD_TEST_TAG = "EPOLL_MANAGER";

 OHOS::HiviewDFX::SmartFd CreateEventFd()
{
    auto ret = OHOS::HiviewDFX::SmartFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    if (!ret) {
        DFXLOGE("%{public}s :: failed to create eventFd and errno is %{public}d.", FAULTLOGGERD_TEST_TAG, errno);
    }
    return ret;
}
}

class TaskQueue {
public:
    static TaskQueue& GetInstance(TestThreadEnum testThreadEnum);
    bool AddTask(const std::function<void()>& task);
private:
    TaskQueue();

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
    std::mutex mutex;
};

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

TaskQueue::TaskQueue()
{
    auto executor = std::unique_ptr<Executor>(new Executor(*this));
    executor_ = executor.get();
    OHOS::HiviewDFX::EpollManager::GetInstance().AddListener(std::move(executor));
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

void ClearTempFiles()
{
    std::vector<std::string> files;
    OHOS::GetDirFiles(TEST_TEMP_FILE_PATH, files);
    for (const auto& file : files) {
        OHOS::RemoveFile(file);
    }
}

uint64_t CountTempFiles()
{
    std::vector<std::string> files;
    OHOS::GetDirFiles(TEST_TEMP_FILE_PATH, files);
    return files.size();
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
    std::thread([] {
        auto& helper = OHOS::HiviewDFX::EpollManager::GetInstance();
        helper.Init(maxEpollEvent);
        TaskQueue::GetInstance(TestThreadEnum::HELPER);
        OHOS::HiviewDFX::FaultLoggerDaemon::GetInstance().InitHelperServer();
        helper.StartEpoll(maxConnection);
    }).detach();
    std::thread([] {
        auto& main = OHOS::HiviewDFX::EpollManager::GetInstance();
        main.Init(maxEpollEvent);
        TaskQueue::GetInstance(TestThreadEnum::MAIN);
        OHOS::HiviewDFX::FaultLoggerDaemon::GetInstance().InitMainServer();
        constexpr auto epollTimeoutInMilliseconds = 3 * 1000;
        main.StartEpoll(maxConnection, epollTimeoutInMilliseconds);
    }).detach();
    constexpr int32_t faultLoggerdInitTime = 2;
    // Pause for two seconds to wait for the server to initialize.
    std::this_thread::sleep_for(std::chrono::seconds(faultLoggerdInitTime));
}

bool FaultLoggerdTestServer::AddTask(TestThreadEnum type, const std::function<void()>& task)
{
    return TaskQueue::GetInstance(type).AddTask(task);
}