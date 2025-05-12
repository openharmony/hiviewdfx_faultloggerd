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

#include "dfx_dump_catcher_slow_policy.h"

#include <vector>
#include <unistd.h>

#include "dfx_log.h"
#include "dfx_util.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxDumpCatcherSlow"
#endif

namespace OHOS {
namespace HiviewDFX {

static const uint64_t DUMP_CATCH_SLOW_PERIOD_MS = 5000; // 5000 : 5s
static const int DUMP_CATCH_SLOW_MAX_RECORD_NUM = 50;

DfxDumpCatcherSlowPolicy& DfxDumpCatcherSlowPolicy::GetInstance()
{
    static DfxDumpCatcherSlowPolicy instance;
    return instance;
}

bool DfxDumpCatcherSlowPolicy::IsDumpCatcherInSlowPeriod(pid_t pid)
{
    std::unique_lock<std::mutex> lck(dumpSlowRecordMtx_);
    auto it = dumpSlowRecordMap_.find(pid);
    if (it == dumpSlowRecordMap_.end()) {
        return false;
    }

    uint64_t now = GetAbsTimeMilliSeconds();
    if (now - it->second > DUMP_CATCH_SLOW_PERIOD_MS) {
        dumpSlowRecordMap_.erase(pid);
        return false;
    }
    return true;
}

void DfxDumpCatcherSlowPolicy::DumpCatcherSlowRecordAgingClean(uint64_t time)
{
    for (auto it = dumpSlowRecordMap_.begin(); it != dumpSlowRecordMap_.end();) {
        if (time - it->second > DUMP_CATCH_SLOW_PERIOD_MS) {
            it = dumpSlowRecordMap_.erase(it);
        } else {
            ++it;
        }
    }

    if (dumpSlowRecordMap_.size() >= DUMP_CATCH_SLOW_MAX_RECORD_NUM) {
        dumpSlowRecordMap_.clear();
    }
}

void DfxDumpCatcherSlowPolicy::SetDumpCatcherSlowStat(pid_t pid)
{
    std::unique_lock<std::mutex> lck(dumpSlowRecordMtx_);
    uint64_t now = GetAbsTimeMilliSeconds();
    DumpCatcherSlowRecordAgingClean(now);
    dumpSlowRecordMap_[pid] = now;
}

} // namespace HiviewDFX
} // namespace OHOS
