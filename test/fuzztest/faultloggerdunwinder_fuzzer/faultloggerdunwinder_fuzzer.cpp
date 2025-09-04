/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include "faultloggerdunwinder_fuzzer.h"

#include <cstddef>
#include <cstdint>

#include "dfx_ark.h"
#include "dfx_hap.h"
#include "dfx_regs.h"
#include "dwarf_op.h"
#include "thread_context.h"
#include "unwinder.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {

struct TestArkFrameData {
    uintptr_t pc;
    uintptr_t fp;
    uintptr_t sp;
    uintptr_t methodid;
};

void TestStepArkFrame(const uint8_t* data, size_t size)
{
    if (data == nullptr || size < sizeof(TestArkFrameData)) {
        return;
    }
    auto testData = *reinterpret_cast<const TestArkFrameData *>(data);
    bool isJsFrame = testData.methodid % 2;

    DfxMemory dfxMemory;
    ArkStepParam arkParam(&testData.fp, &testData.sp, &testData.pc, &isJsFrame);
    DfxArk::Instance().StepArkFrame(&dfxMemory, &(Unwinder::AccessMem), &arkParam);
}

void TestStepArkFrameWithJit(const uint8_t* data, size_t size)
{
    if (data == nullptr || size < sizeof(TestArkFrameData)) {
        return;
    }
    auto testData = *reinterpret_cast<const TestArkFrameData*>(data);
    bool isJsFrame = testData.methodid % 2;
    std::vector<uintptr_t> jitCache_ = {};
    DfxMemory dfxMemory;
    ArkUnwindParam arkParam(&dfxMemory, &(Unwinder::AccessMem), &testData.fp,
        &testData.sp, &testData.pc, &testData.methodid, &isJsFrame, jitCache_);
    DfxArk::Instance().StepArkFrameWithJit(&arkParam);
}

void TestJitCodeWriteFile(const uint8_t* data, size_t size)
{
    struct TestData {
        int fd;
        uintptr_t jitCacheData;
    };
    if (data == nullptr || size < sizeof(TestData)) {
        return;
    }
    const auto testData = reinterpret_cast<const TestData*>(data);
    std::vector<uintptr_t> jitCache = {};
    jitCache.push_back(testData->jitCacheData);
    DfxMemory dfxMemory;
    DfxArk::Instance().JitCodeWriteFile(&dfxMemory, &(Unwinder::AccessMem),
        testData->fd, jitCache.data(), jitCache.size());
}

void TestParseArkFrameInfoLocal(const uint8_t* data, size_t size)
{
    struct TestData {
        uintptr_t pc;
        uintptr_t mapBegin;
        uintptr_t offset;
    };
    if (data == nullptr || size < sizeof(TestData)) {
        return;
    }
    const auto testData = reinterpret_cast<const TestData*>(data);

    JsFunction jsFunction;
    DfxArk::Instance().ParseArkFrameInfoLocal(testData->pc, testData->mapBegin, testData->offset, &jsFunction);
}

void TestArkCreateJsSymbolExtractor(const uint8_t* data, size_t size)
{
    if (data == nullptr || size < sizeof(uintptr_t)) {
        return;
    }
    auto extractorPtr = *reinterpret_cast<const uintptr_t*>(data);
    DfxArk::Instance().ArkCreateJsSymbolExtractor(&extractorPtr);
}

void TestArkDestoryJsSymbolExtractor(const uint8_t* data, size_t size)
{
    if (data == nullptr || size < sizeof(uintptr_t)) {
        return;
    }
    DfxArk::Instance().ArkDestoryJsSymbolExtractor(*reinterpret_cast<const uintptr_t*>(data));
}

void TestDfxArk(const uint8_t* data, size_t size)
{
    TestStepArkFrame(data, size);
    TestStepArkFrameWithJit(data, size);
    TestJitCodeWriteFile(data, size);
    TestParseArkFrameInfoLocal(data, size);
    TestArkCreateJsSymbolExtractor(data, size);
}

void TestDfxHap(const uint8_t* data, size_t size)
{
    struct TestData {
        pid_t pid;
        uint64_t pc;
    };
    if (data == nullptr || size < sizeof(TestData)) {
        return;
    }
    const auto testData = reinterpret_cast<const TestData*>(data);

    auto map = std::make_shared<DfxMap>();
    JsFunction jsFunction;
    DfxHap dfxHap;
    dfxHap.ParseHapInfo(testData->pid, testData->pc, map, &jsFunction);
}

#if defined(__aarch64__)
void TestSetFromFpMiniRegs(const uint8_t* data, size_t size)
{
    struct TestData {
        uintptr_t regs[FP_MINI_REGS_SIZE];
    };
    if (data == nullptr || size < sizeof(TestData)) {
        return;
    }
    const auto testData = reinterpret_cast<const TestData*>(data);

    auto dfxregs = std::make_shared<DfxRegsArm64>();
    dfxregs->SetFromFpMiniRegs(testData->regs, FP_MINI_REGS_SIZE);
}
#endif

#if defined(__aarch64__)
void TestSetFromQutMiniRegs(const uint8_t* data, size_t size)
{
    struct TestData {
        uintptr_t regs[QUT_MINI_REGS_SIZE];
    };
    if (data == nullptr || size < sizeof(TestData)) {
        return;
    }
    const auto testData = reinterpret_cast<const TestData*>(data);
    auto dfxregs = std::make_shared<DfxRegsArm64>();
    dfxregs->SetFromQutMiniRegs(testData->regs, QUT_MINI_REGS_SIZE);
}
#endif

#if defined(__aarch64__)
void TestDfxRegsArm64(const uint8_t* data, size_t size)
{
    TestSetFromFpMiniRegs(data, size);
    TestSetFromQutMiniRegs(data, size);
}
#endif

void TestThreadContext(const uint8_t* data, size_t size)
{
    if (data || size < sizeof(int32_t)) {
        return;
    }
    auto tid = *reinterpret_cast<const int32_t*>(data);
    LocalThreadContext& context = LocalThreadContext::GetInstance();
    uintptr_t stackBottom;
    uintptr_t stackTop;
    context.GetStackRange(tid, stackBottom, stackTop);
    context.CollectThreadContext(tid);
    context.GetThreadContext(tid);
    context.ReleaseThread(tid);
}

void TestDfxInstrStatistic(const uint8_t* data, size_t size)
{
    struct TestData {
        uint32_t type;
        uint64_t val;
        uint64_t errInfo;
    };
    if (data == nullptr || size < sizeof(TestData)) {
        return;
    }
    auto testData = reinterpret_cast<const TestData*>(data);
    std::string soName(reinterpret_cast<const char*>(data), FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH);
    data += FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH;
    InstrStatisticType statisticType = (testData->type % 10) ? InstructionEntriesArmExidx : UnsupportedArmExidx;
    DfxInstrStatistic& statistic = DfxInstrStatistic::GetInstance();
    statistic.SetCurrentStatLib(soName);
    statistic.AddInstrStatistic(statisticType, testData->val, testData->errInfo);
    std::vector<std::pair<uint32_t, uint32_t>> result;
    statistic.DumpInstrStatResult(result);
}

void FaultloggerdUnwinderTest(const uint8_t* data, size_t size)
{
    TestDfxArk(data, size);
    TestDfxHap(data, size);
#if defined(__aarch64__)
    TestDfxRegsArm64(data, size);
#endif
    TestThreadContext(data, size);
    TestDfxInstrStatistic(data, size);
    sleep(1);
}
} // namespace HiviewDFX
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    /* Run your code on data */
    OHOS::HiviewDFX::FaultloggerdUnwinderTest(data, size);
    return 0;
}
