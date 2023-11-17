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
#include "dfx_ptrace.h"
#include "dfx_regs_qut.h"
#include "dfx_test_util.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
using namespace std;

#define TEST_MIN_UNWIND_FRAMES 5

struct UnwindData {
    bool isFillFrames = false;
};

static void TestFunc6(MAYBE_UNUSED void (*func)(void*), MAYBE_UNUSED void* data)
{
    while (true);
}

static void TestFunc5(void (*func)(void*), void* data)
{
    return TestFunc6(func, data);
}

static void TestFunc4(void (*func)(void*), void* data)
{
    return TestFunc5(func, data);
}

static void TestFunc3(void (*func)(void*), void* data)
{
    return TestFunc4(func, data);
}

static void TestFunc2(void (*func)(void*), void* data)
{
    return TestFunc3(func, data);
}

static void TestFunc1(void (*func)(void*), void* data)
{
    return TestFunc2(func, data);
}

static pid_t RemoteFork()
{
    pid_t pid;
    if ((pid = fork()) == 0) {
        TestFunc1(nullptr, nullptr);
        _exit(0);
    }
    if (pid == -1) {
        return -1;
    }
    if (!DfxPtrace::Attach(pid)) {
        LOGE("Failed to attach pid: %d", pid);
        TestScopedPidReaper::Kill(pid);
    }
    return pid;
}

static size_t UnwinderRemote(std::shared_ptr<Unwinder> unwinder, UnwindData* dataPtr)
{
    UnwindData data;
    if (dataPtr != nullptr) {
        data.isFillFrames = dataPtr->isFillFrames;
    }
    if (unwinder == nullptr) {
        return 0;
    }
    MAYBE_UNUSED bool unwRet = unwinder->UnwindRemote();
    if (data.isFillFrames) {
        auto frames = unwinder->GetFrames();
        return frames.size();
    } else {
        auto pcs = unwinder->GetPcs();
        LOGU("%s pcs.size: %zu", __func__, pcs.size());
        return pcs.size();
    }
}

static void Run(benchmark::State& state, void* data)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);

    pid_t pid = RemoteFork();
    if (pid == -1) {
        state.SkipWithError("Failed to fork remote process.");
        return;
    }
    LOGU("pid: %d", pid);
    TestScopedPidReaper reap(pid);

    for (const auto& _ : state) {
        auto unwinder = std::make_shared<Unwinder>(pid);
        auto unwSize = UnwinderRemote(unwinder, dataPtr);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    LOGU("Detach pid: %d", pid);
    DfxPtrace::Detach(pid);
}

static void GetCacheUnwinder(pid_t pid, std::shared_ptr<Unwinder>& unwinder)
{
    static std::unordered_map<pid_t, std::shared_ptr<Unwinder>> unwinders_;
    auto iter = unwinders_.find(pid);
    if (iter != unwinders_.end()) {
        unwinder = iter->second;
    } else {
        unwinder = std::make_shared<Unwinder>(pid);
        unwinders_[pid] = unwinder;
    }
}

static void RunCache(benchmark::State& state, void* data)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);

    pid_t pid = RemoteFork();
    if (pid == -1) {
        state.SkipWithError("Failed to fork remote process.");
        return;
    }
    LOGU("pid: %d", pid);
    TestScopedPidReaper reap(pid);

    std::shared_ptr<Unwinder> unwinder;
    GetCacheUnwinder(pid, unwinder);

    for (const auto& _ : state) {
        auto unwSize = UnwinderRemote(unwinder, dataPtr);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    LOGU("Detach pid: %d", pid);
    DfxPtrace::Detach(pid);
}

/**
* @tc.name: BenchmarkUnwinderRemoteFull
* @tc.desc: Unwind remote full
* @tc.type: FUNC
*/
static void BenchmarkUnwinderRemoteFull(benchmark::State& state)
{
    std::vector<uint16_t> qutRegs;
    for (uint16_t i = REG_EH; i < REG_LAST; ++i) {
        qutRegs.push_back(i);
    }
    DfxRegsQut::SetQutRegs(qutRegs);
    Run(state, nullptr);
}
BENCHMARK(BenchmarkUnwinderRemoteFull);

/**
* @tc.name: BenchmarkUnwinderRemoteQut
* @tc.desc: Unwind remote qut
* @tc.type: FUNC
*/
static void BenchmarkUnwinderRemoteQut(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    Run(state, nullptr);
}
BENCHMARK(BenchmarkUnwinderRemoteQut);

/**
* @tc.name: BenchmarkUnwinderRemoteQutCache
* @tc.desc: Unwind remote qut cache
* @tc.type: FUNC
*/
static void BenchmarkUnwinderRemoteQutCache(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    RunCache(state, nullptr);
}
BENCHMARK(BenchmarkUnwinderRemoteQutCache);

/**
* @tc.name: BenchmarkUnwinderRemoteQutFrames
* @tc.desc: Unwind remote qut frames
* @tc.type: FUNC
*/
static void BenchmarkUnwinderRemoteQutFrames(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.isFillFrames = true;
    Run(state, &data);
}
BENCHMARK(BenchmarkUnwinderRemoteQutFrames);

/**
* @tc.name: BenchmarkUnwinderRemoteQutFramesCache
* @tc.desc: Unwind remote qut frames cache
* @tc.type: FUNC
*/
static void BenchmarkUnwinderRemoteQutFramesCache(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.isFillFrames = true;
    RunCache(state, &data);
}
BENCHMARK(BenchmarkUnwinderRemoteQutFramesCache);

#if defined(__aarch64__)
/**
* @tc.name: BenchmarkUnwinderRemoteFp
* @tc.desc: Unwind remote fp
* @tc.type: FUNC
*/
static void BenchmarkUnwinderRemoteFp(benchmark::State& state)
{
    pid_t pid = RemoteFork();
    if (pid == -1) {
        state.SkipWithError("Failed to fork remote process.");
        return;
    }
    LOGU("pid: %d", pid);
    TestScopedPidReaper reap(pid);

    std::shared_ptr<Unwinder> unwinder;
    GetCacheUnwinder(pid, unwinder);

    for (const auto& _ : state) {
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        unwinder->UnwindByFp(&context);
        size_t unwSz = unwinder->GetPcs().size();
        if (unwSz < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    LOGU("Detach pid: %d", pid);
    DfxPtrace::Detach(pid);
}
BENCHMARK(BenchmarkUnwinderRemoteFp);
#endif