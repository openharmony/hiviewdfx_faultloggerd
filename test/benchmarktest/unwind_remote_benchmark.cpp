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
#include "dfx_test_util.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
using namespace std;

#define TEST_MIN_UNWIND_FRAMES 5

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
        exit(0);
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

/**
* @tc.name: BenchmarkUnwinderRemote
* @tc.desc: Unwind remote
* @tc.type: FUNC
* @tc.require:
*/
static void BenchmarkUnwinderRemote(benchmark::State& state)
{
    pid_t pid = RemoteFork();
    if (pid == -1) {
        state.SkipWithError("Failed to fork remote process.");
        return;
    }
    LOGU("pid: %d", pid);
    TestScopedPidReaper reap(pid);

    for (const auto& _ : state) {
        auto unwinder = std::make_shared<Unwinder>(pid);
        MAYBE_UNUSED bool unwRet = unwinder->UnwindRemote();
        auto pcs = unwinder->GetPcs();
        if (pcs.size() < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    LOGU("Detach pid: %d", pid);
    DfxPtrace::Detach(pid);
}
BENCHMARK(BenchmarkUnwinderRemote);
