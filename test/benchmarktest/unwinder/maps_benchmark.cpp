/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "dfx_map.h"
#include "dfx_maps.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxMaps"

using namespace OHOS::HiviewDFX;
using namespace std;

/**
* @tc.name: BenchmarkMapsCreate
* @tc.desc: Maps Create
* @tc.type: FUNC
*/
static void BenchmarkMapsCreate(benchmark::State& state)
{
    for (const auto& _ : state) {
        auto dfxMaps = DfxMaps::Create();
        LOGU("%s:: maps.size: %zu", __func__, dfxMaps->GetMapsSize());
    }
}
BENCHMARK(BenchmarkMapsCreate);

/**
* @tc.name: BenchmarkMapsCreate2
* @tc.desc: Maps Create
* @tc.type: FUNC
*/
static void BenchmarkMapsCreate2(benchmark::State& state)
{
    std::vector<std::shared_ptr<DfxMap>> maps {};
    std::vector<int> mapIndex {};
    for (const auto& _ : state) {
        if (DfxMaps::Create(0, maps, mapIndex)) {
            LOGU("%s:: maps.size: %zu", __func__, maps.size());
        }
    }
}
BENCHMARK(BenchmarkMapsCreate2);