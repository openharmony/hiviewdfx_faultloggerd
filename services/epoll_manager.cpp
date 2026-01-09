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
#include <atomic>
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

constexpr uint64_t NS_PER_S = MS_PER_S * US_PER_MS * NS_PER_US;
constexpr uint64_t US_PER_S = MS_PER_S * US_PER_MS;
constexpr uint8_t TIMESTAMP_BITS = 60;
constexpr uint64_t TIMESTAMP_MASK = (static_cast<uint64_t>(1) << TIMESTAMP_BITS) - 1;
constexpr uint8_t SEQUENCE_START_BIT = TIMESTAMP_BITS;

bool SetTimeOptionForTimeFd(int32_t fd, uint64_t delayTimeInNs, uint64_t intervalTimeInNs)
{
    if (fd < 0) {
        return false;
    }
    struct itimerspec timeOption{};
    timeOption.it_value.tv_sec =
        static_cast<decltype(timeOption.it_value.tv_sec)>(delayTimeInNs / NS_PER_S);
    timeOption.it_value.tv_nsec =
        static_cast<decltype(timeOption.it_value.tv_nsec)>(delayTimeInNs % NS_PER_S);
    timeOption.it_interval.tv_sec =
        static_cast<decltype(timeOption.it_interval.tv_sec)>(intervalTimeInNs / NS_PER_S);
    timeOption.it_interval.tv_nsec =
        static_cast<decltype(timeOption.it_interval.tv_nsec)>(intervalTimeInNs % NS_PER_S);
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

/**
 * @brief Combined uint64_t value with dual-field encoding
 * @details Bit allocation for the 64-bit unsigned integer:
 *          - Bits 60-63 (4 high-order bits): Sequence number (range: 0~15, for batch/instance identification)
 *          - Bits 0-59 (60 low-order bits): microSecond-level timestamp (unsigned 60-bit value)
 *          - Timestamp range: 0 to ~36500 years (max value: 2^60 - 1 us ≈ 1.15×10^18 us = 36534 years)
 * @note To parse:
 *       - Sequence number = (value >> 60) & 0x0F;
 *       - Timestamp (us) = value & TIMESTAMP_MASK; (mask for 60 low bits)
 */
inline uint64_t CalculateDelayTaskId(uint64_t executeTimeInMicroSecond)
{
    static std::atomic<uint8_t> delaySequence{0};
    return (static_cast<uint64_t>(delaySequence.fetch_add(1)) << SEQUENCE_START_BIT) | executeTimeInMicroSecond;
}
}

uint64_t GetMicroSecondsSinceBoot()
{
    struct timespec times{};
    if (clock_gettime(CLOCK_BOOTTIME, &times) == -1) {
        DFXLOGE("%{public}s :: failed get time for %{public}d", EPOLL_MANAGER, errno);
        return 0;
    }
    return static_cast<uint64_t>(times.tv_sec * US_PER_S + times.tv_nsec / NS_PER_US);
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

EpollListener* EpollManager::GetTargetListener(int32_t fd) const
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
    return SetTimeOptionForTimeFd(GetFd(), static_cast<uint64_t>(delayTimeInS) * NS_PER_S,
        static_cast<uint64_t>(intervalTimeInS) * NS_PER_S);
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
    if (!SetTimeOptionForTimeFd(executor->GetFd(), delayTimeInS * NS_PER_S, NS_PER_S)) {
        return false;
    }
    executor_ = executor.get();
    if (!EpollManager::GetInstance().AddListener(std::move(executor))) {
        return false;
    }
    return true;
}

uint64_t DelayTaskQueue::AddDelayTask(std::function<void()> workFunc, uint32_t delayTimeInS)
{
    if (delayTimeInS == 0 || !workFunc) {
        return 0;
    }
    if (executor_ == nullptr && !InitExecutor(delayTimeInS)) {
        return 0;
    }
    const auto delayTimeInMicroSeconds =  static_cast<uint64_t>(delayTimeInS) * MS_PER_S * US_PER_MS;
    if (GetMicroSecondsSinceBoot() == 0) {
        return 0;
    }
    const auto executeTimeInMicroSecond = GetMicroSecondsSinceBoot() + delayTimeInMicroSeconds;
    auto insertPos = std::find_if(delayTasks_.begin(), delayTasks_.end(),
        [executeTimeInMicroSecond](const std::pair<uint64_t, std::function<void()>>& existingTask) {
            return (existingTask.first & TIMESTAMP_MASK) > executeTimeInMicroSecond;
        });
    if (insertPos == delayTasks_.begin()) {
        SetTimeOptionForTimeFd(executor_->GetFd(), delayTimeInMicroSeconds * NS_PER_US, NS_PER_S);
    }
    auto delayTaskId = CalculateDelayTaskId(executeTimeInMicroSecond);
    delayTasks_.insert(insertPos, std::make_pair(delayTaskId, std::move(workFunc)));
    return delayTaskId;
}

bool DelayTaskQueue::RemoveDelayTask(uint64_t delayTaskId)
{
    auto it = std::find_if(delayTasks_.begin(), delayTasks_.end(),
        [&](const std::pair<uint64_t, std::function<void()>>& item) {
            return item.first == delayTaskId;
        });
    if (it == delayTasks_.end()) {
        return false;
    }
    bool isBegin = (it == delayTasks_.begin());
    delayTasks_.erase(it);
    if (isBegin && executor_ != nullptr && delayTasks_.empty()) {
        SetTimeOptionForTimeFd(executor_->GetFd(), 1, NS_PER_S);
    }
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
        auto currentTimeInMicroSecond = GetMicroSecondsSinceBoot();
        auto delayTaskId = delayTaskQueue_.delayTasks_.front().first;
        auto firstTaskExecuteTime = delayTaskId & TIMESTAMP_MASK;
        if (firstTaskExecuteTime > currentTimeInMicroSecond) {
            auto nextTaskDelayTime = firstTaskExecuteTime - currentTimeInMicroSecond;
            SetTimeOptionForTimeFd(GetFd(), nextTaskDelayTime * NS_PER_US, NS_PER_S);
            return;
        }
        delayTaskQueue_.delayTasks_.front().second();
        delayTaskQueue_.RemoveDelayTask(delayTaskId);
    }
    EpollManager::GetInstance().RemoveListener(GetFd());
}
}
}