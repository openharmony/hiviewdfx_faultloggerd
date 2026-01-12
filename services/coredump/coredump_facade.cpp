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
#include "coredump_facade.h"
#include "coredump_session_manager.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
bool CoredumpFacade::CreateCoredump(const CreateCoredumpRequest& req)
{
    sessionService_.CreateSession(req);
    signalService_.SendStartSignal(req.targetPid);
    constexpr int32_t maxCoredumpDelayTime = 10 * 1000;
    taskScheduler_.ScheduleCancelTime(req.targetPid, maxCoredumpDelayTime);
    return true;
}

void CoredumpFacade::CancelCoredump(SessionId sessionId)
{
    sessionService_.CancelSession(sessionId);
    const auto session = CoredumpSessionManager::Instance().GetSession(sessionId);
    if (!session) {
        return;
    }
    stateContext_.HandleEvent(*session, CoredumpEvent::CANCEL);
}

bool CoredumpFacade::UpdateWorkerPid(SessionId sessionId, pid_t workerPid)
{
    DFXLOGI("%{public}s sessionId %{public}d, workerPid  %{public}d", __func__, sessionId, workerPid);
    sessionService_.UpdateWorkerPid(sessionId, workerPid);

    const auto session = CoredumpSessionManager::Instance().GetSession(sessionId);
    if (!session) {
        return false;
    }
    stateContext_.HandleEvent(*session, CoredumpEvent::UPDATE_DUMPER);
    return true;
}

void CoredumpFacade::ReportResult(SessionId sessionId, const CoredumpCallbackReport& rpt)
{
    DFXLOGI("%{public}s sessionId %{public}d  path %{public}s", __func__, sessionId, rpt.filePath.c_str());
    sessionService_.UpdateReport(sessionId, rpt);
    const auto session = CoredumpSessionManager::Instance().GetSession(sessionId);
    if (!session) {
        return;
    }

    stateContext_.HandleEvent(*session,
        rpt.status == CoredumpStatus::SUCCESS ? CoredumpEvent::REPORT_SUCCESS : CoredumpEvent::REPORT_FAIL);
}

void CoredumpFacade::OnTimeout(SessionId sessionId)
{
    const auto session = CoredumpSessionManager::Instance().GetSession(sessionId);
    if (!session) {
        return;
    }
    stateContext_.HandleEvent(*session, CoredumpEvent::TIMEOUT);
}
}
}
