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
#ifndef COREDUMP_CALLBACK_SERVICE_H
#define COREDUMP_CALLBACK_SERVICE_H

#include "dfx_socket_request.h"
#include "fault_logger_service.h"
#include "coredump_facade.h"

namespace OHOS {
namespace HiviewDFX {
class CoredumpCallbackService : public FaultLoggerService<CoreDumpStatusData> {
public:
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const CoreDumpStatusData& requestData) override;
private:
    int32_t HandleUpdateWorkerPid(const CoreDumpStatusData& requestData);
    int32_t HandleReport(const CoreDumpStatusData& requestData);
    CoredumpFacade cf_;
};

class CoredumpCallbackValidator {
public:
    static int32_t ValidateRequest(const std::string& socketName, int32_t connectionFd,
                                    const CoreDumpStatusData& requestData);
private:
    static bool IsValidPid(pid_t pid);
    static bool IsAuthorizedSocket(const std::string& socketName);
};
}
}
#endif
