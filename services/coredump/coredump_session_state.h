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
#ifndef COREDUMP_SESSION_STATE_H
#define COREDUMP_SESSION_STATE_H

#include "coredump_task_scheduler.h"
#include "coredump_signal_service.h"
#include "coredump_session_service.h"

namespace OHOS {
namespace HiviewDFX {

class SessionState;

class SessionStateContext {
public:
    void HandleEvent(CoredumpSession& session, CoredumpEvent event);
    static std::unique_ptr<SessionState> CreateState(CoredumpStatus status);
};

class SessionState {
public:
    virtual ~SessionState() = default;
    virtual void OnEvent(CoredumpEvent event, CoredumpSession& session) = 0;
};

class PendingState : public SessionState {
public:
    void OnEvent(CoredumpEvent event, CoredumpSession& session) override;
private:
    CoredumpSessionService sessionService_;
};

class RunningState : public SessionState {
public:
    void OnEvent(CoredumpEvent event, CoredumpSession& session) override;
private:
    CoredumpSessionService sessionService_;
    CoredumpSignalService signalService_;
};

class CancelPendingState : public SessionState {
public:
    void OnEvent(CoredumpEvent event, CoredumpSession& session) override;
private:
    CoredumpSessionService sessionService_;
    CoredumpSignalService signalService_;
};
}
}
#endif
