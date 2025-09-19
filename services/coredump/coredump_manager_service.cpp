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
#include "coredump_manager_service.h"

#include <cJSON.h>
#include <fstream>

#include "coredump_model.h"
#include "fault_common_util.h"
#include "faultloggerd_socket.h"
#include "file_util.h"

namespace OHOS {
namespace HiviewDFX {
int32_t CoredumpManagerService::OnRequest(const std::string& socketName, int32_t connectionFd,
    const CoreDumpRequestData& requestData)
{
    DFXLOGI("Receive coredump request for pid:%{public}d, action:%{public}s.", requestData.pid,
        requestData.coredumpAction == 0 ? "save":"cancel");

    int res = CoredumpRequestValidator::ValidateRequest(socketName, connectionFd, requestData);
    if (res != ResponseCode::REQUEST_SUCCESS) {
        SendMsgToSocket(connectionFd, &res, sizeof(res));
        return res;
    }

    if (requestData.coredumpAction == CoreDumpAction::DO_CORE_DUMP) {
        res = HandleCreateEvent(connectionFd, requestData);
    } else if (requestData.coredumpAction == CoreDumpAction::CANCEL_CORE_DUMP) {
        res = HandleCancelEvent(requestData);
    } else {
        res = ResponseCode::REQUEST_REJECT;
    }
    SendMsgToSocket(connectionFd, &res, sizeof(res));
    return res;
}

int32_t CoredumpManagerService::HandleCreateEvent(int32_t connectionFd, const CoreDumpRequestData& requestData)
{
    auto session = sessionManager_.GetSession(requestData.pid);
    if (session) {
        DFXLOGI("coredump pid %{public}d is dumping", requestData.pid);
        return ResponseCode::CORE_DUMP_REPEAT;
    }

    CreateCoredumpRequest request;
    request.endTime = requestData.endTime;
    request.clientFd = dup(connectionFd);
    if (request.clientFd == -1) {
        DFXLOGE("dup connection socket fd error %{public}d", errno);
        return ResponseCode::DEFAULT_ERROR_CODE;
    }
    request.targetPid = requestData.pid;
    return cf_.CreateCoredump(request) ? ResponseCode::REQUEST_SUCCESS : ResponseCode::DEFAULT_ERROR_CODE;
}

int32_t CoredumpManagerService::HandleCancelEvent(const CoreDumpRequestData& requestData)
{
    auto session = sessionManager_.GetSession(requestData.pid);
    if (!session) {
        DFXLOGI("pid %{public}d is not dumping, just return!", requestData.pid);
        return ResponseCode::DEFAULT_ERROR_CODE;
    }

    cf_.CancelCoredump(requestData.pid);
    return ResponseCode::REQUEST_SUCCESS;
}

bool CoredumpRequestValidator::CheckCoredumpUID(uint32_t callerUid)
{
    constexpr const char* const configPath = "/system/etc/fault_coredump.json";
    auto config = FaultCoredumpConfig::Create(configPath);
    if (!config) {
        return false;
    }

    if (config->Contains(callerUid)) {
        DFXLOGI("UID %{public}d is whitelisted.", callerUid);
        return true;
    } else {
        DFXLOGI("UID %{public}d is NOT whitelisted.", callerUid);
        return false;
    }
}

int32_t CoredumpRequestValidator::ValidateRequest(const std::string& socketName, int32_t connectionFd,
    const CoreDumpRequestData& requestData)
{
    if (!IsValidPid(requestData.pid)) {
        DFXLOGE("Invalid PID: %{public}d", requestData.pid);
        return ResponseCode::REQUEST_REJECT;
    }

    if (!IsAuthorizedSocket(socketName)) {
        DFXLOGE("Unauthorized socket: %{public}s", socketName.c_str());
        return ResponseCode::REQUEST_REJECT;
    }

    if (!IsAuthorizedUid(connectionFd)) {
        DFXLOGE("Unauthorized UID:");
        return ResponseCode::REQUEST_REJECT;
    }

    if (TempFileManager::CheckCrashFileRecord(requestData.pid)) {
        DFXLOGW(" pid(%{public}d) has been crashed, break.",
                requestData.pid);
        return ResponseCode::CORE_PROCESS_CRASHED;
    }
    return ResponseCode::REQUEST_SUCCESS;
}

bool CoredumpRequestValidator::IsValidPid(pid_t pid)
{
    return pid > 0;
}

bool CoredumpRequestValidator::IsAuthorizedSocket(const std::string& socketName)
{
    return socketName == SERVER_SOCKET_NAME;
}

bool CoredumpRequestValidator::IsAuthorizedUid(int32_t connectionFd)
{
    struct ucred creds;
    if (!FaultCommonUtil::GetUcredByPeerCred(creds, connectionFd)) {
        return false;
    }
    return CheckCoredumpUID(creds.uid);
}

std::unique_ptr<FaultCoredumpConfig> FaultCoredumpConfig::Create(const std::string& jsonFilePath)
{
    if (jsonFilePath.empty()) {
        return nullptr;
    }

    std::string jsonText;
    if (!LoadStringFromFile(jsonFilePath, jsonText)) {
        DFXLOGE("Failed to read JSON file.");
        return nullptr;
    }

    auto config = std::make_unique<FaultCoredumpConfig>();
    if (!config->Parse(jsonText)) {
        DFXLOGE("Failed to parse whitelist.");
        return nullptr;
    }

    return config;
}

bool FaultCoredumpConfig::Contains(uint32_t uid) const
{
    return std::find(uids.begin(), uids.end(), uid) != uids.end();
}

bool FaultCoredumpConfig::Parse(const std::string& jsonText)
{
    cJSON* root = cJSON_Parse(jsonText.c_str());
    if (!root) {
        DFXLOGE("Failed to parse JSON: %{public}s", cJSON_GetErrorPtr());
        return false;
    }

    const cJSON* whitelistArray = cJSON_GetObjectItem(root, "whitelist");
    if (!cJSON_IsArray(whitelistArray)) {
        DFXLOGE("whitelist is missing or not an array");
        cJSON_Delete(root);
        return false;
    }
    uids.clear();

    cJSON* uid = nullptr;
    cJSON_ArrayForEach(uid, whitelistArray) {
        if (!cJSON_IsNumber(uid) || uid->valueint < 0) {
            char *printed = cJSON_Print(uid);
            DFXLOGE("Invalid UID in whitelist: %{public}s", printed);
            cJSON_free(printed);
            continue;
        }
        uids.push_back(static_cast<uint32_t>(uid->valueint));
    }

    cJSON_Delete(root);
    return true;
}
}
}
