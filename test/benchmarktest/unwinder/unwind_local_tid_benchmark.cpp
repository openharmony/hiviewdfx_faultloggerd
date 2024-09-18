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

#include <atomic>
#include <benchmark/benchmark.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>
#include "dfx_log.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
using namespace std;

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxUnwinderLocalTid"

#define NOINLINE __attribute__((noinline))
static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;
static constexpr int NUM_ONE = 1;
static constexpr int NUM_TWO = 2;
static constexpr int NUM_THREE = 3;
static constexpr int NUM_FOUR = 4;
static constexpr int NUM_FIVE = 5;

NOINLINE int TestFunc6(std::atomic_int* tid, std::atomic_bool* done)
{
    tid->store(gettid());
    while (!done->load()) {
    }
    return NUM_ONE;
}

NOINLINE int TestFunc5(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc6(tid, done);
    return val * val + NUM_FIVE;
}

NOINLINE int TestFunc4(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc5(tid, done);
    return val * val + NUM_FOUR;
}

NOINLINE int TestFunc3(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc4(tid, done);
    return val * val + NUM_THREE;
}

NOINLINE int TestFunc2(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc3(tid, done);
    return val * val + NUM_TWO;
}

NOINLINE int TestFunc1(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc2(tid, done);
    return val * val + NUM_ONE;
}

/**
* @tc.name: BenchmarkUnwinderLocalWithTid
* @tc.desc: Unwind local with tid
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalWithTid(benchmark::State& state)
{
    static std::mutex mutex;
    constexpr int waitThreadTime = 1000;
    std::atomic_int tid(0);
    std::atomic_bool done(false);
    std::thread th([&tid, &done] { TestFunc1(&tid, &done); });
    while (tid.load() == 0) {
        usleep(waitThreadTime);
        LOGU("wait thread");
    }
    LOGU("+++tid: %d", tid.load());
#if defined(__aarch64__)
    auto unwinder = std::make_shared<Unwinder>(false);
#else
    auto unwinder = std::make_shared<Unwinder>();
#endif
    unwinder->EnableFillFrames(false);
    for (const auto& _ : state) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!unwinder->UnwindLocalWithTid(tid.load())) {
            continue;
        }
        auto unwSize = unwinder->GetPcs().size();
        //rk 64 frames size is 14, rk 32 frames size is 9
        LOGU("%s, frames.size: %zu", __func__, unwSize);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }

    done.store(true);
    LOGU("---tid: %d", tid.load());
    th.join();
}
BENCHMARK(BenchmarkUnwinderLocalWithTid);