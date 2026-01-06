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

#include "dfx_log.h"
#include "dfx_util.h"
#include "faultloggerd_socket.h"
#include "hisysevent_c.h"
#include "securec.h"

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

bool CoredumpSessionService::WriteTimeout(SessionId sessionId)
{
    CoreDumpResult coredumpRequest;
    coredumpRequest.retCode = -1;
    coredumpRequest.fileName[0] = '\0';
    return WriteResult(sessionId, coredumpRequest);
}

bool CoredumpSessionService::WriteResult(SessionId sessionId)
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
    ReportCoredumpStatistics(sessionId);
    sessionManager_.RemoveSession(sessionId);
    return true;
}

bool CoredumpSessionService::ReportCoredumpStatistics(SessionId sessionId)
{
    const auto session = sessionManager_.GetSession(sessionId);
    if (!session) {
        return false;
    }
    char summary[] = "coredump event happen";
    HiSysEventParam params[] = {
        {.name = "TARGET_PROCESS_NAME", .t = HISYSEVENT_STRING,
            .v = {.s = const_cast<char*>(session->filePath.c_str())}, .arraySize = 0},
        {.name = "TARGET_PID", .t = HISYSEVENT_INT32, .v = { .i32 = session->sessionId}, .arraySize = 0},
        {.name = "WORKER_PID", .t = HISYSEVENT_INT32, .v = { .i32 = session->workerPid}, .arraySize = 0},
        {.name = "RESULT", .t = HISYSEVENT_INT32, .v = { .i32 = session->errorCode}, .arraySize = 0},
        {.name = "SUMMARY", .t = HISYSEVENT_STRING, .v = {.s = summary}, .arraySize = 0},
        {.name = "REQUEST_TIMESTAMP", .t = HISYSEVENT_UINT32,
            .v = {.ui32 = static_cast<uint32_t>(session->startTime)}, .arraySize = 0},
        {.name = "FINISH_TIMESTAMP", .t = HISYSEVENT_UINT32,
            .v = {.ui32 = static_cast<uint32_t>(GetTimeMilliSeconds())}, .arraySize = 0},
    };
    int ret = OH_HiSysEvent_Write("RELIABILITY", "COREDUMP_STATISTICS",
                                  HISYSEVENT_STATISTIC, params, sizeof(params) / sizeof(params[0]));
    DFXLOGI(" report coredump event %{public}d result %{public}d", sessionId, ret);
    return ret == 0;
}
}
}
