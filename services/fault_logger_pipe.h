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
    bool SetSize(long sz);
    void Destroy(void);

    int fds_[2] = {-1, -1};
    bool init_;
    bool write_;
};

class FaultLoggerPipePair {
public:
    explicit FaultLoggerPipePair(uint64_t time, bool isJson = false);
    int GetPipFd(bool isWritePip, bool isResPip, bool isJson);
    bool isJson_;
    uint64_t time_;
    FaultLoggerPipe faultLoggerPipeBuf_;
    FaultLoggerPipe faultLoggerPipeRes_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif // FAULT_LOGGER_PIPE_H
