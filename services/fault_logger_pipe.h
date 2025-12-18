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
#include "smart_fd.h"

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerPipe {
public:
    FaultLoggerPipe();
    ~FaultLoggerPipe() = default;
    FaultLoggerPipe(const FaultLoggerPipe&) = delete;
    FaultLoggerPipe& operator=(const FaultLoggerPipe&) = delete;
    FaultLoggerPipe(FaultLoggerPipe&& rhs) noexcept = default;
    FaultLoggerPipe& operator=(FaultLoggerPipe&& rhs) noexcept = default;
    int GetReadFd() const;
    int GetWriteFd();
private:
    bool write_{false};
    SmartFd readFd_;
    SmartFd writeFd_;
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

class LitePerfPipePair {
public:
    int32_t GetPipeFd(PipeFdUsage usage, FaultLoggerPipeType pipeType);
    LitePerfPipePair(int32_t uid, uint64_t timeOutTime);
    LitePerfPipePair(LitePerfPipePair&&) noexcept = default;
    LitePerfPipePair& operator=(LitePerfPipePair&&) noexcept = default;

    static LitePerfPipePair& CreatePipePair(int uid, uint64_t timeOutTime);
    static bool CheckDumpRecord(int uid);
    static bool CheckDumpMax();
    static LitePerfPipePair* GetPipePair(int uid);
    static void DelPipePair(int uid);
    static void ClearTimeOutPairs();
private:
    LitePerfPipePair(const LitePerfPipePair&) = delete;
    LitePerfPipePair& operator=(const LitePerfPipePair&) = delete;
    int32_t uid_;
    uint64_t timeOutTime_;
    FaultLoggerPipe faultLoggerPipeBuf_;
    FaultLoggerPipe faultLoggerPipeRes_;
    static std::list<LitePerfPipePair> pipes_;
};

class LimitedPipePair {
public:
    int32_t GetPipeFd(FaultLoggerPipeType pipeType);
    explicit LimitedPipePair(int32_t pid);
    LimitedPipePair(LimitedPipePair&&) noexcept = default;
    LimitedPipePair& operator=(LimitedPipePair&&) noexcept = default;

    static LimitedPipePair& CreatePipePair(int pid);
    static bool CheckDumpRecord(int pid);
    static LimitedPipePair* GetPipePair(int pid);
    static void DelPipePair(int pid);
    static size_t GetPipeSize() { return pipes_.size(); }
private:
    LimitedPipePair(const LimitedPipePair&) = delete;
    LimitedPipePair& operator=(const LimitedPipePair&) = delete;
    int32_t pid_;
    FaultLoggerPipe faultLoggerPipeBuf_;
    static std::list<LimitedPipePair> pipes_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULT_LOGGER_PIPE_H
