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
#ifndef DFX_LPERF_H
#define DFX_LPERF_H

#include <cinttypes>

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr int MIN_SAMPLE_TIDS = 1;
constexpr int MAX_SAMPLE_TIDS = 10;
constexpr int MIN_SAMPLE_FREQUENCY = 1;
constexpr int MAX_SAMPLE_FREQUENCY = 200;
constexpr int MIN_STOP_SECONDS = 1;
constexpr int MAX_STOP_SECONDS = 10000;
constexpr int DUMP_LITEPERF_TIMEOUT = 5000;
}

/**
 * @brief lite perf struct
 * It serves as the public definition of the lite perf.
 */
struct LitePerfParam {
    int pid;
    int tids[MAX_SAMPLE_TIDS];
    int freq = 0;
    int durationMs = 0;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
