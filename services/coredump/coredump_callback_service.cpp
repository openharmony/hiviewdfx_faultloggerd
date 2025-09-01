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
#include "coredump_callback_service.h"
#include "faultloggerd_socket.h"

namespace OHOS {
namespace HiviewDFX {
int32_t CoredumpCallbackService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const CoreDumpStatusData& requestData)
{
    DFXLOGI("Receive coredump status request for pid:%{public}d, status:%{public}d", requestData.pid,
        requestData.coredumpStatus);

    int res = CoredumpCallbackValidator::ValidateRequest(socketName, connectionFd, requestData);
    if (res != 0) {
        SendMsgToSocket(connectionFd, &res, sizeof(res));
        return res;
    }

    if (requestData.coredumpStatus == CoreDumpStatus::CORE_DUMP_START) {
        res = HandleUpdateWorkerPid(requestData);
    } else {
        res = HandleReport(requestData);
    }
    SendMsgToSocket(connectionFd, &res, sizeof(res));
    return res;
}

int32_t CoredumpCallbackService::HandleUpdateWorkerPid(const CoreDumpStatusData& requestData)
{
    if (requestData.processDumpPid <= 0) {
        DFXLOGE("workpid is invalid, targetpid %{public}d errno%{public}d", requestData.pid, requestData.retCode);
        return ResponseCode::ABNORMAL_SERVICE;
    }

    CoredumpCallbackReport req;
    req.workerPid = requestData.processDumpPid;
    bool ret = cf_.UpdateWorkerPid(requestData.pid, req.workerPid);
    return ret ? ResponseCode::REQUEST_SUCCESS : ResponseCode::ABNORMAL_SERVICE;
}

int32_t CoredumpCallbackService::HandleReport(const CoreDumpStatusData& requestData)
{
    CoredumpCallbackReport req;
    req.status = requestData.retCode == 0 ? CoredumpStatus::SUCCESS : CoredumpStatus::FAILED;
    req.filePath = requestData.fileName;
    cf_.ReportResult(requestData.pid, req);

    return ResponseCode::REQUEST_SUCCESS;
}

int32_t CoredumpCallbackValidator::ValidateRequest(const std::string& socketName, int32_t connectionFd,
    const CoreDumpStatusData& requestData)
{
    if (!IsValidPid(requestData.pid)) {
        DFXLOGE("Invalid PID: %{public}d", requestData.pid);
        return ResponseCode::REQUEST_REJECT;
    }

    if (!IsAuthorizedSocket(socketName)) {
        DFXLOGE("Unauthorized socket: %{public}s", socketName.c_str());
        return ResponseCode::REQUEST_REJECT;
    }
    return 0;
}

bool CoredumpCallbackValidator::IsValidPid(pid_t pid)
{
    return pid > 0;
}

bool CoredumpCallbackValidator::IsAuthorizedSocket(const std::string& socketName)
{
    return socketName == SERVER_SOCKET_NAME;
}
}
}
