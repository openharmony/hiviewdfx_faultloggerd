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

#ifndef DFX_DUMP_CATCHER_SLOW_POLICY_H
#define DFX_DUMP_CATCHER_SLOW_POLICY_H

#include <cinttypes>
#include <map>
#include <mutex>

namespace OHOS {
namespace HiviewDFX {

class DfxDumpCatcherSlowPolicy {
public:
    static DfxDumpCatcherSlowPolicy& GetInstance();
    bool IsDumpCatcherInSlowPeriod(pid_t pid);
    void SetDumpCatcherSlowStat(pid_t pid);

private:
    DfxDumpCatcherSlowPolicy() = default;
    DfxDumpCatcherSlowPolicy(const DfxDumpCatcherSlowPolicy&) = delete;
    DfxDumpCatcherSlowPolicy& operator=(const DfxDumpCatcherSlowPolicy&) = delete;
    void DumpCatcherSlowRecordAgingClean(uint64_t time);

    std::map<int, uint64_t> dumpSlowRecordMap_;
    std::mutex dumpSlowRecordMtx_;
};

} // namespace HiviewDFX
} // namespace OHOS
#endif
