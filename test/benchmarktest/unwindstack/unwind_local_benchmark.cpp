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

#include <memory>
#include <securec.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <unwindstack/Elf.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsGetLocal.h>
#include <unwindstack/Unwinder.h>
#include "dfx_define.h"
#include "dfx_log.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;
static constexpr size_t MAX_FRAMES = 32;

struct UnwindData {
    std::shared_ptr<unwindstack::Memory>& processMemory;
    unwindstack::Maps* maps;
    bool isFillFrames = false;
};

NOINLINE static size_t TestFunc5(size_t (*func)(void*), void* data)
{
    return func(data);
}

NOINLINE static size_t TestFunc4(size_t (*func)(void*), void* data)
{
    return TestFunc5(func, data);
}

NOINLINE static size_t TestFunc3(size_t (*func)(void*), void* data)
{
    return TestFunc4(func, data);
}

NOINLINE static size_t TestFunc2(size_t (*func)(void*), void* data)
{
    return TestFunc3(func, data);
}

NOINLINE static size_t TestFunc1(size_t (*func)(void*), void* data)
{
    return TestFunc2(func, data);
}

static size_t UnwindLocal(MAYBE_UNUSED void* data)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    if (dataPtr == nullptr) {
        return 0;
    }
    std::unique_ptr<unwindstack::Regs> regs(unwindstack::Regs::CreateFromLocal());
    unwindstack::RegsGetLocal(regs.get());
    unwindstack::Unwinder unwinder(MAX_FRAMES, dataPtr->maps, regs.get(), dataPtr->processMemory);
    unwinder.SetResolveNames(dataPtr->isFillFrames);
    unwinder.Unwind();
    auto unwSize = unwinder.NumFrames();
    DFXLOGU("%{public}s frames.size: %{public}zu", __func__, unwSize);
    if (dataPtr->isFillFrames) {
        for (size_t i = 0; i < unwSize; ++i) {
            auto str = unwinder.FormatFrame(i);
            DFXLOGU("%{public}s frames: %{public}s", __func__, str.c_str());
        }
    }
    return unwSize;
}

static void Run(benchmark::State& state, size_t (*func)(void*), void* data)
{
    DFXLOGU("++++++pid: %{public}d", getpid());
    for (const auto& _ : state) {
        if (TestFunc1(func, data) < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    DFXLOGU("------pid: %{public}d", getpid());
}

/**
* @tc.name: BenchmarkUnwindStackLocal
* @tc.desc: UnwindStack local
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackLocal(benchmark::State& state)
{
    auto processMemory = unwindstack::Memory::CreateProcessMemory(getpid());
    unwindstack::LocalMaps maps;
    if (!maps.Parse()) {
        state.SkipWithError("Failed to parse local maps.");
    }

    UnwindData data = {.processMemory = processMemory, .maps = &maps, .isFillFrames = false};
    Run(state, UnwindLocal, &data);
}
BENCHMARK(BenchmarkUnwindStackLocal);

/**
* @tc.name: BenchmarkUnwindStackLocalCache
* @tc.desc: UnwindStack local cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackLocalCache(benchmark::State& state)
{
    auto processMemory = unwindstack::Memory::CreateProcessMemoryCached(getpid());
    unwindstack::LocalMaps maps;
    if (!maps.Parse()) {
        state.SkipWithError("Failed to parse local maps.");
    }

    UnwindData data = {.processMemory = processMemory, .maps = &maps, .isFillFrames = false};
    Run(state, UnwindLocal, &data);
}
BENCHMARK(BenchmarkUnwindStackLocalCache);

/**
* @tc.name: BenchmarkUnwindStackLocalFrames
* @tc.desc: UnwindStack local frames
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackLocalFrames(benchmark::State& state)
{
    auto processMemory = unwindstack::Memory::CreateProcessMemory(getpid());
    unwindstack::LocalMaps maps;
    if (!maps.Parse()) {
        state.SkipWithError("Failed to parse local maps.");
    }

    UnwindData data = {.processMemory = processMemory, .maps = &maps, .isFillFrames = true};
    Run(state, UnwindLocal, &data);
}
BENCHMARK(BenchmarkUnwindStackLocalFrames);

/**
* @tc.name: BenchmarkUnwindStackLocalFramesCache
* @tc.desc: UnwindStack local frames cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackLocalFramesCache(benchmark::State& state)
{
    auto processMemory = unwindstack::Memory::CreateProcessMemoryCached(getpid());
    unwindstack::LocalMaps maps;
    if (!maps.Parse()) {
        state.SkipWithError("Failed to parse local maps.");
    }

    UnwindData data = {.processMemory = processMemory, .maps = &maps, .isFillFrames = true};
    Run(state, UnwindLocal, &data);
}
BENCHMARK(BenchmarkUnwindStackLocalFramesCache);