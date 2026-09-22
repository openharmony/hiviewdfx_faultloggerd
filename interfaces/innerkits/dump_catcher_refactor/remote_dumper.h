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

#ifndef REMOTE_DUMPER_H
#define REMOTE_DUMPER_H

#include <cstdint>
#include <string>

#include "dump_catcher_shared_state.h"
#include "pipe_poller.h"
#include "result_handler.h"
#include "stats_reporter.h"
#include "timeout_analyzer.h"

namespace OHOS {
namespace HiviewDFX {

class RemoteDumper {
public:
    explicit RemoteDumper(DumpCatcherSharedState& state)
        : state_(state), poller_(state), resultHandler_(state), statsReporter_(state) {}

    int32_t DoDumpRemoteLocked(int pid, int tid, std::string& msg, bool isJson, int timeout);
    int32_t DoDumpCatchRemote(int pid, int tid, std::string& msg, bool isJson, int timeout);
    int DoDumpRemotePid(int pid, std::string& msg, DumpCatcherPipeData& pipeData, bool isJson, int32_t timeout);
    void ReportStats(int32_t pid, uint64_t requestTime, int32_t ret, void* retAddr);

private:
    DumpCatcherSharedState& state_;
    PipePoller poller_;
    ResultHandler resultHandler_;
    StatsReporter statsReporter_;
    TimeoutAnalyzer timeoutAnalyzer_;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // REMOTE_DUMPER_H
