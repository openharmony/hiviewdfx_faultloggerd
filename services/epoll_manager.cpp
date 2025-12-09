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

#include "epoll_manager.h"

#include <algorithm>
#include <thread>
#include <vector>

#include <unistd.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "dfx_define.h"
#include "dfx_log.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr const char *const EPOLL_MANAGER = "EPOLL_MANAGER";

constexpr uint64_t SECONDS_TO_NANOSCONDS = 1000 * 1000 * 1000;

bool SetTimeOptionForTimeFd(int32_t fd, uint64_t delayTimeInNs, uint64_t intervalTimeInNs)
{
    if (fd < 0) {
        return false;
    }
    struct itimerspec timeOption{};
    timeOption.it_value.tv_sec =
        static_cast<decltype(timeOption.it_value.tv_sec)>(delayTimeInNs / SECONDS_TO_NANOSCONDS);
    timeOption.it_value.tv_nsec =
        static_cast<decltype(timeOption.it_value.tv_nsec)>(delayTimeInNs % SECONDS_TO_NANOSCONDS);
    timeOption.it_interval.tv_sec =
        static_cast<decltype(timeOption.it_interval.tv_sec)>(intervalTimeInNs / SECONDS_TO_NANOSCONDS);
    timeOption.it_interval.tv_nsec =
        static_cast<decltype(timeOption.it_interval.tv_nsec)>(intervalTimeInNs % SECONDS_TO_NANOSCONDS);
    if (timerfd_settime(fd, 0, &timeOption, nullptr) == -1) {
        DFXLOGE("%{public}s :: failed to set delay time for fd, errno: %{public}d.", EPOLL_MANAGER, errno);
        return false;
    }
    return true;
}

SmartFd CreateTimeFd()
{
    SmartFd timefd{timerfd_create(CLOCK_MONOTONIC, 0)};
    if (!timefd) {
        DFXLOGE("%{public}s :: failed to create time fd, errno: %{public}d", EPOLL_MANAGER, errno);
    }
    return timefd;
}
}

uint64_t GetElapsedNanoSecondsSinceBoot()
{
    struct timespec times{};
    if (clock_gettime(CLOCK_BOOTTIME, &times) == -1) {
        DFXLOGE("%{public}s :: failed get time for %{public}d", EPOLL_MANAGER, errno);
        return 0;
    }
    constexpr int64_t secondToNanosecond = 1 * 1000 * 1000 * 1000;
    return static_cast<uint64_t>(times.tv_sec * secondToNanosecond + times.tv_nsec);
}

EpollListener::EpollListener(SmartFd fd, bool persist) : fd_(std::move(fd)), persist_(persist) {}

bool EpollListener::IsPersist() const
{
    return persist_;
}

int32_t EpollListener::GetFd() const
{
    return fd_.GetFd();
}

EpollManager &EpollManager::GetInstance()
{
    static thread_local EpollManager mainEpollManager;
    return mainEpollManager;
}

EpollManager::~EpollManager()
{
    StopEpoll();
}

bool EpollManager::AddEpollEvent(EpollListener& epollListener) const
{
    if (!eventFd_) {
        return false;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = epollListener.GetFd();
    if (epoll_ctl(eventFd_.GetFd(), EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
        DFXLOGE("%s :: Failed to epoll ctl add fd %{public}d, errno %{public}d",
            EPOLL_MANAGER, epollListener.GetFd(), errno);
        return false;
    }
    return true;
}

bool EpollManager::DelEpollEvent(int32_t fd) const
{
    if (!eventFd_) {
        return false;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(eventFd_.GetFd(), EPOLL_CTL_DEL, fd, &ev) < 0) {
        DFXLOGW("%s :: Failed to epoll ctl delete Fd %{public}d, errno %{public}d", EPOLL_MANAGER, fd, errno);
        return false;
    }
    return true;
}

bool EpollManager::AddListener(std::unique_ptr<EpollListener> epollListener)
{
    if (!epollListener || epollListener->GetFd() < 0 || !AddEpollEvent(*epollListener)) {
        return false;
    }
    epollListener->IsPersist() ? listeners_.push_back(std::move(epollListener)) :
        listeners_.push_front(std::move(epollListener));
    return true;
}

bool EpollManager::RemoveListener(int32_t fd)
{
    if (fd < 0 || !DelEpollEvent(fd)) {
        return false;
    }
    listeners_.remove_if([fd](const std::unique_ptr<EpollListener>& epollLister) {
        return epollLister->GetFd() == fd;
    });
    return true;
}

EpollListener* EpollManager::GetTargetListener(int32_t fd)
{
    auto iter = std::find_if(listeners_.begin(), listeners_.end(),
        [fd](const std::unique_ptr<EpollListener>& listener) {
            return listener->GetFd() == fd;
        });
    return iter == listeners_.end() ? nullptr : iter->get();
}

bool EpollManager::Init(int maxPollEvent)
{
    eventFd_ = SmartFd{epoll_create(maxPollEvent)};
    if (!eventFd_) {
        DFXLOGE("%s :: Failed to create eventFd.", EPOLL_MANAGER);
        return false;
    }
    return true;
}

void EpollManager::StartEpoll(int maxConnection, int epollTimeoutInMilliseconds)
{
    std::vector<epoll_event> events(maxConnection);
    while (eventFd_) {
        int32_t timeOut = (!listeners_.empty() && !listeners_.front()->IsPersist()) ? epollTimeoutInMilliseconds : -1;
        int epollNum = OHOS_TEMP_FAILURE_RETRY(epoll_wait(eventFd_.GetFd(), events.data(), maxConnection, timeOut));
        std::lock_guard<std::mutex> lck(epollMutex_);
        if (epollNum < 0 || !eventFd_) {
            continue;
        }
        if (epollNum == 0) {
            DFXLOGE("%{public}s :: epoll_wait timeout, clean up all temporary event listeners.", EPOLL_MANAGER);
            listeners_.remove_if([](const std::unique_ptr<EpollListener>& epollLister) {
                return !epollLister->IsPersist();
            });
            continue;
        }
        for (int i = 0; i < epollNum; i++) {
            if (!(events[i].events & EPOLLIN)) {
                DFXLOGE("%{public}s :: client fd %{public}d disconnected", EPOLL_MANAGER, events[i].data.fd);
                RemoveListener(events[i].data.fd);
                continue;
            }
            const auto listener = GetTargetListener(events[i].data.fd);
            if (listener == nullptr) {
                DelEpollEvent(events[i].data.fd);
                continue;
            }
            // The listener lifecycle is only guaranteed to be valid before the OnEventPoll call.
            bool isPersist = listener->IsPersist();
            listener->OnEventPoll();
            if (!isPersist) {
                RemoveListener(events[i].data.fd);
            }
        }
    }
}

void EpollManager::StopEpoll()
{
    std::lock_guard<std::mutex> lck(epollMutex_);
    if (eventFd_) {
        for (const auto& listener : listeners_) {
            (void)DelEpollEvent(listener->GetFd());
        }
        eventFd_.Reset();
    }
}

TimerTask::TimerTask(bool persist) : EpollListener(CreateTimeFd(), persist) {}

bool TimerTask::SetTimeOption(int32_t delayTimeInS, int32_t intervalTimeInS)
{
    if (delayTimeInS < 0 || intervalTimeInS < 0) {
        return false;
    }
    return SetTimeOptionForTimeFd(GetFd(), static_cast<uint64_t>(delayTimeInS) * SECONDS_TO_NANOSCONDS,
        static_cast<uint64_t>(intervalTimeInS) * SECONDS_TO_NANOSCONDS);
}

void TimerTask::OnEventPoll()
{
    uint64_t exp = 0;
    auto ret = OHOS_TEMP_FAILURE_RETRY(read(GetFd(), &exp, sizeof(exp)));
    if (ret < 0 || static_cast<uint64_t>(ret) != sizeof(exp)) {
        DFXLOGE("%{public}s :: failed read time fd %{public}" PRId32, EPOLL_MANAGER, GetFd());
    } else {
        OnTimer();
    }
}

std::unique_ptr<TimerTask> TimerTaskAdapter::CreateInstance(std::function<void()> workFunc,
    int32_t delayTimeInS, int32_t intervalTimeInS)
{
    if (!workFunc) {
        return nullptr;
    }
    auto task = std::unique_ptr<TimerTaskAdapter>(new (std::nothrow)TimerTaskAdapter(workFunc,
        intervalTimeInS > 0));
    if (task == nullptr || !task->SetTimeOption(delayTimeInS, intervalTimeInS)) {
        return nullptr;
    }
    return task;
}

TimerTaskAdapter::TimerTaskAdapter(std::function<void()>& workFunc, bool persist) : TimerTask(persist),
    work_(std::move(workFunc)) {}

void TimerTaskAdapter::OnTimer()
{
    work_();
}

DelayTaskQueue& DelayTaskQueue::GetInstance()
{
    static thread_local DelayTaskQueue queue;
    return queue;
}

bool DelayTaskQueue::InitExecutor(uint32_t delayTimeInS)
{
    auto executor = std::unique_ptr<Executor>(new(std::nothrow) Executor(*this));
    if (executor == nullptr) {
        return false;
    }
    if (!SetTimeOptionForTimeFd(executor->GetFd(), delayTimeInS * SECONDS_TO_NANOSCONDS, SECONDS_TO_NANOSCONDS)) {
        return false;
    }
    executor_ = executor.get();
    if (!EpollManager::GetInstance().AddListener(std::move(executor))) {
        return false;
    }
    return true;
}

bool DelayTaskQueue::AddDelayTask(std::function<void()> workFunc, uint32_t delayTimeInS)
{
    if (delayTimeInS == 0 || !workFunc) {
        return false;
    }
    if (executor_ == nullptr && !InitExecutor(delayTimeInS)) {
        return false;
    }
    const auto delayTimeInNanoSeconds =  static_cast<uint64_t>(delayTimeInS) * SECONDS_TO_NANOSCONDS;
    if (GetElapsedNanoSecondsSinceBoot() == 0) {
        return false;
    }
    const auto executeTime = GetElapsedNanoSecondsSinceBoot() + delayTimeInNanoSeconds;
    auto insertPos = std::find_if(delayTasks_.begin(), delayTasks_.end(),
        [executeTime](const std::pair<uint64_t, std::function<void()>>& existingTask) {
            return existingTask.first > executeTime;
        });
    if (insertPos == delayTasks_.begin()) {
        SetTimeOptionForTimeFd(executor_->GetFd(), delayTimeInNanoSeconds, SECONDS_TO_NANOSCONDS);
    }
    delayTasks_.insert(insertPos, std::make_pair(executeTime, std::move(workFunc)));
    return true;
}

DelayTaskQueue::Executor::~Executor()
{
    /**
     * Set the executor_ in the outer class to null to indicate that the existing executor has been destroyed.
     */
    delayTaskQueue_.executor_ = nullptr;
}

void DelayTaskQueue::Executor::OnTimer()
{
    while (!delayTaskQueue_.delayTasks_.empty()) {
        auto currentTime = GetElapsedNanoSecondsSinceBoot();
        if (delayTaskQueue_.delayTasks_.front().first > currentTime) {
            auto nextTaskDelayTime = delayTaskQueue_.delayTasks_.front().first - currentTime;
            SetTimeOptionForTimeFd(GetFd(), nextTaskDelayTime, SECONDS_TO_NANOSCONDS);
            return;
        }
        delayTaskQueue_.delayTasks_.front().second();
        delayTaskQueue_.delayTasks_.pop_front();
    }
    EpollManager::GetInstance().RemoveListener(GetFd());
}
}
}