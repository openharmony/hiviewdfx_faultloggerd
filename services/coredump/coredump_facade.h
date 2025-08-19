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
#ifndef COREDUMP_FACADE_H
#define COREDUMP_FACADE_H

#include "coredump_session_service.h"
#include "coredump_signal_service.h"
#include "coredump_task_scheduler.h"
#include "coredump_session_state.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpFacade {
public:
    bool CreateCoredump(const CreateCoredumpRequest& req);
    void CancelCoredump(SessionId sessionId);
    bool UpdateWorkerPid(SessionId sessionId, pid_t workerPid);
    void ReportResult(SessionId sessionId, const CoredumpCallbackReport& rpt);
    void OnTimeout(SessionId sessionId);
private:
    CoredumpSessionService sessionService_;
    CoredumpSignalService signalService_;
    CoredumpTaskScheduler taskScheduler_;
    SessionStateContext stateContext_;
};
}
}
#endif
