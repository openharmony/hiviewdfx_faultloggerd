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
#include <sys/ptrace.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/Unwinder.h>
#include "MemoryRemote.h"
#include "pid_utils.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;
static constexpr size_t MAX_FRAMES = 32;

struct UnwindData {
    bool isCache = false;
    bool isFillFrames = false;
};

static void TestFunc6(MAYBE_UNUSED void (*func)(void*), MAYBE_UNUSED volatile bool* ready)
{
    *ready = true;
    while (true);
    LOGE("Not be run here!!!");
}

static void TestFunc5(void (*func)(void*), volatile bool* ready)
{
    return TestFunc6(func, ready);
}

static void TestFunc4(void (*func)(void*), volatile bool* ready)
{
    return TestFunc5(func, ready);
}

static void TestFunc3(void (*func)(void*), volatile bool* ready)
{
    return TestFunc4(func, ready);
}

static void TestFunc2(void (*func)(void*), volatile bool* ready)
{
    return TestFunc3(func, ready);
}

static void TestFunc1(void (*func)(void*), volatile bool* ready)
{
    return TestFunc2(func, ready);
}

static bool WaitForRemote(pid_t pid, volatile bool* readyPtr)
{
    return PidUtils::WaitForPidState(pid, [pid, readyPtr]() {
        unwindstack::MemoryRemote memory(pid);
        bool ready;
        uint64_t readyAddr = reinterpret_cast<uint64_t>(readyPtr);
        if (memory.ReadFully(readyAddr, &ready, sizeof(ready)) && ready) {
            return PidRunEnum::PID_RUN_PASS;
        }
        return PidRunEnum::PID_RUN_KEEP_GOING;
    });
}

static pid_t RemoteFork()
{
    static volatile bool ready = false;

    pid_t pid;
    if ((pid = fork()) == 0) {
        TestFunc1(nullptr, &ready);
        _exit(0);
    }
    if (pid == -1) {
        return -1;
    }

    if (!WaitForRemote(pid, &ready)) {
        LOGE("Failed to wait pid: %d", pid);
        TestScopedPidReaper::Kill(pid);
        return -1;
    }
    return pid;
}

static size_t UnwindRemote(unwindstack::Unwinder unwinder, MAYBE_UNUSED UnwindData* dataPtr)
{
    if (dataPtr != nullptr) {
        unwinder.SetResolveNames(dataPtr->isFillFrames);
    }
    unwinder.Unwind();
    auto unwSize = unwinder.NumFrames();
    LOGU("%s frames.size: %zu", __func__, unwSize);
    if (dataPtr != nullptr && dataPtr->isFillFrames) {
        for (size_t i = 0; i < unwSize; ++i) {
            auto str = unwinder.FormatFrame(i);
            LOGU("%s frames: %s", __func__, str.c_str());
        }
    }
    return unwSize;
}

static void Run(benchmark::State& state, void* data)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    UnwindData unwindData;
    if (dataPtr != nullptr) {
        unwindData.isCache = dataPtr->isCache;
    }

    pid_t pid = RemoteFork();
    if (pid == -1) {
        state.SkipWithError("Failed to fork remote process.");
        return;
    }
    LOGU("pid: %d", pid);
    TestScopedPidReaper reap(pid);

    std::shared_ptr<unwindstack::Memory> processMemory;
    if (unwindData.isCache) {
        processMemory = unwindstack::Memory::CreateProcessMemoryCached(pid);
    } else {
        processMemory = unwindstack::Memory::CreateProcessMemory(pid);
    }
    unwindstack::RemoteMaps maps(pid);
    if (!maps.Parse()) {
        state.SkipWithError("Failed to parse maps.");
    }

    for (const auto& _ : state) {
        std::unique_ptr<unwindstack::Regs> regs(unwindstack::Regs::RemoteGet(pid));
        unwindstack::Unwinder unwinder(MAX_FRAMES, &maps, regs.get(), processMemory);
        auto unwSize = UnwindRemote(unwinder, dataPtr);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    LOGU("Detach pid: %d", pid);
    ptrace(PTRACE_DETACH, pid, 0, 0);
}

/**
* @tc.name: BenchmarkUnwindStackRemote
* @tc.desc: UnwindStack remote
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackRemote(benchmark::State& state)
{
    UnwindData data;
    data.isCache = false;
    Run(state, &data);
}
BENCHMARK(BenchmarkUnwindStackRemote);

/**
* @tc.name: BenchmarkUnwindStackRemoteCache
* @tc.desc: UnwindStack remote cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackRemoteCache(benchmark::State& state)
{
    UnwindData data;
    data.isCache = true;
    Run(state, &data);
}
BENCHMARK(BenchmarkUnwindStackRemoteCache);

/**
* @tc.name: BenchmarkUnwindStackRemoteFrames
* @tc.desc: UnwindStack remote frames
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackRemoteFrames(benchmark::State& state)
{
    UnwindData data;
    data.isCache = false;
    data.isFillFrames = true;
    Run(state, &data);
}
BENCHMARK(BenchmarkUnwindStackRemoteFrames);

/**
* @tc.name: BenchmarkUnwindStackRemoteFramesCache
* @tc.desc: UnwindStack remote frames cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindStackRemoteFramesCache(benchmark::State& state)
{
    UnwindData data;
    data.isCache = true;
    data.isFillFrames = true;
    Run(state, &data);
}
BENCHMARK(BenchmarkUnwindStackRemoteFramesCache);