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

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;

struct UnwindData {
    bool isCache = false;
    bool isFillFrames = false;
    bool isFp = false;
};

static void TestFunc6(MAYBE_UNUSED void (*func)(void*), MAYBE_UNUSED void* data)
{
    while (true) {};
    LOGE("Not be run here!!!");
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
        return -1;
    }
    return pid;
}

static size_t UnwinderRemote(std::shared_ptr<Unwinder> unwinder, const pid_t tid)
{
    if (unwinder == nullptr) {
        return 0;
    }
    MAYBE_UNUSED bool unwRet = unwinder->UnwindRemote(tid);
    auto frames = unwinder->GetFrames();
    LOGU("%s frames.size: %zu", __func__, frames.size());
    return frames.size();
}

static size_t UnwinderRemoteFp(std::shared_ptr<Unwinder> unwinder)
{
    if (unwinder == nullptr) {
        return 0;
    }
    auto pid = unwinder->GetTargetPid();
    auto regs = DfxRegs::CreateRemoteRegs(pid);
    unwinder->SetRegs(regs);
    UnwindContext context;
    context.pid = pid;
    unwinder->UnwindByFp(&context);
    auto frames = unwinder->GetPcs();
    LOGU("%s frames.size: %zu", __func__, frames.size());
    return frames.size();
}

static bool GetUnwinder(pid_t pid, void* data, std::shared_ptr<Unwinder>& unwinder, bool& isFp)
{
    static std::unordered_map<pid_t, std::shared_ptr<Unwinder>> unwinders_;
    auto iter = unwinders_.find(pid);
    if (iter != unwinders_.end()) {
        unwinder = iter->second;
    } else {
        unwinder = std::make_shared<Unwinder>(pid);
        unwinders_[pid] = unwinder;
    }

    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    if ((dataPtr != nullptr) && (unwinder != nullptr)) {
        if (dataPtr->isFillFrames) {
            unwinder->EnableFillFrames(true);
        } else {
            unwinder->EnableFillFrames(false);
        }

        if (dataPtr->isCache) {
            unwinder->EnableUnwindCache(true);
        } else {
            unwinder->EnableUnwindCache(false);
        }

        isFp = dataPtr->isFp;
    }
    return true;
}

static void Run(benchmark::State& state, void* data)
{
    pid_t pid = RemoteFork();
    if (pid == -1) {
        state.SkipWithError("Failed to fork remote process.");
        return;
    }
    LOGU("pid: %d", pid);
    TestScopedPidReaper reap(pid);

    std::shared_ptr<Unwinder> unwinder;
    bool isFp = false;
    GetUnwinder(pid, data, unwinder, isFp);

    for (const auto& _ : state) {
        size_t unwSize = 0;
        if (isFp) {
            unwSize = UnwinderRemoteFp(unwinder);
        } else {
            unwSize = UnwinderRemote(unwinder, pid);
        }

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
    UnwindData data;
    data.isCache = false;
    data.isFillFrames = false;
    Run(state, &data);
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
    UnwindData data;
    data.isCache = false;
    data.isFillFrames = false;
    Run(state, &data);
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
    UnwindData data;
    data.isCache = true;
    data.isFillFrames = false;
    Run(state, &data);
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
    data.isCache = false;
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
    data.isCache = true;
    data.isFillFrames = true;
    Run(state, &data);
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
    UnwindData data;
    data.isFp = true;
    Run(state, &data);
}
BENCHMARK(BenchmarkUnwinderRemoteFp);
#endif