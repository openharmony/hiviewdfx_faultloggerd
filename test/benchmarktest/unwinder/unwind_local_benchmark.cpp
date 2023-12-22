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
#include <unistd.h>
#include "dfx_log.h"
#include "dfx_regs_get.h"
#include "dfx_regs_qut.h"
#include "dfx_test_util.h"
#include "unwinder.h"
#include "unwinder_config.h"

using namespace OHOS::HiviewDFX;
using namespace std;

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxUnwinderLocal"

static constexpr size_t TEST_MIN_UNWIND_FRAMES = 5;

struct UnwindData {
    std::shared_ptr<Unwinder> unwinder = nullptr;
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

static bool GetUnwinder(void* data, std::shared_ptr<Unwinder>& unwinder)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    if ((dataPtr != nullptr) && (dataPtr->unwinder != nullptr)) {
        unwinder = dataPtr->unwinder;
        return true;
    }
    LOGE("Failed to get unwinder");
    return false;
}

static size_t UnwinderLocal(MAYBE_UNUSED void* data)
{
    std::shared_ptr<Unwinder> unwinder = nullptr;
    if (!GetUnwinder(data, unwinder)) {
        return 0;
    }

    MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
    auto unwSize = unwinder->GetFrames().size();
    LOGU("%s frames.size: %zu", __func__, unwSize);
    return unwSize;
}

#if defined(__aarch64__)
static size_t UnwinderLocalFp(MAYBE_UNUSED void* data) {
    std::shared_ptr<Unwinder> unwinder = nullptr;
    if (!GetUnwinder(data, unwinder)) {
        return 0;
    }

    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    unwinder->GetStackRange(stackBottom, stackTop);
    uintptr_t miniRegs[FP_MINI_REGS_SIZE] = {0};
    GetFramePointerMiniRegs(miniRegs);
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::FRAMEPOINTER_UNWIND, miniRegs);
    unwinder->SetRegs(regs);
    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
    unwinder->EnableFpCheckMapExec(false);
    unwinder->UnwindByFp(&context);
    auto unwSize = unwinder->GetPcs().size();
    LOGU("%s frames.size: %zu", __func__, unwSize);
    return unwSize;
}
#endif

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
* @tc.name: BenchmarkUnwinderLocalFull
* @tc.desc: Unwind local full
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalFull(benchmark::State& state)
{
    std::vector<uint16_t> qutRegs;
    for (uint16_t i = REG_EH; i < REG_LAST; ++i) {
        qutRegs.emplace_back(i);
    }
    DfxRegsQut::SetQutRegs(qutRegs);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.unwinder->EnableUnwindCache(false);
    data.unwinder->EnableFillFrames(false);
    Run(state, UnwinderLocal, &data);
}
BENCHMARK(BenchmarkUnwinderLocalFull);

/**
* @tc.name: BenchmarkUnwinderLocalQut
* @tc.desc: Unwind local qut
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQut(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.unwinder->EnableUnwindCache(false);
    data.unwinder->EnableFillFrames(false);
    Run(state, UnwinderLocal, &data);
}
BENCHMARK(BenchmarkUnwinderLocalQut);

/**
* @tc.name: BenchmarkUnwinderLocalQutCache
* @tc.desc: Unwind local qut cache
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQutCache(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.unwinder->EnableUnwindCache(true);
    data.unwinder->EnableFillFrames(false);
    Run(state, UnwinderLocal, &data);
}
BENCHMARK(BenchmarkUnwinderLocalQutCache);

/**
* @tc.name: BenchmarkUnwinderLocalQutFrames
* @tc.desc: Unwind local qut frames
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQutFrames(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.unwinder->EnableUnwindCache(false);
    data.unwinder->EnableFillFrames(true);
    Run(state, UnwinderLocal, &data);
}
BENCHMARK(BenchmarkUnwinderLocalQutFrames);

/**
* @tc.name: BenchmarkUnwinderLocalQutFramesCache
* @tc.desc: Unwind local qut Frames cache
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQutFramesCache(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.unwinder->EnableUnwindCache(true);
    data.unwinder->EnableFillFrames(true);
    Run(state, UnwinderLocal, &data);
}
BENCHMARK(BenchmarkUnwinderLocalQutFramesCache);

/**
* @tc.name: BenchmarkUnwinderLocalQutMiniDebugInfos
* @tc.desc: Unwind local qut minidebuginfo
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQutMiniDebugInfos(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.unwinder->EnableUnwindCache(false);
    data.unwinder->EnableFillFrames(true);
    Run(state, UnwinderLocal, &data);
    UnwinderConfig::SetEnableMiniDebugInfo(false);
}
BENCHMARK(BenchmarkUnwinderLocalQutMiniDebugInfos);

/**
* @tc.name: BenchmarkUnwinderLocalQutMiniDebugInfosLazily
* @tc.desc: Unwind local qut minidebuginfo lazily
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQutMiniDebugInfosLazily(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.unwinder->EnableUnwindCache(true);
    data.unwinder->EnableFillFrames(true);
    Run(state, UnwinderLocal, &data);
    UnwinderConfig::SetEnableMiniDebugInfo(false);
    UnwinderConfig::SetEnableLoadSymbolLazily(false);
}
BENCHMARK(BenchmarkUnwinderLocalQutMiniDebugInfosLazily);

#if defined(__aarch64__)
/**
* @tc.name: BenchmarkUnwinderLocalFp
* @tc.desc: Unwind local fp
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalFp(benchmark::State& state)
{
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    Run(state, UnwinderLocalFp, &data);
}
BENCHMARK(BenchmarkUnwinderLocalFp);
#endif