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
    EpollManager() = default;
    ~EpollManager();
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
    bool SetTimeOption(int32_t delayTime, int32_t periodicity);
private:
    static SmartFd CreateTimeFd();
};

class DelayTask : public TimerTask {
public:
    DelayTask(const DelayTask&) = delete;
    DelayTask& operator=(const DelayTask&) = delete;
    static std::unique_ptr<DelayTask> CreateInstance(std::function<void()> workFunc, int32_t timeout);
    void OnTimer() override;
private:
    explicit DelayTask(std::function<void()> workFunc);
    std::function<void()> work_;
};
}
}

#endif // EPOLL_MANAGER_H_
