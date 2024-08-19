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
#include <string>

#include "faultloggerd_client.h"
#include "fault_logger_pipe.h"

namespace OHOS {
namespace HiviewDFX {
class DumpStats {
public:
    int32_t pid = 0;
    uint64_t requestTime = 0;
    uint64_t signalTime = 0;
    uint64_t processdumpStartTime = 0;
    uint64_t processdumpFinishTime = 0;
    uint64_t dumpCatcherFinishTime = 0;
    int32_t result = 0;
    std::string summary;
    std::string callerProcessName;
    std::string callerElfName;
    std::string targetProcessName;
};
class FaultLoggerDaemon {
public:
    FaultLoggerDaemon();
    virtual ~FaultLoggerDaemon() {};
    int32_t StartServer();
    virtual bool InitEnvironment();
    void RecordFileCreation(int32_t type, int32_t pid);
    void ClearTimeOutRecords();
    bool IsCrashed(int32_t pid);

    int32_t CreateFileForRequest(int32_t type, int32_t pid, int32_t tid, uint64_t time, bool debugFlag) const;
#ifdef FAULTLOGGERD_FUZZER
    bool HandleStaticForFuzzer(int32_t type, uint32_t callerUid);
    void HandleRequestForFuzzer(int32_t epollFd, int32_t connectionFd,
                                const FaultLoggerdRequest *requestConst, FaultLoggerdRequest *request);
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
    void HandleReadBuf(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
        FaultLoggerPipe2* faultLoggerPipe);
    void HandleWriteBuf(int& fd, FaultLoggerPipe2* faultLoggerPipe);
    void HandleReadRes(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
        FaultLoggerPipe2* faultLoggerPipe);
    void HandleWriteRes(int& fd, FaultLoggerPipe2* faultLoggerPipe);
    void HandleJsonReadBuf(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
        FaultLoggerPipe2* faultLoggerPipe);
    void HandleJsonWriteBuf(int& fd, FaultLoggerPipe2* faultLoggerPipe);
    void HandleJsonReadRes(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
        FaultLoggerPipe2* faultLoggerPipe);
    void HandleJsonWriteRes(int& fd, FaultLoggerPipe2* faultLoggerPipe);
    void HandleDelete(FaultLoggerdRequest* request);
    void HandleRequestByPipeType(int& fd, int32_t connectionFd, FaultLoggerdRequest* request,
        FaultLoggerPipe2* faultLoggerPipe);
    void HandleExceptionRequest(int32_t connectionFd, FaultLoggerdRequest* request);
    void HandleRequestByClientType(int32_t connectionFd, FaultLoggerdRequest* request);
    bool CheckRequestCredential(int32_t connectionFd, FaultLoggerdRequest* request);
    virtual bool CreateSockets();
    virtual bool CreateEventFd();
    void CleanupSockets();
    virtual void WaitForRequest();
    void CleanupEventFd();
    void HandleDumpStats(int32_t connectionFd, FaultLoggerdStatsRequest* request);
    void RemoveTimeoutDumpStats();
    void ReportDumpStats(const DumpStats& stat);

private:
    int32_t defaultSocketFd_ = -1;
    int32_t crashSocketFd_ = -1;
    int32_t sdkdumpSocketFd_ = -1;
    int32_t eventFd_ = -1;
    std::map<int32_t, int32_t> connectionMap_;
    std::map<int32_t, int64_t> crashTimeMap_;
    std::vector<DumpStats> stats_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
