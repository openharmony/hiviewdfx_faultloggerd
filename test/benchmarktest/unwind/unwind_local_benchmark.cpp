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
#include <libunwind_i-ohos.h>
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;

struct UnwindData {
    UnwindData() = default;
    ~UnwindData()
    {
        if (as != nullptr) {
            unw_destroy_local_address_space(as);
        }
    }
    unw_addr_space_t as = nullptr;
    bool isFillFrames = false;
};

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

static size_t UnwindLocal(MAYBE_UNUSED void* data)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    bool isCache = false;
    unw_addr_space_t as = nullptr;
    if (dataPtr != nullptr) {
        as = dataPtr->as;
    }
    if (as == nullptr) {
        unw_init_local_address_space(&as);
    } else {
        isCache = true;
    }

    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local_with_as(as, &cursor, &context);

    std::vector<unw_word_t> pcs;
    size_t index = 0;
    unw_word_t sp = 0, pc = 0, prevPc, relPc;
    do {
        if ((unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) || (prevPc == pc)) {
            break;
        }
        pcs.push_back(pc);
        prevPc = pc;
        relPc = unw_get_rel_pc(&cursor);
        unw_word_t sz = unw_get_previous_instr_sz(&cursor);
        if ((index > 0) && (relPc > sz)) {
            relPc -= sz;
            pc -= sz;
        }

        struct map_info* map = unw_get_map(&cursor);
        if (map == nullptr) {
            break;
        }

        if (unw_step(&cursor) <= 0) {
            break;
        }
        index++;
    } while (true);
    if (!isCache) {
        unw_destroy_local_address_space(as);
    }
    LOGU("%s pcs.size: %zu", __func__, pcs.size());
    return pcs.size();
}

static void Run(benchmark::State& state, size_t (*func)(void*), void* data)
{
    LOGU("++++++pid: %d", getpid());
    for (const auto& _ : state) {
        if (TestFunc1(func, data) < TEST_MIN_UNWIND_FRAMES) {
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
    Run(state, UnwindLocal, nullptr);
}
BENCHMARK(BenchmarkUnwindLocal);

/**
* @tc.name: BenchmarkUnwindLocalCache
* @tc.desc: Unwind local cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindLocalCache(benchmark::State& state)
{
    UnwindData data;
    unw_init_local_address_space(&(data.as));
    Run(state, UnwindLocal, &data);
}
BENCHMARK(BenchmarkUnwindLocalCache);
