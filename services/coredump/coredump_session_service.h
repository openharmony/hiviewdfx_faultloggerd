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
#ifndef COREDUMP_SESSION_SERVICE_H
#define COREDUMP_SESSION_SERVICE_H

#include "coredump_model.h"
#include "coredump_session_manager.h"
#include "dfx_socket_request.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpSessionService {
public:
    CoredumpSessionService():sessionManager_(CoredumpSessionManager::Instance()) {}

    SessionId CreateSession(const CreateCoredumpRequest& request) const;
    bool CancelSession(SessionId sessionId) const;
    bool UpdateWorkerPid(SessionId sessionId, pid_t workerPid) const;
    void UpdateReport(SessionId sessionId, const CoredumpCallbackReport& rpt) const;

    int GetClientFd(SessionId sessionId) const;

    bool WriteTimeout(SessionId sessionId) const;
    bool WriteResult(SessionId sessionId) const;
private:
    bool WriteResult(SessionId sessionId, const CoreDumpResult& result) const;
    bool ReportCoredumpStatistics(SessionId sessionId) const;
    CoredumpSessionManager& sessionManager_;
};
}
}
#endif
