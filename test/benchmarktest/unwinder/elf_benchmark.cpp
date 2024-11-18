/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include "dfx_log.h"
#include "dfx_elf.h"
#include "dfx_map.h"
#include "dfx_maps.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static void InitializeBuildId(benchmark::State& state, DfxMaps* dfxMaps, DfxMap** buildIdMap)
{
    auto maps = dfxMaps->GetMaps();
    if (maps.size() == 0) {
        state.SkipWithError("Failed to get local maps.");
        return;
    }

    // Find the libc.so share library and use that for benchmark purposes.
    *buildIdMap = nullptr;
    for (auto& map : maps) {
        auto elf = map->GetElf();
        if (elf == nullptr) {
            continue;
        }
        if (map->offset == 0 && elf->GetBuildId() != "") {
            *buildIdMap = map.get();
            break;
        }
    }

    if (*buildIdMap == nullptr) {
        state.SkipWithError("Failed to find a map with a BuildId.");
    }
}

/**
* @tc.name: BenchmarkElfGetBuildIdFromObj
* @tc.desc: Elf Get BuildId From obj
* @tc.type: FUNC
*/
static void BenchmarkElfGetBuildIdFromObj(benchmark::State& state)
{
    auto dfxMaps = DfxMaps::Create();
    DfxMap* buildIdMap;
    InitializeBuildId(state, dfxMaps.get(), &buildIdMap);

    auto elf = buildIdMap->GetElf();
    if (elf == nullptr) {
        return;
    }
    if (!elf->IsValid()) {
        state.SkipWithError("Cannot get valid elf from map.");
    }

    for (const auto& _ : state) {
        state.PauseTiming();
        elf->SetBuildId("");
        state.ResumeTiming();
        benchmark::DoNotOptimize(elf->GetBuildId());
    }
}
BENCHMARK(BenchmarkElfGetBuildIdFromObj);
