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
#ifndef COREDUMP_MANAGER_SERVICE_H
#define COREDUMP_MANAGER_SERVICE_H

#include "coredump_facade.h"
#include "dfx_socket_request.h"
#include "fault_logger_service.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpManagerService : public FaultLoggerService<CoreDumpRequestData> {
public:
    CoredumpManagerService(): sessionManager_(CoredumpSessionManager::Instance()) {}
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const CoreDumpRequestData& requestData) override;
private:
    int32_t HandleCreate(int32_t connectionFd, const CoreDumpRequestData& requestData);
    int32_t HandleCancel(const CoreDumpRequestData& requestData);
    CoredumpFacade cf_;
    CoredumpSessionManager& sessionManager_;
};

class CoredumpRequestValidator {
public:
    static int32_t ValidateRequest(const std::string& socketName, int32_t connectionFd,
                                    const CoreDumpRequestData& requestData);
private:
    static bool IsValidPid(pid_t pid);
    static bool IsAuthorizedSocket(const std::string& socketName);
    static bool IsAuthorizedUid(int32_t connectionFd);
    static bool CheckCoredumpUID(uint32_t callerUid);
};

class FaultCoredumpConfig {
public:
    static FaultCoredumpConfig& GetInstance(const std::string& jsonFilePath = "");

    bool Contains(uint32_t uid) const;
private:
    FaultCoredumpConfig() = default;
    FaultCoredumpConfig(const FaultCoredumpConfig&) = delete;
    FaultCoredumpConfig& operator=(const FaultCoredumpConfig&) = delete;

    bool Parse(const std::string& jsonText);

    std::vector<uint32_t> uids;
};
}
}
#endif
