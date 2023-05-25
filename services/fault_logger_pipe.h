/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files contains header of pipe module. */

#ifndef FAULT_LOGGER_PIPE_H
#define FAULT_LOGGER_PIPE_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerPipe {
public:
    FaultLoggerPipe();
    ~FaultLoggerPipe();

    int GetReadFd(void);
    int GetWriteFd(void);

    void Close(int fd) const;
private:
    bool Init(void);
    void Destroy(void);

    int fds_[2] = {-1, -1};
    bool init_;
    bool write_;
};

class FaultLoggerPipe2 {
public:
    explicit FaultLoggerPipe2(uint64_t time);
    ~FaultLoggerPipe2();

    std::unique_ptr<FaultLoggerPipe> faultLoggerPipeBuf_;
    std::unique_ptr<FaultLoggerPipe> faultLoggerPipeRes_;
    uint64_t time_;
};

class FaultLoggerPipeMap {
public:
    FaultLoggerPipeMap();
    ~FaultLoggerPipeMap();

    bool Check(int pid, uint64_t time);
    void Set(int pid, uint64_t time);
    FaultLoggerPipe2* Get(int pid);
    void Del(int pid);

private:
    bool Find(int pid) const;

private:
    std::map<int, std::unique_ptr<FaultLoggerPipe2> > faultLoggerPipes_;
    std::mutex pipeMapsMutex_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
