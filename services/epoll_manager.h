/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#ifndef EPOLL_MANAGER_H_
#define EPOLL_MANAGER_H_

#include <functional>
#include <list>
#include <memory>
#include <mutex>

#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {

class EpollListener {
public:
    explicit EpollListener(SmartFd fd, bool persist = false);
    virtual ~EpollListener() = default;
    virtual void OnEventPoll() = 0;
    virtual bool IsPersist() const final;
    int32_t GetFd() const;
private:
    const SmartFd fd_;
    bool persist_{false};
};

class EpollManager {
public:
    static EpollManager& GetInstance();
    EpollManager(const EpollManager&) = delete;
    EpollManager& operator=(const EpollManager&) = delete;
    EpollManager(EpollManager&&) noexcept = delete;
    EpollManager& operator=(EpollManager&&) noexcept = delete;
    bool Init(int maxPollEvent);
    void StartEpoll(int maxConnection, int epollTimeoutInMilliseconds = -1);
    void StopEpoll();
    bool AddListener(std::unique_ptr<EpollListener> epollListener);
    bool RemoveListener(int32_t fd);
private:
    EpollManager() = default;
    ~EpollManager();
    bool AddEpollEvent(EpollListener& epollListener) const;
    bool DelEpollEvent(int32_t fd) const;
    EpollListener* GetTargetListener(int32_t fd);
    std::list<std::unique_ptr<EpollListener>> listeners_;
    SmartFd eventFd_;
    std::mutex epollMutex_;
};

class TimerTask : public EpollListener {
public:
    explicit TimerTask(bool persist);
    ~TimerTask() override = default;
    void OnEventPoll() final;
protected:
    virtual void OnTimer() = 0;
    bool SetTimeOption(int32_t delayTimeInS, int32_t intervalTimeInS);
};

class TimerTaskAdapter : public TimerTask {
public:
    static std::unique_ptr<TimerTask> CreateInstance(std::function<void()> workFunc,
        int32_t delayTimeInS, int32_t intervalTimeInS = 0);
    TimerTaskAdapter(const TimerTaskAdapter&) = delete;
    TimerTaskAdapter& operator=(const TimerTaskAdapter&) = delete;
    ~TimerTaskAdapter() override = default;
    void OnTimer() override;
private:
    TimerTaskAdapter(std::function<void()>& workFunc, bool persist);
    std::function<void()> work_;
};

class DelayTaskQueue {
public:
    static DelayTaskQueue& GetInstance();
    DelayTaskQueue& operator=(const DelayTaskQueue&) = delete;
    DelayTaskQueue(const DelayTaskQueue&) = delete;
    DelayTaskQueue(DelayTaskQueue&&) = delete;
    DelayTaskQueue& operator=(DelayTaskQueue&&) = delete;
    bool AddDelayTask(std::function<void()> workFunc, uint32_t delayTimeInS);
private:
    class Executor final : public TimerTask {
    public:
        explicit Executor(DelayTaskQueue& queue) : TimerTask(true), delayTaskQueue_(queue) {};
        ~Executor() override;
    protected:
        void OnTimer() final;
        DelayTaskQueue& delayTaskQueue_;
    };
    DelayTaskQueue() = default;
    ~DelayTaskQueue() = default;
    bool InitExecutor(uint32_t delayTimeInS);
    /**
     * Used to check if there is already an executor and retrieve the fd bound to this executor.
     */
    const Executor* executor_{};
    std::list<std::pair<uint64_t, std::function<void()>>> delayTasks_;
};
}
}

#endif // EPOLL_MANAGER_H_
