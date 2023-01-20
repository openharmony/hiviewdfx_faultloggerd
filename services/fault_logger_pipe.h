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

/* This files contains header of secure module. */

#ifndef _FAULT_LOGGER_PIPE_H
#define _FAULT_LOGGER_PIPE_H

#include <map>
#include <memory>
#include <string>

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerPipe {
public:
    FaultLoggerPipe();
    ~FaultLoggerPipe();

    int GetReadFd();
    int GetWriteFd();

    void Close(int fd) const;
private:
    bool Init();
    void Destroy();

    int pfds_[2] = {-1, -1};
    bool bInit_;
};

class FaultLoggerPipe2 {
public:
    FaultLoggerPipe2(std::unique_ptr<FaultLoggerPipe> pipeBuf, std::unique_ptr<FaultLoggerPipe> pipeRes);
    FaultLoggerPipe2();
    ~FaultLoggerPipe2();

    std::unique_ptr<FaultLoggerPipe> faultLoggerPipeBuf_;
    std::unique_ptr<FaultLoggerPipe> faultLoggerPipeRes_;
};

class FaultLoggerPipeMap {
public:
    FaultLoggerPipeMap();
    ~FaultLoggerPipeMap();

    void Set(int pid);
    FaultLoggerPipe2* Get(int pid);
    void Del(int pid);

private:
    bool Find(int pid) const;

private:
    std::map<int, std::unique_ptr<FaultLoggerPipe2> > faultLoggerPipes_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
