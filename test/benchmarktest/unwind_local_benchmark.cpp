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
#include "dfx_test_util.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
using namespace std;

#define TEST_MIN_UNWIND_FRAMES 5

static size_t TestFunc5(size_t (*func)(void*), void* data)
{
    return func(data);
}

static size_t TestFunc4(size_t (*func)(void*), void* data)
{
    return TestFunc5(func, data);
}

static size_t TestFunc3(size_t (*func)(void*), void* data)
{
    return TestFunc4(func, data);
}

static size_t TestFunc2(size_t (*func)(void*), void* data)
{
    return TestFunc3(func, data);
}

static size_t TestFunc1(size_t (*func)(void*), void* data)
{
    return TestFunc2(func, data);
}

static size_t UnwinderLocal(MAYBE_UNUSED void* data) {
    auto unwinder = std::make_shared<Unwinder>();
    MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
    auto pcs = unwinder->GetPcs();
    return pcs.size();
}

/**
* @tc.name: BenchmarkUnwinderLocal
* @tc.desc: Unwind local
* @tc.type: FUNC
* @tc.require:
*/
static void BenchmarkUnwinderLocal(benchmark::State& state)
{
    for (const auto& _ : state) {
        if (TestFunc1(UnwinderLocal, nullptr) < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
}
BENCHMARK(BenchmarkUnwinderLocal);
