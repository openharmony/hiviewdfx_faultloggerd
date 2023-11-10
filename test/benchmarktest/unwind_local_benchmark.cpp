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
#include "dfx_regs_get.h"
#include "dfx_test_util.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
using namespace std;

#define TEST_MIN_UNWIND_FRAMES 5

struct UnwindData {
    std::shared_ptr<Unwinder> unwinder = nullptr;
    bool isFillFrames = false;
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

static size_t UnwinderLocal(MAYBE_UNUSED void* data)
{
    UnwindData* dataPtr = reinterpret_cast<UnwindData*>(data);
    UnwindData unwindData;
    if (dataPtr != nullptr) {
        unwindData.unwinder = dataPtr->unwinder;
        unwindData.isFillFrames = dataPtr->isFillFrames;
    }
    if (unwindData.unwinder == nullptr) {
        unwindData.unwinder = std::make_shared<Unwinder>();
    }

    MAYBE_UNUSED bool unwRet = unwindData.unwinder->UnwindLocal();
    if (unwindData.isFillFrames) {
        auto frames = unwindData.unwinder->GetFrames();
        return frames.size();
    }
    auto pcs = unwindData.unwinder->GetPcs();
    return pcs.size();
}

#if defined(__aarch64__)
static size_t UnwinderLocalFp(MAYBE_UNUSED void* data) {
    auto unwinder = std::make_shared<Unwinder>();
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

    int idx = 0;
    uintptr_t pc, fp;
    while (true) {
        pc = regs->GetPc();
        fp = regs->GetFp();
        if (!unwinder->FpStep(fp, pc, &context) || (pc == 0)) {
            break;
        }
        idx++;
    };
    return idx;
}
#endif

static void Run(benchmark::State& state, size_t (*func)(void*), void* data)
{
    for (const auto& _ : state) {
        if (TestFunc1(func, data) < TEST_MIN_UNWIND_FRAMES) {
            state.SkipWithError("Failed to unwind.");
        }
    }
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
        qutRegs.push_back(i);
    }
    DfxRegs::SetQutRegs(qutRegs);
    Run(state, UnwinderLocal, nullptr);
}
BENCHMARK(BenchmarkUnwinderLocalFull);

/**
* @tc.name: BenchmarkUnwinderLocalQut
* @tc.desc: Unwind local qut
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQut(benchmark::State& state)
{
    DfxRegs::SetQutRegs(QUT_REGS);
    Run(state, UnwinderLocal, nullptr);
}
BENCHMARK(BenchmarkUnwinderLocalQut);

/**
* @tc.name: BenchmarkUnwinderLocalQutCache
* @tc.desc: Unwind local qut cache
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalQutCache(benchmark::State& state)
{
    DfxRegs::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
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
    DfxRegs::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = nullptr;
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
    DfxRegs::SetQutRegs(QUT_REGS);
    UnwindData data;
    data.unwinder = std::make_shared<Unwinder>();
    data.isFillFrames = true;
    Run(state, UnwinderLocal, &data);
}
BENCHMARK(BenchmarkUnwinderLocalQutFramesCache);

#if defined(__aarch64__)
/**
* @tc.name: BenchmarkUnwinderLocalFp
* @tc.desc: Unwind local fp
* @tc.type: FUNC
*/
static void BenchmarkUnwinderLocalFp(benchmark::State& state)
{
    Run(state, UnwinderLocalFp, nullptr);
}
BENCHMARK(BenchmarkUnwinderLocalFp);
#endif