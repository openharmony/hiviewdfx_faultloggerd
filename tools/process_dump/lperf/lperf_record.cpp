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

#include "dfx_lperf.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxLperfRecord"

const char *const LPERF_UNIQUE_TABLE = "lperf_unique_table";
constexpr uint32_t UNIQUE_STABLE_SIZE = 1024 * 1024;
}

int LperfRecord::StartProcessSampling(int pid, const std::vector<int>& tids, int freq, int duration)
{
    CHECK_TRUE_AND_RET(!CheckOutOfRange<int>(tids.size(), MIN_SAMPLE_TIDS, MAX_SAMPLE_TIDS), -1,
                       "invalid tids size: %d", tids.size());
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

    int ret = OnSubCommand();
    lperfEvents_.Clear();
    return ret;
}

int LperfRecord::CollectSampleStack(std::string& datas)
{
    if (stackPrinter_ == nullptr) {
        return -1;
    }
    auto frames = stackPrinter_->GetThreadSampledFrames();
    if (frames.empty()) {
        return -1;
    }
    std::ostringstream oss;
    StackPrinter::SerializeSampledFrameMap(frames, oss);
    datas = oss.str();
    return datas.empty() ? -1 : 0;
}

void LperfRecord::FinishProcessSampling()
{
    stackPrinter_ = nullptr;
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
    const uint64_t maxNr = 51;
    CHECK_TRUE_AND_RET(record.data_.nr > 0 && record.data_.nr < maxNr,
                       NO_RETVAL, "Symbolic invalid Record, nr: %llu", record.data_.nr);
    std::vector<uintptr_t> pcs;
    for (uint64_t i = 0; i < record.data_.nr; i++) {
        if (record.data_.ips[i] != PERF_CONTEXT_USER) {
            pcs.emplace_back(static_cast<uintptr_t>(record.data_.ips[i]));
        }
    }

    if (stackPrinter_ == nullptr) {
        stackPrinter_ = std::make_unique<StackPrinter>();
        stackPrinter_->InitUniqueTable(static_cast<pid_t>(record.data_.pid), UNIQUE_STABLE_SIZE, LPERF_UNIQUE_TABLE);
    }
    stackPrinter_->PutPcsInTable(pcs, static_cast<int>(record.data_.tid), record.data_.time);
}
} // namespace HiviewDFX
} // namespace OHOS