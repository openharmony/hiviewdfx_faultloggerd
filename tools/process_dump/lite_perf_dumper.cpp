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
    if (RequestLitePerfPipeFd(FaultLoggerPipeType::PIPE_FD_WRITE, pipeWriteFd) == -1) {
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
    for (auto i = 0; i < MAX_PERF_TIDS_SIZE; ++i) {
        if (lperf.tids[i] <= 0 || !IsThreadInPid(lperf.pid, lperf.tids[i])) {
            DFXLOGW("tid(%{public}d) is not in curr pid(%{public}d).", lperf.tids[i], lperf.pid);
            continue;
        }
        lperfTids.emplace_back(lperf.tids[i]);
    }

    LperfRecord record;
    auto accessor = std::make_shared<OHOS::HiviewDFX::UnwindAccessors>();
    auto unwinder = std::make_shared<Unwinder>(accessor, false);
    auto maps = DfxMaps::Create(lperf.pid);
    record.SetUnwindInfo(unwinder, maps);
    int res = record.StartProcessSampling(lperf.pid, lperfTids, lperf.freq, lperf.durationMs, lperf.parseMiniDebugInfo);
    if (res == 0) {
        for (const auto& tid : lperfTids) {
            std::string stack;
            if (record.CollectSampleStack(tid, stack) == 0) {
                WriteSampleStackByTid(tid, bufFd.GetFd());
            } else {
                DFXLOGW("%{public}s CollectSampleStack tid(%{public}d) fail", __func__, tid);
                continue;
            }
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

void LitePerfDumper::WriteSampleStackByTid(int tid, int bufFd)
{
    std::string writeStr = LITE_PERF_SPLIT + std::to_string(tid) + "\n";
    constexpr size_t step = MAX_PIPE_SIZE;
    writeStr += stack;
    for (size_t i = 0; i < writeStr.size(); i += step) {
        size_t length = (i + step) < writeStr.size() ? step : writeStr.size() - i;
        OHOS_TEMP_FAILURE_RETRY(write(bufFd, writeStr.substr(i, length).c_str(), length));
    }
}

int32_t LitePerfDumper::ReadLperfAndCheck(LitePerfParam& lperf)
{
    DFX_TRACE_SCOPED("ReadRequestAndCheck");
    ElapsedTime counter("ReadRequestAndCheck", 20); // 20 : limit cost time 20 ms
    ssize_t readCount = OHOS_TEMP_FAILURE_RETRY(read(STDOUT_FILENO, &lperf, sizeof(LitePerfParam)));
    if (readCount != static_cast<long>(sizeof(LitePerfParam))) {
        DFXLOGE("Failed to read LitePerfParam(%{public}d), readCount(%{public}zd).", errno, readCount);
        return -1;
    }
    return 0;
}

} // namespace HiviewDFX
} // namespace OHOS
