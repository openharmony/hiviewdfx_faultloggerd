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

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerDaemon {
public:
    FaultLoggerDaemon();
    ~FaultLoggerDaemon() {};
    int32_t StartServer();
    bool InitEnvironment();
    void LoopAcceptRequestAndFork(int socketFd);
    int32_t CreateFileForRequest(int32_t type, int32_t pid, uint64_t time, bool debugFlag) const;

private:
    static void HandleRequesting(int32_t connectionFd);
    void RemoveTempFileIfNeed();
    void HandleRequest(int32_t connectionFd);
    void HandleDefaultClientReqeust(int32_t connectionFd, const FaultLoggerdRequest* request);
    void HandleLogFileDesClientReqeust(int32_t connectionFd, const FaultLoggerdRequest* request);
    void HandlePrintTHilogClientReqeust(int32_t const connectionFd, FaultLoggerdRequest* request);
    FaultLoggerCheckPermissionResp SecurityCheck(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandlePermissionReqeust(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandleSdkDumpReqeust(int32_t connectionFd, FaultLoggerdRequest* request);

private:
    static FaultLoggerDaemon* faultLoggerDaemon_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
