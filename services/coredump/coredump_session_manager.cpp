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
#include "coredump_session_manager.h"
#include <unistd.h>

#include "coredump_task_scheduler.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {

CoredumpSessionManager& CoredumpSessionManager::Instance()
{
    static CoredumpSessionManager sessionManager;
    return sessionManager;
}

SessionId CoredumpSessionManager::CreateSession(const CreateCoredumpRequest& request)
{
    CoredumpSession session;
    session.sessionId = request.targetPid;
    session.clientFd = request.clientFd;
    session.status = CoredumpStatus::PENDING;
    session.endTime = request.endTime;
    session.startTime = GetTimeMilliSeconds();

    sessions_.emplace(session.sessionId, std::move(session));

    DFXLOGI("success create session id %{public}d ", session.sessionId);

    return session.sessionId;
}

CoredumpSession* CoredumpSessionManager::GetSession(SessionId sessionId)
{
    auto session = sessions_.find(sessionId);
    if (session == sessions_.end()) {
        DFXLOGW("fail to get sessoin by id %{public}d", sessionId);
        return nullptr;
    }
    return &session->second;
}

void CoredumpSessionManager::RemoveSession(SessionId sessionId)
{
    if (auto it = sessions_.find(sessionId); it != sessions_.end()) {
        DFXLOGI("success remove session %{public}d", sessionId);
        if (int& fd = it->second.clientFd; fd >= 0) {
            close(fd);
            fd = -1;
        }

        CoredumpTaskScheduler().CancelTimeout(sessionId);
        sessions_.erase(it);
    }
}
}
}
