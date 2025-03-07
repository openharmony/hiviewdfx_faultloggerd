/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef FAULT_LOGGER_SERVICE_H_
#define FAULT_LOGGER_SERVICE_H_

#include "string"
#include "vector"
#include "cstdint"
#include "dfx_socket_request.h"
#include "temp_file_manager.h"
#include "dfx_exception.h"

namespace OHOS {
namespace HiviewDFX {

class IFaultLoggerService {
public:
    virtual int32_t OnReceiveMsg(const std::string& socketName, int32_t connectionFd,
                              ssize_t nRead, const std::vector<uint8_t>& buf) = 0;
    virtual ~IFaultLoggerService() = default;
};

template<typename T>
class FaultLoggerService : public IFaultLoggerService {
public:
    FaultLoggerService() = default;
    int32_t OnReceiveMsg(const std::string& socketName, int32_t connectionFd,
                      const ssize_t nRead, const std::vector<uint8_t>& buf) final
    {
        if (nRead != sizeof(T)) {
            return ResponseCode::INVALID_REQUEST_DATA;
        }
        return OnRequest(socketName, connectionFd, *(reinterpret_cast<const T*>(buf.data())));
    };
protected:
    virtual int32_t OnRequest(const std::string& socketName, int32_t connectionFd, const T& requestData) = 0;
};

class FileDesService : public FaultLoggerService<FaultLoggerdRequest> {
public:
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const FaultLoggerdRequest& requestData) override;
private:
    static bool Filter(const std::string& socketName, int32_t connectionFd, const FaultLoggerdRequest& requestData);
};

#ifndef HISYSEVENT_DISABLE
class ExceptionReportService : public FaultLoggerService<CrashDumpException> {
public:
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const CrashDumpException& requestData) override;
private:
    static bool Filter(int32_t connectionFd, const CrashDumpException& requestData);
};

struct DumpStats {
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

class StatsService : public FaultLoggerService<FaultLoggerdStatsRequest> {
public:
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const FaultLoggerdStatsRequest& requestData) override;
private:
    void RemoveTimeoutDumpStats();
    void ReportDumpStats(const DumpStats& stat);
    static std::string GetElfName(const FaultLoggerdStatsRequest& request);
    void StartDelayTask(std::function<void()> workFunc, int32_t delayTime);
    std::list<DumpStats> stats_{};
};
#endif

#ifndef is_ohos_lite
class SdkDumpService : public FaultLoggerService<SdkDumpRequestData> {
public:

    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const SdkDumpRequestData& requestData) override;
private:
    static int32_t Filter(const std::string& socketName, const SdkDumpRequestData& requestData, uint32_t uid);
};

class PipeService : public FaultLoggerService<PipFdRequestData> {
public:
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const PipFdRequestData& requestData) override;
private:
    static bool Filter(const std::string& socketName, int32_t connectionFd, const PipFdRequestData& requestData);
};
#endif
}
}
#endif // FAULT_LOGGER_SERVICE_H_
