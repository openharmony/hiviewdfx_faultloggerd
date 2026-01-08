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
#include "fp_unwinder.h"
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
    bool isCache = false;
    bool isFillFrames = false;
    bool isFp = false;
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

static bool GetUnwinder(void* data, std::shared_ptr<Unwinder>& unwinder, bool& isFp)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    if ((dataPtr != nullptr) && (dataPtr->unwinder != nullptr)) {
        unwinder = dataPtr->unwinder;
        unwinder->EnableFillFrames(dataPtr->isFillFrames);
        unwinder->EnableUnwindCache(dataPtr->isCache);
        isFp = dataPtr->isFp;
        return true;
    }
    DFXLOGE("Failed to get unwinder");
    return false;
}

static size_t UnwinderLocal(MAYBE_UNUSED void* data)
{
    std::shared_ptr<Unwinder> unwinder = nullptr;
    bool isFp = false;
    if (!GetUnwinder(data, unwinder, isFp)) {
        return 0;
    }
    MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal(false, isFp);
    size_t unwSize = 0;
    if (isFp) {
        unwSize = unwinder->GetPcs().size();
    } else {
        unwSize = unwinder->GetFrames().size();
    }
    DFXLOGU("%{public}s frames.size: %{public}zu", __func__, unwSize);
    return unwSize;
}

#if defined(__aarch64__)
static size_t UnwinderLocalFp(MAYBE_UNUSED void* data) {
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    if ((dataPtr == nullptr) || (dataPtr->unwinder == nullptr)) {
        return 0;
    }

    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    if (!dataPtr->unwinder->GetStackRange(stackBottom, stackTop)) {
        return 0;
    }
    uintptr_t miniRegs[FP_MINI_REGS_SIZE] = {0};
    GetFramePointerMiniRegs(miniRegs, sizeof(miniRegs) / sizeof(miniRegs[0]));
    auto regs = DfxRegs::CreateFromRegs(UnwindMode::FRAMEPOINTER_UNWIND, miniRegs,
                                        sizeof(miniRegs) / sizeof(miniRegs[0]));
    dataPtr->unwinder->SetRegs(regs);
    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
    dataPtr->unwinder->EnableFpCheckMapExec(false);
    dataPtr->unwinder->UnwindByFp(&context);
    auto unwSize = dataPtr->unwinder->GetPcs().size();
    DFXLOGU("%{public}s frames.size: %{public}zu", __func__, unwSize);

    if (dataPtr->isFillFrames) {
        auto& pcs = dataPtr->unwinder->GetPcs();
        std::vector<DfxFrame> frames;
        for (size_t i = 0; i < unwSize; ++i) {
            DfxFrame frame;
            frame.index = i;
            frame.pc = static_cast<uint64_t>(pcs[i]);
            frames.emplace_back(frame);
        }
        Unwinder::FillLocalFrames(frames);
        DFXLOGU("%{public}s", Unwinder::GetFramesStr(frames).c_str());
    }
    return unwSize;
}

static size_t FpUnwinderLocal(MAYBE_UNUSED void* data) {
    uintptr_t regs[2]; // 2: pc and fp reg
    FpUnwinder::GetPcFpRegs(regs);
    const size_t maxSize = 32;
    uintptr_t pcs[maxSize] = {0};
    auto unwSize = FpUnwinder::GetPtr()->Unwind(regs[0], regs[1], pcs, maxSize);
    DFXLOGU("%{public}s frames.size: %{public}zu", __func__, unwSize);

    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    if (dataPtr != nullptr && dataPtr->isFillFrames) {
        std::vector<DfxFrame> frames;
        for (auto i = 0; i < unwSize; ++i) {
            DfxFrame frame;
            frame.index = i;
            frame.pc = static_cast<uint64_t>(pcs[i]);
            frames.emplace_back(frame);
        }
        Unwinder::FillLocalFrames(frames);
        DFXLOGU("%{public}s", Unwinder::GetFramesStr(frames).c_str());
    }
    return unwSize;
}

static size_t FpUnwinderLocalSafe(MAYBE_UNUSED void* data) {
    uintptr_t regs[2]; // 2: pc and fp reg
    FpUnwinder::GetPcFpRegs(regs);
    const size_t maxSize = 32;
    uintptr_t pcs[maxSize] = {0};
    auto unwSize = FpUnwinder::GetPtr()->UnwindSafe(regs[0], regs[1], pcs, maxSize);
    DFXLOGU("%{public}s frames.size: %{public}zu", __func__, unwSize);

    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    if (dataPtr != nullptr && dataPtr->isFillFrames) {
        std::vector<DfxFrame> frames;
        for (auto i = 0; i < unwSize; ++i) {
            DfxFrame frame;
            frame.index = i;
            frame.pc = static_cast<uint64_t>(pcs[i]);
            frames.emplace_back(frame);
        }
        Unwinder::FillLocalFrames(frames);
        DFXLOGU("%{public}s", Unwinder::GetFramesStr(frames).c_str());
    }
    return unwSize;
}
#endif

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
    data.isCache = false;
    data.isFillFrames = false;
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
    data.isCache = false;
    data.isFillFrames = false;
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
    data.isCache = true;
    data.isFillFrames = false;
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
    data.isCache = false;
    data.isFillFrames = true;
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
    data.isCache = true;
    data.isFillFrames = true;
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
    data.isCache = true;
    data.isFillFrames = true;
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
    data.isCache = true;
    data.isFillFrames = true;
    Run(state, UnwinderLocal, &data);
    UnwinderConfig::SetEnableMiniDebugInfo(false);
    UnwinderConfig::SetEnableLoadSymbolLazily(false);
}
BENCHMARK(BenchmarkUnwinderLocalQutMiniDebugInfosLazily);

#if defined(__aarch64__)
/**
* @tc.name: BenchmarkUnwinderLocalByFp
* @tc.desc: Unwind local by fp
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalByFp(benchmark::State& state)
{
    DfxRegsQut::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>(false);
    data.isCache = true;
    data.isFillFrames = false;
    data.isFp = true;
    Run(state, UnwinderLocal, &data);
}
BENCHMARK(BenchmarkUnwinderLocalByFp);

/**
* @tc.name: BenchmarkUnwinderLocalFp
* @tc.desc: Unwind local fp
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalFp(benchmark::State& state)
{
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>(false);
    data.isFp = true;
    data.isFillFrames = false;
    Run(state, UnwinderLocalFp, &data);
}
BENCHMARK(BenchmarkUnwinderLocalFp);

/**
* @tc.name: BenchmarkUnwinderLocalFpFrames
* @tc.desc: Unwind local fp Frames
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalFpFrames(benchmark::State& state)
{
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>(false);
    data.isFp = true;
    data.isFillFrames = true;
    Run(state, UnwinderLocalFp, &data);
}
BENCHMARK(BenchmarkUnwinderLocalFpFrames);

/**
* @tc.name: BenchmarkFpUnwinderLocal
* @tc.desc: FpUnwinder Unwind
* @tc.type: FUNC
*/
static void BenchmarkFpUnwinderLocal(benchmark::State& state)
{
    UnwindData data;
    data.isFillFrames = false;
    Run(state, FpUnwinderLocal, &data);
}
BENCHMARK(BenchmarkFpUnwinderLocal);

/**
* @tc.name: BenchmarkFpUnwinderLocalFrames
* @tc.desc: FpUnwinder Unwind Frames
* @tc.type: FUNC
*/
static void BenchmarkFpUnwinderLocalFrames(benchmark::State& state)
{
    UnwindData data;
    data.isFillFrames = true;
    Run(state, FpUnwinderLocal, &data);
}
BENCHMARK(BenchmarkFpUnwinderLocalFrames);

/**
* @tc.name: BenchmarkFpUnwinderLocalSafe
* @tc.desc: FpUnwinder UnwindSafe
* @tc.type: FUNC
*/
static void BenchmarkFpUnwinderLocalSafe(benchmark::State& state)
{
    UnwindData data;
    data.isFillFrames = false;
    Run(state, FpUnwinderLocalSafe, &data);
}
BENCHMARK(BenchmarkFpUnwinderLocalSafe);

/**
* @tc.name: BenchmarkFpUnwinderLocalSafeFrames
* @tc.desc: FpUnwinder UnwindSafe Frames
* @tc.type: FUNC
*/
static void BenchmarkFpUnwinderLocalSafeFrames(benchmark::State& state)
{
    UnwindData data;
    data.isFillFrames = true;
    Run(state, FpUnwinderLocalSafe, &data);
}
BENCHMARK(BenchmarkFpUnwinderLocalSafeFrames);
#endif
