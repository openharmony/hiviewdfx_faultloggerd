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

#ifndef STATS_REPORTER_H
#define STATS_REPORTER_H

#include <cstdint>

#include "dump_catcher_shared_state.h"
#include "kernel_stack_async_collector.h"

namespace OHOS {
namespace HiviewDFX {

struct FaultLoggerdStatsRequest;

class StatsReporter {
public:
    explicit StatsReporter(DumpCatcherSharedState& state) : state_(state) {}
    void ReportDumpCatcherStats(int32_t pid, uint64_t requestTime, int32_t ret, void* retAddr);

private:
    int32_t ConvertDumpResultToDumpStats(int32_t dumpRes);
    bool CopyProcessName(int32_t pid, struct FaultLoggerdStatsRequest* stat);
    bool CopySummary(int32_t ret, struct FaultLoggerdStatsRequest* stat);
    bool CopyCallerElf(void* retAddr, struct FaultLoggerdStatsRequest* stat);
    bool CopyCallerCmdline(struct FaultLoggerdStatsRequest* stat);

    DumpCatcherSharedState& state_;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // STATS_REPORTER_H
