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
#include "coredump_session_state.h"

#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {

std::unique_ptr<SessionState> SessionStateContext::CreateState(CoredumpStatus status)
{
    switch (status) {
        case CoredumpStatus::PENDING:
            return std::make_unique<PendingState>();
        case CoredumpStatus::RUNNING:
            return std::make_unique<RunningState>();
        case CoredumpStatus::CANCEL_PENDING:
            return std::make_unique<CancelPendingState>();
        default:
            DFXLOGE("not support current status %{public}s", CoredumpSession::StatusToString(status).c_str());
            return nullptr;
    }
}

void SessionStateContext::HandleEvent(CoredumpSession& session, CoredumpEvent event)
{
    auto stateHandler = CreateState(session.status);
    if (!stateHandler) {
        return;
    }
    stateHandler->OnEvent(event, session);
}

void PendingState::OnEvent(CoredumpEvent event, CoredumpSession& session)
{
    switch (event) {
        case CoredumpEvent::UPDATE_DUMPER:
            session.status = CoredumpStatus::RUNNING;
            break;
        case CoredumpEvent::CANCEL:
            session.cancelRequest = true;
            session.status = CoredumpStatus::CANCEL_PENDING;
            break;
        case CoredumpEvent::TIMEOUT:
            sessionService_.WriteTimeout(session.sessionId);
            session.status = CoredumpStatus::CANCEL_TIMEOUT;
            break;
        default:
            DFXLOGW("pending state recevie invalid event");
            break;
    }
}

void RunningState::OnEvent(CoredumpEvent event, CoredumpSession& session)
{
    switch (event) {
        case CoredumpEvent::REPORT_SUCCESS:
            sessionService_.WriteResult(session.sessionId);
            session.status = CoredumpStatus::SUCCESS;
            break;
        case CoredumpEvent::REPORT_FAIL:
            sessionService_.WriteResult(session.sessionId);
            session.status = CoredumpStatus::FAILED;
            break;
        case CoredumpEvent::CANCEL:
            signalService_.SendCancelSignal(session.workerPid);
            sessionService_.WriteResult(session.sessionId);
            session.status = CoredumpStatus::CANCELED;
            break;
        default:
            DFXLOGI("running state receive invalid event");
            break;
    }
}

void CancelPendingState::OnEvent(CoredumpEvent event, CoredumpSession& session)
{
    switch (event) {
        case CoredumpEvent::UPDATE_DUMPER:
            signalService_.SendCancelSignal(session.workerPid);
            sessionService_.WriteResult(session.sessionId);
            session.status = CoredumpStatus::CANCELED;
            break;
        case CoredumpEvent::TIMEOUT:
            sessionService_.WriteTimeout(session.sessionId);
            session.status = CoredumpStatus::CANCEL_TIMEOUT;
            break;
        default:
            DFXLOGI("cancel pending receive invalid event");
            break;
    }
}
}
}
