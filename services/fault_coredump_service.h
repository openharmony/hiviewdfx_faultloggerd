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

#ifndef FAULT_COREDUMP_SERVICE_H
#define FAULT_COREDUMP_SERVICE_H

#include "csignal"
#include "cstdint"
#include "fault_logger_service.h"

namespace OHOS {
namespace HiviewDFX {
struct CoredumpProcessInfo {
    int32_t processDumpPid;
    int32_t coredumpSocketId;
    int endTime;
    bool cancelFlag;

    CoredumpProcessInfo(int32_t processDump, int32_t connectionFd, int time, bool flag)
        : processDumpPid(processDump), coredumpSocketId(connectionFd), endTime(time), cancelFlag(flag) {}
};

class RecorderProcessMap {
public:
    RecorderProcessMap(const RecorderProcessMap&) = delete;
    RecorderProcessMap& operator=(const RecorderProcessMap&) = delete;

    bool IsEmpty();
    bool HasTargetPid(int32_t targetPid);
    void AddProcessMap(int32_t targetPid, const CoredumpProcessInfo& processInfo);
    bool ClearTargetPid(int32_t targetPid);
    bool GetCoredumpSocketId(int32_t targetPid, int32_t& coredumpSocketId);
    bool GetCancelFlag(int32_t targetPid, bool& flag);
    bool GetProcessDumpPid(int32_t targetPid, int32_t& processDumpPid);
    bool SetCancelFlag(int32_t targetPid, bool flag);
    bool SetProcessDumpPid(int32_t targetPid, int32_t processDumpPid);
    static RecorderProcessMap& GetInstance();
    
private:
    RecorderProcessMap() = default;
    std::unordered_map<int32_t, CoredumpProcessInfo> coredumpProcessMap;
};

#ifndef is_ohos_lite
class CoredumpService : public FaultLoggerService<CoreDumpRequestData> {
public:
    CoredumpService() : coredumpRecorder(RecorderProcessMap::GetInstance()) {}
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const CoreDumpRequestData& requestData) override;

private:
    static int32_t Filter(const std::string& socketName, const CoreDumpRequestData& requestData, uint32_t uid);
    void StartDelayTask(std::function<void()> workFunc, int32_t delayTime);
    int32_t DoCoredumpRequest(const std::string& socketName, int32_t connectionFd,
                              const CoreDumpRequestData& requestData);
    int32_t CancelCoredumpRequest(int32_t connectionFd, const CoreDumpRequestData& requestData);
    RecorderProcessMap& coredumpRecorder;
};


class CoredumpStatusService : public FaultLoggerService<CoreDumpStatusData> {
public:
    CoredumpStatusService() : coredumpRecorder(RecorderProcessMap::GetInstance()) {}
    int32_t OnRequest(const std::string& socketName, int32_t connectionFd,
                      const CoreDumpStatusData& requestData) override;

private:
    bool HandleProcessDumpPid(int32_t targetPid, int32_t processDumpPid);
    RecorderProcessMap& coredumpRecorder;
};
#endif
}
}

#endif