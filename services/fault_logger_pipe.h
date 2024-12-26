/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef FAULT_LOGGER_PIPE_H
#define FAULT_LOGGER_PIPE_H

#include <cstdint>
#include <list>

#include "dfx_socket_request.h"

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerPipe {
public:
    FaultLoggerPipe();
    ~FaultLoggerPipe();
    FaultLoggerPipe(const FaultLoggerPipe&) = delete;
    FaultLoggerPipe& operator=(const FaultLoggerPipe&) = delete;
    FaultLoggerPipe(FaultLoggerPipe&& rhs) noexcept;
    FaultLoggerPipe& operator=(FaultLoggerPipe&& rhs) noexcept;
    int GetReadFd() const;
    int GetWriteFd();
private:
    static void Close(int& fd);
    bool write_{false};
    int readFd_{-1};
    int writeFd_{-1};
};

enum class PipeFdUsage {
    BUFFER_FD = 0,
    RESULT_FD = 1,
};

class FaultLoggerPipePair {
public:
    int32_t GetPipeFd(PipeFdUsage usage, FaultLoggerPipeType pipeType);
    FaultLoggerPipePair(int32_t pid, uint64_t requestTime);
    FaultLoggerPipePair(FaultLoggerPipePair&&) noexcept = default;
    FaultLoggerPipePair& operator=(FaultLoggerPipePair&&) noexcept = default;
    static FaultLoggerPipePair& CreateSdkDumpPipePair(int pid, uint64_t requestTime);
    static bool CheckSdkDumpRecord(int pid, uint64_t checkTime);
    static FaultLoggerPipePair* GetSdkDumpPipePair(int pid);
    static void DelSdkDumpPipePair(int pid);
private:
    FaultLoggerPipePair(const FaultLoggerPipePair&) = delete;
    FaultLoggerPipePair& operator=(const FaultLoggerPipePair&) = delete;
    bool IsValid(uint64_t checkTime) const;
    int32_t pid_;
    uint64_t requestTime_;
    FaultLoggerPipe faultLoggerPipeBuf_;
    FaultLoggerPipe faultLoggerPipeRes_;
    static std::list<FaultLoggerPipePair> sdkDumpPipes_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULT_LOGGER_PIPE_H
