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
#include <unistd.h>
#include <vector>
#include "dfx_define.h"
#include "dfx_log.h"
#include <libunwind.h>
#include <libunwind-ptrace.h>
#include "dfx_ptrace.h"
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;

NOINLINE static void TestFunc6(MAYBE_UNUSED void (*func)(void*))
{
    while (true) {};
    DFXLOGE("Not be run here!!!");
}

NOINLINE static void TestFunc5(void (*func)(void*))
{
    return TestFunc6(func);
}

NOINLINE static void TestFunc4(void (*func)(void*))
{
    return TestFunc5(func);
}

NOINLINE static void TestFunc3(void (*func)(void*))
{
    return TestFunc4(func);
}

NOINLINE static void TestFunc2(void (*func)(void*))
{
    return TestFunc3(func);
}

NOINLINE static void TestFunc1(void (*func)(void*))
{
    return TestFunc2(func);
}

static size_t UnwindRemote(pid_t pid, unw_addr_space_t as)
{
    if (as == nullptr) {
        DFXLOGE("as is nullptr");
        return 0;
    }
    unw_set_caching_policy(as, UNW_CACHE_GLOBAL);
    void *context = _UPT_create(pid);
    if (context == nullptr) {
        DFXLOGE("_UPT_create");
        return 0;
    }

    unw_cursor_t cursor;
    int unwRet = unw_init_remote(&cursor, as, context);
    if (unwRet != 0) {
        DFXLOGE("unw_init_remote");
        _UPT_destroy(context);
        return 0;
    }

    std::vector<unw_word_t> pcs;
    size_t index = 0;
    unw_word_t pc = 0;
    unw_word_t prevPc;
    do {
        if ((unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) || (prevPc == pc)) {
            break;
        }
        pcs.emplace_back(pc);
        prevPc = pc;
        index++;
    } while (unw_step(&cursor) > 0);
    _UPT_destroy(context);
    DFXLOGU("%{public}s pcs.size: %{public}zu", __func__, pcs.size());
    return pcs.size();
}

static void Run(benchmark::State& state)
{
    pid_t pid = fork();
    if (pid == 0) {
        TestFunc1(nullptr);
        _exit(0);
    } else if (pid < 0) {
        return;
    }
    if (!DfxPtrace::Attach(pid)) {
        DFXLOGE("Failed to attach pid: %{public}d", pid);
        TestScopedPidReaper::Kill(pid);
        return;
    }

    DFXLOGU("pid: %{public}d", pid);
    TestScopedPidReaper reap(pid);

    for (const auto& _ : state) {
        unw_addr_space_t as = unw_create_addr_space(&_UPT_accessors, 0);
        auto unwSize = UnwindRemote(pid, as);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
        unw_destroy_addr_space(as);
        as = nullptr;
    }
    DFXLOGU("Detach pid: %{public}d", pid);
    DfxPtrace::Detach(pid);
}

static void GetCacheUnwind(pid_t pid, unw_addr_space_t& as)
{
    static std::unordered_map<pid_t, unw_addr_space_t> ass_;
    auto iter = ass_.find(pid);
    if (iter != ass_.end()) {
        as = iter->second;
    } else {
        as = unw_create_addr_space(&_UPT_accessors, 0);
        ass_[pid] = as;
    }
}

static void RunCache(benchmark::State& state)
{
    pid_t pid = fork();
    if (pid == 0) {
        TestFunc1(nullptr);
        _exit(0);
    } else if (pid < 0) {
        return;
    }
    if (!DfxPtrace::Attach(pid)) {
        DFXLOGE("Failed to attach pid: %{public}d", pid);
        TestScopedPidReaper::Kill(pid);
        return;
    }
    DFXLOGU("pid: %{public}d", pid);
    TestScopedPidReaper reap(pid);

    unw_addr_space_t as;
    GetCacheUnwind(pid, as);

    for (const auto& _ : state) {
        auto unwSize = UnwindRemote(pid, as);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    unw_destroy_addr_space(as);
    as = nullptr;
    DFXLOGU("Detach pid: %{public}d", pid);
    DfxPtrace::Detach(pid);
}

/**
* @tc.name: BenchmarkUnwindRemote
* @tc.desc: Unwind remote
* @tc.type: FUNC
*/
static void BenchmarkUnwindRemote(benchmark::State& state)
{
    Run(state);
}
BENCHMARK(BenchmarkUnwindRemote);

/**
* @tc.name: BenchmarkUnwindRemoteCache
* @tc.desc: Unwind remote cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindRemoteCache(benchmark::State& state)
{
    RunCache(state);
}
BENCHMARK(BenchmarkUnwindRemoteCache);
