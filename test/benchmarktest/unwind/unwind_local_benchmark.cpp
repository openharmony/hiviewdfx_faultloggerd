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

#include <securec.h>
#include <string>
#include <vector>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include <libunwind.h>
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;

NOINLINE static size_t TestFunc5(size_t (*func)(void))
{
    return func();
}

NOINLINE static size_t TestFunc4(size_t (*func)(void))
{
    return TestFunc5(func);
}

NOINLINE static size_t TestFunc3(size_t (*func)(void))
{
    return TestFunc4(func);
}

NOINLINE static size_t TestFunc2(size_t (*func)(void))
{
    return TestFunc3(func);
}

NOINLINE static size_t TestFunc1(size_t (*func)(void))
{
    return TestFunc2(func);
}

static size_t UnwindLocal()
{
    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    std::vector<unw_word_t> pcs;
    size_t index = 0;
    unw_word_t pc = 0;
    unw_word_t prevPc = 0;
    do {
        if ((unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) || (prevPc == pc)) {
            break;
        }
        pcs.emplace_back(pc);
        prevPc = pc;
        index++;
    } while (unw_step(&cursor) > 0);
    LOGU("%s pcs.size: %zu", __func__, pcs.size());
    return pcs.size();
}

static void Run(benchmark::State& state, size_t (*func)(void))
{
    LOGU("++++++pid: %d", getpid());
    for (const auto& _ : state) {
        if (TestFunc1(func) < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    LOGU("------pid: %d", getpid());
}

/**
* @tc.name: BenchmarkUnwindLocal
* @tc.desc: Unwind local
* @tc.type: FUNC
*/
static void BenchmarkUnwindLocal(benchmark::State& state)
{
    Run(state, UnwindLocal);
}
BENCHMARK(BenchmarkUnwindLocal);

/**
* @tc.name: BenchmarkUnwindLocalCache
* @tc.desc: Unwind local cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindLocalCache(benchmark::State& state)
{
    Run(state, UnwindLocal);
}
BENCHMARK(BenchmarkUnwindLocalCache);
