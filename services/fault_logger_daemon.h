/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#include <map>

#include "faultloggerd_client.h"

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerDaemon {
public:
    FaultLoggerDaemon();
    ~FaultLoggerDaemon() {};
    int32_t StartServer();
    bool InitEnvironment();
    int32_t CreateFileForRequest(int32_t type, int32_t pid, int32_t tid, uint64_t time, bool debugFlag) const;
#ifdef FAULTLOGGERD_FUZZER
    void HandleRequestForFuzzer(int32_t epollFd, int32_t connectionFd);
#endif

private:
    void AddEvent(int32_t epollFd, int32_t addFd, uint32_t event);
    void DelEvent(int32_t epollFd, int32_t delFd, uint32_t event);
    void HandleAccept(int32_t epollFd, int32_t socketFd);
    void HandleRequest(int32_t epollFd, int32_t connectionFd);

    void RemoveTempFileIfNeed();
    void HandleDefaultClientRequest(int32_t connectionFd, const FaultLoggerdRequest* request);
    void HandleLogFileDesClientRequest(int32_t connectionFd, const FaultLoggerdRequest* request);
    void HandlePrintTHilogClientRequest(int32_t const connectionFd, FaultLoggerdRequest* request);
    FaultLoggerCheckPermissionResp SecurityCheck(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandlePermissionRequest(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandleSdkDumpRequest(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandlePipeFdClientRequest(int32_t connectionFd, FaultLoggerdRequest* request);
    bool CheckRequestCredential(int32_t connectionFd, FaultLoggerdRequest* request);
    bool CreateSockets();
    bool CreateEventFd();
    void CleanupSockets();
    void WaitForRequest();
    void CleanupEventFd();

private:
    int32_t defaultSocketFd_ = -1;
    int32_t crashSocketFd_ = -1;
    int32_t eventFd_ = -1;
    std::map<int32_t, int32_t> connectionMap_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
