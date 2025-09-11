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

#include "lite_perf_dumper.h"

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <securec.h>
#include <string>

#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_dump_res.h"
#include "dfx_trace.h"
#include "faultloggerd_client.h"
#include "lperf_event_record.h"
#include "lperf_events.h"
#include "lperf_record.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxLitePerfDump"
}

LitePerfDumper &LitePerfDumper::GetInstance()
{
    static LitePerfDumper ins;
    return ins;
}

void LitePerfDumper::Perf()
{
    LitePerfParam lperf;
    PerfProcess(lperf);
}

int LitePerfDumper::PerfProcess(LitePerfParam& lperf)
{
    DFX_TRACE_SCOPED("PerfProcess");
    int ret = ReadLperfAndCheck(lperf);
    if (ret < 0) {
        return ret;
    }

    int pipeWriteFd[PIPE_NUM_SZ] = { -1, -1};
    if (RequestLitePerfPipeFd(FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd, 0, false) == -1) {
        DFXLOGE("%{public}s request pipe failed, err:%{public}d", __func__, errno);
        return -1;
    }
    
    return PerfRecord(pipeWriteFd, lperf);
}

int LitePerfDumper::PerfRecord(int (&pipeWriteFd)[2], LitePerfParam& lperf)
{
    SmartFd bufFd(pipeWriteFd[PIPE_BUF_INDEX]);
    SmartFd resFd(pipeWriteFd[PIPE_RES_INDEX]);

    std::vector<int> lperfTids;
    for (auto i = 0; i < MAX_SAMPLE_TIDS; ++i) {
        if (lperf.tids[i] <= 0) {
            continue;
        }
        DFXLOGI("perf tid(%{public}d).", lperf.tids[i]);
        lperfTids.emplace_back(lperf.tids[i]);
    }

    LperfRecord record;
    int res = record.StartProcessSampling(lperf.pid, lperfTids, lperf.freq, lperf.durationMs);
    if (res == 0) {
        std::string data;
        if (record.CollectSampleStack(data) == 0) {
            WriteSampleData(bufFd.GetFd(), data);
        }
    }
    ssize_t nresWrite = OHOS_TEMP_FAILURE_RETRY(write(resFd.GetFd(), &res, sizeof(res)));
    if (nresWrite != static_cast<ssize_t>(sizeof(res))) {
        DFXLOGE("%{public}s write res fail, err:%{public}d", __func__, errno);
        return -1;
    }
    record.FinishProcessSampling();
    return 0;
}

void LitePerfDumper::WriteSampleData(int bufFd, const std::string& data)
{
    constexpr size_t step = MAX_PIPE_SIZE;
    for (size_t i = 0; i < data.size(); i += step) {
        size_t length = (i + step) < data.size() ? step : data.size() - i;
        DFXLOGI("%{public}s write length: %{public}zu", __func__, length);
        ssize_t nwrite = OHOS_TEMP_FAILURE_RETRY(write(bufFd, data.substr(i, length).c_str(), length));
        if (nwrite != static_cast<ssize_t>(length)) {
            DFXLOGE("%{public}s write fail, err:%{public}d", __func__, errno);
            return;
        }
    }
}

int32_t LitePerfDumper::ReadLperfAndCheck(LitePerfParam& lperf)
{
    DFX_TRACE_SCOPED("ReadRequestAndCheck");
    ssize_t readCount = OHOS_TEMP_FAILURE_RETRY(read(STDOUT_FILENO, &lperf, sizeof(LitePerfParam)));
    if (readCount != static_cast<long>(sizeof(LitePerfParam))) {
        DFXLOGE("Failed to read LitePerfParam(%{public}d), readCount(%{public}zd).", errno, readCount);
        return -1;
    }
    return 0;
}

} // namespace HiviewDFX
} // namespace OHOS
