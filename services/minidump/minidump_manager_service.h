/*
* Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef MINIDUMP_MANAGER_SERVICE_H
#define MINIDUMP_MANAGER_SERVICE_H

#include <unistd.h>
#include <vector>
#include <string>
#include <unordered_set>
#include "pdump.h"

#include "epoll_manager.h"

namespace OHOS {
namespace HiviewDFX {

class PDumpListener : public EpollListener {
public:
    explicit PDumpListener(SmartFd fd, bool persist) : EpollListener(std::move(fd), persist) {}
    void OnEventPoll() override;
};

class MinidumpManagerService {
public:
    static MinidumpManagerService& GetInstance();
    bool Init();
    bool ParsePDumpData(const struct __pdump_data_s& data);
    int SetMiniDump(pid_t pid, int8_t enableMinidump, int8_t enableMinidumpToCrashLog);

private:
    MinidumpManagerService() = default;
    ~MinidumpManagerService() = default;
    MinidumpManagerService(const MinidumpManagerService&) = delete;
    MinidumpManagerService& operator=(const MinidumpManagerService&) = delete;
    void ProcessWorkStart(const struct __pdump_data_s& data);
    void ProcessWorkEnd(const struct __pdump_data_s& data);
    int pFd_ {-1};
    std::mutex configsMutex_;
    std::unordered_set<pid_t> enableMinidumpConfigs;
    std::unordered_set<pid_t> disableMinidumpToCrashLogConfigs;
};
}
}

#endif
