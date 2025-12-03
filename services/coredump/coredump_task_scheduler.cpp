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
#include "coredump_task_scheduler.h"
#include "coredump_session_manager.h"
#include "dfx_util.h"
#include "epoll_manager.h"
#include "fault_logger_daemon.h"

namespace OHOS {
namespace HiviewDFX {

void CoredumpTaskScheduler::ScheduleCancelTime(SessionId sessionId, int timeoutMs)
{
    auto session = sessionManager_.GetSession(sessionId);
    if (!session) {
        return;
    }

    auto removeTask = [sessionId]() {
        CoredumpSessionManager::Instance().RemoveSession(sessionId);
        DFXLOGI("timer real remove %{public}d", sessionId);
    };

    auto endTime = session->endTime;
    constexpr int32_t maxDelaySec = 30; // one coredump session is fail
    auto curTime = GetAbsTimeMilliSeconds();
    int32_t delaySec = endTime > curTime ? static_cast<int32_t>((endTime - curTime) / 1000) : 0;
    delaySec = std::min(delaySec, maxDelaySec);

    auto delayTask = TimerTaskAdapter::CreateInstance(removeTask, delaySec);
    if (delayTask) {
        session->timerFd = delayTask->GetFd();
        EpollManager::GetInstance().AddListener(std::move(delayTask));
    }
}

void CoredumpTaskScheduler::CancelTimeout(SessionId sessionId)
{
    auto session = sessionManager_.GetSession(sessionId);
    if (!session) {
        return;
    }

    EpollManager::GetInstance().RemoveListener(session->timerFd);
}
}
}
