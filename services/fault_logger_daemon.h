/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#ifndef DFX_FAULTLOGGERD_H
#define DFX_FAULTLOGGERD_H

#include <cinttypes>
#include "faultloggerd_client.h"

int32_t StartServer(int argc, char *argv[]);

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerDaemon {
public:
    FaultLoggerDaemon() {};
    ~FaultLoggerDaemon() {};
    bool InitEnvironment();
    void LoopAcceptRequestAndFork(int socketFd);

private:
    int32_t CreateFileForRequest(int32_t type, int32_t pid, bool debugFlag) const;
    void RemoveTempFileIfNeed();
    void HandleRequest(int32_t connectionFd);
    void HandleDefaultClientReqeust(int32_t connectionFd, const FaultLoggerdRequest* request);
    void HandleLogFileDesClientReqeust(int32_t connectionFd, const FaultLoggerdRequest* request);
    void HandlePrintTHilogClientReqeust(int32_t const connectionFd, FaultLoggerdRequest* request);
    FaultLoggerCheckPermissionResp SecurityCheck(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandlePermissionReqeust(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandleSdkDumpReqeust(int32_t connectionFd, FaultLoggerdRequest* request);

private:
    int32_t currentLogCounts_ {0};
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
