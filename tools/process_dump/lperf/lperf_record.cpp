/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "lperf_record.h"
#include "unwinder_config.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxLperfRecord"

constexpr int MIN_SAMPLE_COUNT = 1;
constexpr int MAX_SAMPLE_COUNT = 10;
constexpr int MIN_SAMPLE_FREQUENCY = 1;
constexpr int MAX_SAMPLE_FREQUENCY = 100;
constexpr int MIN_STOP_SECONDS = 1;
constexpr int MAX_STOP_SECONDS = 10000;
const char *const LPERF_UNIQUE_TABLE = "lperf_unique_table";
constexpr uint32_t UNIQUE_STABLE_SIZE = 1024 * 1024;
}

void LperfRecord::SetUnwindInfo(const std::shared_ptr<Unwinder>& unwinder, const std::shared_ptr<DfxMaps>& maps)
{
    unwinder_ = unwinder;
    maps_ = maps;
}

int LperfRecord::StartProcessSampling(int pid, const std::vector<int>& tids,
                                      int freq, int duration, bool parseMiniDebugInfo)
{
    CHECK_TRUE_AND_RET(!CheckOutOfRange<int>(tids.size(), MIN_SAMPLE_COUNT, MAX_SAMPLE_COUNT), -1,
                       "invalid tids count: %d", tids.size());
    CHECK_TRUE_AND_RET(!CheckOutOfRange<int>(freq, MIN_SAMPLE_FREQUENCY, MAX_SAMPLE_FREQUENCY), -1,
                       "invalid frequency value: %d", freq);
    CHECK_TRUE_AND_RET(!CheckOutOfRange<int>(duration, MIN_STOP_SECONDS, MAX_STOP_SECONDS), -1,
                       "invalid duration value: %d", duration);
    for (int tid : tids) {
        CHECK_TRUE_AND_RET(tid > 0, -1, "invalid tid: %d", tid);
    }

    pid_ = pid;
    tids_ = tids;
    frequency_ = static_cast<unsigned int>(freq);
    timeStopSec_ = static_cast<unsigned int>(duration);
    enableDebugInfoSymbolic_ = parseMiniDebugInfo;

    int ret = OnSubCommand();
    lperfEvents_.Clear();
    return ret;
}

int LperfRecord::CollectSampleStack(int tid, std::string& stack)
{
    CHECK_TRUE_AND_RET(tid > 0, -1, "invalid tid: %d", tid);
    unsigned int uintTid = static_cast<unsigned int>(tid);
    if (tidStackMaps_.find(uintTid) != tidStackMaps_.end()) {
        tidStackMaps_[uintTid]->SetUnwindInfo(unwinder_, maps_);
        stack = tidStackMaps_[uintTid]->GetTreeStack();
        if (stack.size() > 0) {
            return 0;
        }
    }
    return -1;
}

void LperfRecord::FinishProcessSampling()
{
    UnwinderConfig::SetEnableMiniDebugInfo(defaultEnableDebugInfo_);
    tidStackMaps_.clear();
}

int LperfRecord::OnSubCommand()
{
    PrepareLperfEvent();
    CHECK_TRUE_AND_RET(lperfEvents_.PrepareRecord() == 0, -1, "OnSubCommand prepareRecord failed");
    CHECK_TRUE_AND_RET(lperfEvents_.StartRecord() == 0, -1, "OnSubCommand startRecord failed");
    return 0;
}

void LperfRecord::PrepareLperfEvent()
{
    defaultEnableDebugInfo_ = UnwinderConfig::GetEnableMiniDebugInfo();
    UnwinderConfig::SetEnableMiniDebugInfo(enableDebugInfoSymbolic_);
    lperfEvents_.SetTid(tids_);
    lperfEvents_.SetTimeOut(timeStopSec_);
    lperfEvents_.SetSampleFrequency(frequency_);
    auto processRecord = [this](LperfRecordSample& record) -> void {
        this->SymbolicRecord(record);
    };
    lperfEvents_.SetRecordCallBack(processRecord);
}

void LperfRecord::SymbolicRecord(LperfRecordSample& record)
{
    CHECK_TRUE_AND_RET(record.data_.tid > 0, NO_RETVAL, "Symbolic invalid Record, tid: %d", record.data_.tid);
    unsigned int tid = static_cast<unsigned int>(record.data_.tid);
    if (tidStackMaps_.find(tid) == tidStackMaps_.end()) {
        tidStackMaps_.emplace(tid, std::make_unique<StackPrinter>());
        tidStackMaps_[tid]->InitUniqueTable(record.data_.pid, UNIQUE_STABLE_SIZE, LPERF_UNIQUE_TABLE);
    }
    std::vector<uintptr_t> pcs;
    for (unsigned int i = 0; i < record.data_.nr; i++) {
        if (record.data_.ips[i] != PERF_CONTEXT_USER) {
            pcs.emplace_back(static_cast<uintptr_t>(record.data_.ips[i]));
        }
    }
    tidStackMaps_[tid]->PutPcsInTable(pcs, record.data_.time);
}
} // namespace HiviewDFX
} // namespace OHOS