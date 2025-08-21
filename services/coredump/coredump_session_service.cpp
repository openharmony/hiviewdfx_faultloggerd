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
#include "coredump_session_service.h"
#include "securec.h"
#include "faultloggerd_socket.h"

namespace OHOS {
namespace HiviewDFX {
SessionId CoredumpSessionService::CreateSession(const CreateCoredumpRequest& request)
{
    return sessionManager_.CreateSession(request);
}

bool CoredumpSessionService::CancelSession(SessionId sessionId)
{
    auto session = sessionManager_.GetSession(sessionId);
    if (!session) {
        return false;
    }
    session->cancelRequest = true;
    return true;
}

bool CoredumpSessionService::UpdateWorkerPid(SessionId sessionId, pid_t workerPid)
{
    auto session = sessionManager_.GetSession(sessionId);
    if (!session) {
        return false;
    }
    session->workerPid = workerPid;
    return true;
}

void CoredumpSessionService::UpdateReport(SessionId sessionId, const CoredumpCallbackReport& rpt)
{
    auto session = sessionManager_.GetSession(sessionId);
    if (!session) {
        return;
    }
    session->filePath = rpt.filePath;
    session->errorCode = rpt.errorCode;
}

int CoredumpSessionService::GetClientFd(SessionId sessionId) const
{
    auto session = sessionManager_.GetSession(sessionId);
    if (!session) {
        return -1;
    }
    return session->clientFd;
}

bool CoredumpSessionService::WriteTimeoutAndClose(SessionId sessionId)
{
    CoreDumpResult coredumpRequst;
    coredumpRequst.retCode = -1;
    coredumpRequst.fileName[0] = '\0';
    return WriteResult(sessionId, coredumpRequst);
}

bool CoredumpSessionService::WriteResultAndClose(SessionId sessionId)
{
    DFXLOGI("%{public}s sessionId %{public}d ", __func__, sessionId);

    auto session = sessionManager_.GetSession(sessionId);
    if  (!session) {
        return false;
    }

    CoreDumpResult coredumpResult;
    if (strncpy_s(coredumpResult.fileName, sizeof(coredumpResult.fileName),
        session->filePath.c_str(), session->filePath.size()) != 0) {
        DFXLOGE("%{public}s :: strncpy failed.", __func__);
        return false;
    }
    coredumpResult.retCode = session->errorCode;

    return WriteResult(sessionId, coredumpResult);
}

bool CoredumpSessionService::WriteResult(SessionId sessionId, const CoreDumpResult& coredumpResult)
{
    auto session = sessionManager_.GetSession(sessionId);
    if  (!session) {
        return false;
    }

    int32_t savedConnectionFd = session->clientFd;
    SendMsgToSocket(savedConnectionFd, &coredumpResult, sizeof(coredumpResult));
    sessionManager_.RemoveSession(sessionId);
    return true;
}
}
}
