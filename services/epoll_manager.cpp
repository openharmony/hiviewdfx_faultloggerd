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
}

EpollListener::EpollListener(int32_t fd, bool persist) : fd_(fd), persist_(persist) {}

bool EpollListener::IsPersist() const
{
    return persist_;
}

int32_t EpollListener::GetFd() const
{
    return fd_;
}

EpollManager::~EpollManager()
{
    StopEpoll();
}

bool EpollManager::AddEpollEvent(EpollListener& epollListener) const
{
    if (eventFd_ < 0) {
        return false;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = epollListener.GetFd();
    if (epoll_ctl(eventFd_, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
        DFXLOGE("%s :: Failed to epoll ctl add fd %{public}d, errno %{public}d",
            EPOLL_MANAGER, epollListener.GetFd(), errno);
        return false;
    }
    return true;
}

bool EpollManager::DelEpollEvent(int32_t fd) const
{
    if (eventFd_ < 0) {
        return false;
    }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(eventFd_, EPOLL_CTL_DEL, fd, &ev) < 0) {
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
    eventFd_ = epoll_create(maxPollEvent);
    if (eventFd_ < 0) {
        DFXLOGE("%s :: Failed to create eventFd.", EPOLL_MANAGER);
        return false;
    }
    fdsan_exchange_owner_tag(eventFd_, 0, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, LOG_DOMAIN));
    return true;
}

void EpollManager::StartEpoll(int maxConnection, int epollTimeoutInMilliseconds)
{
    std::vector<epoll_event> events(maxConnection);
    while (eventFd_ >= 0) {
        int32_t timeOut = (!listeners_.empty() && !listeners_.front()->IsPersist()) ? epollTimeoutInMilliseconds : -1;
        int epollNum = OHOS_TEMP_FAILURE_RETRY(epoll_wait(eventFd_, events.data(), maxConnection, timeOut));
        std::lock_guard<std::mutex> lck(epollMutex_);
        if (epollNum < 0 || eventFd_ < 0) {
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
                RemoveListener(events[i].data.fd);
                continue;
            }
            listener->OnEventPoll();
            if (!listener->IsPersist()) {
                RemoveListener(events[i].data.fd);
            }
        }
    }
}

void EpollManager::StopEpoll()
{
    std::lock_guard<std::mutex> lck(epollMutex_);
    if (eventFd_ >= 0) {
        for (const auto& listener : listeners_) {
            (void)DelEpollEvent(listener->GetFd());
        }
        fdsan_close_with_tag(eventFd_, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, LOG_DOMAIN));
        eventFd_ = -1;
    }
}

std::unique_ptr<DelayTask> DelayTask::CreateInstance(std::function<void()> workFunc, int32_t timeout)
{
    if (timeout <= 0) {
        return nullptr;
    }
    int timefd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timefd == -1) {
        DFXLOGE("%{public}s :: failed to create time fd, errno: %{public}d", EPOLL_MANAGER, errno);
        return nullptr;
    }
    struct itimerspec timeOption{};
    timeOption.it_value.tv_sec = timeout;
    if (timerfd_settime(timefd, 0, &timeOption, nullptr) == -1) {
        close(timefd);
        DFXLOGE("%{public}s :: failed to set delay time for fd, errno: %{public}d.", EPOLL_MANAGER, errno);
        return nullptr;
    }
    return std::unique_ptr<DelayTask>(new (std::nothrow)DelayTask(workFunc, timefd));
}

DelayTask::DelayTask(std::function<void()> workFunc, int32_t timeFd)
    : EpollListener(timeFd), work_(std::move(workFunc)) {}

void DelayTask::OnEventPoll()
{
    uint64_t exp;
    auto ret = OHOS_TEMP_FAILURE_RETRY(read(GetFd(), &exp, sizeof(exp)));
    if (ret < 0 || static_cast<uint64_t>(ret) != sizeof(exp)) {
        DFXLOGE("%{public}s :: failed read time fd %{public}" PRId32, EPOLL_MANAGER, GetFd());
    } else {
        work_();
    }
}
}
}