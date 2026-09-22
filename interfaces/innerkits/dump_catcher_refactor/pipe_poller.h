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

#ifndef PIPE_POLLER_H
#define PIPE_POLLER_H

#include <cstdint>
#include <poll.h>
#include <string>

#include "dump_catcher_pipe_data.h"
#include "dump_catcher_shared_state.h"

namespace OHOS {
namespace HiviewDFX {

constexpr int POLL_FD_NUM = 2;

class PipePoller {
public:
    explicit PipePoller(DumpCatcherSharedState& state) : state_(state) {}

    int Poll(int pid, int timeout, DumpCatcherPipeData& pipeData);
    int HandleReadPhase(int pid, int timeout, DumpCatcherPipeData& pipeData, bool isJson, std::string& msg);

private:
    bool HandlePollError(uint64_t endTime, int& remainTime, int& pollRet, std::string& resMsg);
    bool HandlePollTimeout(uint64_t endTime, int& remainTime, int& pollRet, std::string& resMsg);
    bool HandlePollEvents(int pid, const struct pollfd (&readFds)[POLL_FD_NUM],
        bool& bPipeConnect, int& pollRet, DumpCatcherPipeData& pipeData);
    int DoPollLoop(int pid, int timeout, DumpCatcherPipeData& pipeData);

    DumpCatcherSharedState& state_;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // PIPE_POLLER_H
