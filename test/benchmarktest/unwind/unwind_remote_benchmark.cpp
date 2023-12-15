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
#include "dfx_define.h"
#include "dfx_log.h"
#include <libunwind.h>
#include <libunwind_i-ohos.h>
#include <libunwind-ptrace.h>
#include "dfx_ptrace.h"
#include "dfx_test_util.h"

using namespace OHOS::HiviewDFX;
using namespace std;

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;

struct UnwindData {
    bool isFillFrames = false;
};

static void TestFunc6(MAYBE_UNUSED void (*func)(void*), MAYBE_UNUSED void* data)
{
    while (true);
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

static size_t UnwindRemote(unw_addr_space_t as, UnwindData* dataPtr)
{
    if (as == nullptr) {
        LOGE("as is nullptr");
        return 0;
    }
    unw_set_caching_policy(as, UNW_CACHE_GLOBAL);
    void *context = _UPT_create(as->pid);
    if (context == nullptr) {
        LOGE("_UPT_create");
        return 0;
    }

    unw_cursor_t cursor;
    int unwRet = unw_init_remote(&cursor, as, context);
    if (unwRet != 0) {
        LOGE("unw_init_remote");
        _UPT_destroy(context);
        return 0;
    }

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
    _UPT_destroy(context);
    LOGU("%s pcs.size: %zu", __func__, pcs.size());
    return pcs.size();
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
        unw_addr_space_t as = unw_create_addr_space(&_UPT_accessors, 0);
        unw_set_target_pid(as, pid);
        auto unwSize = UnwindRemote(as, dataPtr);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
        unw_destroy_addr_space(as);
        as = nullptr;
    }
    LOGU("Detach pid: %d", pid);
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

    unw_addr_space_t as;
    GetCacheUnwind(pid, as);

    for (const auto& _ : state) {
        unw_set_target_pid(as, pid);
        auto unwSize = UnwindRemote(as, dataPtr);
        if (unwSize < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
    unw_destroy_addr_space(as);
    as = nullptr;
    LOGU("Detach pid: %d", pid);
    DfxPtrace::Detach(pid);
}

/**
* @tc.name: BenchmarkUnwindRemote
* @tc.desc: Unwind remote
* @tc.type: FUNC
*/
static void BenchmarkUnwindRemote(benchmark::State& state)
{
    Run(state, nullptr);
}
BENCHMARK(BenchmarkUnwindRemote);

/**
* @tc.name: BenchmarkUnwindRemoteCache
* @tc.desc: Unwind remote cache
* @tc.type: FUNC
*/
static void BenchmarkUnwindRemoteCache(benchmark::State& state)
{
    RunCache(state, nullptr);
}
BENCHMARK(BenchmarkUnwindRemoteCache);
