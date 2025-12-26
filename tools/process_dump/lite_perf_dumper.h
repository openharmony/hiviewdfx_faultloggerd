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

#ifndef DFX_LITE_PERF_DUMP_H
#define DFX_LITE_PERF_DUMP_H

#include <cinttypes>
#include <memory>
#include <mutex>

#include "dfx_lperf.h"
#include "nocopyable.h"

namespace OHOS {
namespace HiviewDFX {
class LitePerfDumper final {
public:
    static LitePerfDumper &GetInstance();
    void Perf(int requestFd);

private:
    LitePerfDumper() = default;
    DISALLOW_COPY_AND_MOVE(LitePerfDumper);

    int PerfProcess(LitePerfParam& lperf, int requestFd);
    int32_t ReadLperfAndCheck(LitePerfParam& lperf, int reqeustFd);
    int PerfRecord(int (&pipeWriteFd)[2], LitePerfParam& lperf);
    void WriteSampleData(int bufFd, const std::string& data);
};
} // namespace HiviewDFX
} // namespace OHOS
#endif  // DFX_LITE_PERF_DUMP_H
