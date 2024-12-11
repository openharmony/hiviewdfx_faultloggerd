/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "dfx_config.h"
#include "dfx_hap.h"
#include "dfx_regs.h"
#include "dfx_xz_utils.h"
#include "dwarf_op.h"
#include "faultloggerd_fuzzertest_common.h"
#include "thread_context.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {
const int FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH = 50;

void TestDfxConfig()
{
    DfxConfig::GetConfig();
}

void TestStepArkFrame(const uint8_t* data, size_t size)
{
    uintptr_t pc;
    uintptr_t fp;
    uintptr_t sp;
    uintptr_t methodid;
    int offsetTotalLength = sizeof(pc) + sizeof(fp) + sizeof(sp) + sizeof(methodid);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, pc);
    STREAM_TO_VALUEINFO(data, fp);
    STREAM_TO_VALUEINFO(data, sp);
    STREAM_TO_VALUEINFO(data, methodid);
    bool isJsFrame = methodid % 2;

    DfxMemory dfxMemory;
    DfxArk::StepArkFrame(&dfxMemory, &(Unwinder::AccessMem), &fp, &sp, &pc, &methodid, &isJsFrame);
}

void TestStepArkFrameWithJit(const uint8_t* data, size_t size)
{
    uintptr_t fp;
    uintptr_t pc;
    uintptr_t sp;
    uintptr_t methodid;
    int offsetTotalLength = sizeof(pc) + sizeof(fp) + sizeof(sp) + sizeof(methodid);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, pc);
    STREAM_TO_VALUEINFO(data, fp);
    STREAM_TO_VALUEINFO(data, sp);
    STREAM_TO_VALUEINFO(data, methodid);
    bool isJsFrame = methodid % 2;

    std::vector<uintptr_t> jitCache_ = {};
    DfxMemory dfxMemory;
    ArkUnwindParam arkParam(&dfxMemory, &(Unwinder::AccessMem), &fp, &sp, &pc, &methodid, &isJsFrame, jitCache_);
    DfxArk::StepArkFrameWithJit(&arkParam);
}

void TestJitCodeWriteFile(const uint8_t* data, size_t size)
{
    int fd;
    uintptr_t jitCacheData;
    int offsetTotalLength = sizeof(fd) + sizeof(jitCacheData);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, fd);
    STREAM_TO_VALUEINFO(data, jitCacheData);

    std::vector<uintptr_t> jitCache = {};
    jitCache.push_back(jitCacheData);
    DfxMemory dfxMemory;
    DfxArk::JitCodeWriteFile(&dfxMemory, &(Unwinder::AccessMem), fd, jitCache.data(), jitCache.size());
}

void TestParseArkFrameInfoLocal(const uint8_t* data, size_t size)
{
    uintptr_t pc;
    uintptr_t funcOffset;
    uintptr_t mapBegin;
    uintptr_t offset;
    int offsetTotalLength = sizeof(pc) + sizeof(funcOffset) + sizeof(mapBegin) + sizeof(offset);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, pc);
    STREAM_TO_VALUEINFO(data, funcOffset);
    STREAM_TO_VALUEINFO(data, mapBegin);
    STREAM_TO_VALUEINFO(data, offset);

    JsFunction jsFunction;
    DfxArk::ParseArkFrameInfoLocal(static_cast<uintptr_t>(pc), static_cast<uintptr_t>(funcOffset),
                                   static_cast<uintptr_t>(mapBegin), static_cast<uintptr_t>(offset), &jsFunction);
}

void TestArkCreateJsSymbolExtractor(const uint8_t* data, size_t size)
{
    uintptr_t extractorPtr;
    if (size < sizeof(extractorPtr)) {
        return;
    }

    STREAM_TO_VALUEINFO(data, extractorPtr);

    DfxArk::ArkCreateJsSymbolExtractor(&extractorPtr);
}

void TestArkDestoryJsSymbolExtractor(const uint8_t* data, size_t size)
{
    uintptr_t extractorPtr;
    if (size < sizeof(extractorPtr)) {
        return;
    }

    STREAM_TO_VALUEINFO(data, extractorPtr);

    DfxArk::ArkDestoryJsSymbolExtractor(extractorPtr);
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
    pid_t pid;
    uint64_t pc;
    uintptr_t methodid;
    uintptr_t offset;
    unsigned int offsetTotalLength = sizeof(pid) + sizeof(pc) + sizeof(methodid) + sizeof(offset);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, pid);
    STREAM_TO_VALUEINFO(data, pc);
    STREAM_TO_VALUEINFO(data, methodid);
    STREAM_TO_VALUEINFO(data, offset);

    auto map = std::make_shared<DfxMap>();
    JsFunction jsFunction;
    DfxHap dfxHap;
    dfxHap.ParseHapInfo(pid, pc, methodid, map, &jsFunction);
}

#if defined(__aarch64__)
void TestSetFromFpMiniRegs(const uint8_t* data, size_t size)
{
    uintptr_t regs;
    if (size < sizeof(regs)) {
        return;
    }

    STREAM_TO_VALUEINFO(data, regs);

    auto dfxregs = std::make_shared<DfxRegsArm64>();
    dfxregs->SetFromFpMiniRegs(&regs, size);
}
#endif

#if defined(__aarch64__)
void TestSetFromQutMiniRegs(const uint8_t* data, size_t size)
{
    uintptr_t regs;
    if (size < sizeof(regs)) {
        return;
    }

    STREAM_TO_VALUEINFO(data, regs);

    auto dfxregs = std::make_shared<DfxRegsArm64>();
    dfxregs->SetFromQutMiniRegs(&regs, size);
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
    int32_t tid;
    uintptr_t stackBottom;
    uintptr_t stackTop;
    unsigned int offsetTotalLength = sizeof(tid) + sizeof(stackBottom) + sizeof(stackTop);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, tid);
    STREAM_TO_VALUEINFO(data, stackBottom);
    STREAM_TO_VALUEINFO(data, stackTop);

    LocalThreadContext& context = LocalThreadContext::GetInstance();
    context.GetStackRange(tid, stackBottom, stackTop);
    context.CollectThreadContext(tid);
    context.GetThreadContext(tid);
    context.ReleaseThread(tid);
}

void TestDfxInstrStatistic(const uint8_t* data, size_t size)
{
    uint32_t type;
    uint64_t val;
    uint64_t errInfo;
    unsigned int offsetTotalLength = sizeof(type) + sizeof(val) + sizeof(errInfo) +
                                     FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH;
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, type);
    type = type % 10; // 10 : get the last digit of the number
    STREAM_TO_VALUEINFO(data, val);
    STREAM_TO_VALUEINFO(data, errInfo);

    std::string soName(reinterpret_cast<const char*>(data), FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH);
    data += FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH;

    InstrStatisticType statisticType;
    if (type == 0) {
        statisticType = InstrStatisticType::InstructionEntriesArmExidx;
    } else {
        statisticType = InstrStatisticType::UnsupportedArmExidx;
    }
    DfxInstrStatistic& statistic = DfxInstrStatistic::GetInstance();
    statistic.SetCurrentStatLib(soName);
    statistic.AddInstrStatistic(statisticType, val, errInfo);
    std::vector<std::pair<uint32_t, uint32_t>> result;
    statistic.DumpInstrStatResult(result);
}

void TestDfxXzUtils(const uint8_t* data, size_t size)
{
    uint8_t src;
    if (size < sizeof(src)) {
        return;
    }

    STREAM_TO_VALUEINFO(data, src);

    std::shared_ptr<std::vector<uint8_t>> out;
    XzDecompress(&src, size, out);
}

void FaultloggerdUnwinderTest(const uint8_t* data, size_t size)
{
    TestDfxConfig();
    TestDfxArk(data, size);
    TestDfxHap(data, size);
#if defined(__aarch64__)
    TestDfxRegsArm64(data, size);
#endif
    TestThreadContext(data, size);
    TestDfxInstrStatistic(data, size);
    TestDfxXzUtils(data, size);
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
