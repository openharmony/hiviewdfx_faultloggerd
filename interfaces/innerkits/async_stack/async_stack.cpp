/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "async_stack.h"

#include <dlfcn.h>
#include <pthread.h>
#include <securec.h>
#include <threads.h>

#include "dfx_frame_formatter.h"
#include "dfx_log.h"
#include "fp_backtrace.h"
#include "unique_stack_table.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
static bool g_init = false;
#if defined(__aarch64__)
static pthread_key_t g_stackidKey;
static OHOS::HiviewDFX::FpBacktrace* g_fpBacktrace = nullptr;
using SetStackIdFn = void(*)(uint64_t stackId);
using CollectAsyncStackFn = uint64_t(*)();
using UvSetAsyncStackFn = void(*)(CollectAsyncStackFn collectAsyncStackFn, SetStackIdFn setStackIdFn);
using FFRTSetAsyncStackFn = void(*)(CollectAsyncStackFn collectAsyncStackFn, SetStackIdFn setStackIdFn);

typedef uint64_t(*GetStackIdFunc)(void);
extern "C" void DFX_SetAsyncStackCallback(GetStackIdFunc func) __attribute__((weak));

extern "C" uint64_t DfxCollectAsyncStack(void)
{
    if (!g_init) {
        return 0;
    }
    const uint32_t maxSize = 16;
    void* pcArray[maxSize] = {0};
    if (g_fpBacktrace == nullptr) {
        return 0;
    }
    size_t size = g_fpBacktrace->BacktraceFromFp(__builtin_frame_address(0), pcArray, maxSize);
    uint64_t stackId = 0;
    auto stackIdPtr = reinterpret_cast<OHOS::HiviewDFX::StackId*>(&stackId);
    uintptr_t* pcs = reinterpret_cast<uintptr_t*>(pcArray);
    OHOS::HiviewDFX::UniqueStackTable::Instance()->PutPcsInTable(stackIdPtr, pcs, size);
    return stackId;
}

extern "C" void DfxSetSubmitterStackId(uint64_t stackId)
{
    if (!g_init) {
        return;
    }
    pthread_setspecific(g_stackidKey, reinterpret_cast<void *>(stackId));
}

void DfxSetAsyncStackCallback(void)
{
    // set callback for DfxSignalHandler to read stackId
    if (DFX_SetAsyncStackCallback == nullptr) {
        return;
    }
    DFX_SetAsyncStackCallback(DfxGetSubmitterStackId);
    const char* uvSetAsyncStackFnName = "LibuvSetAsyncStackFunc";
    auto uvSetAsyncStackFn = reinterpret_cast<UvSetAsyncStackFn>(dlsym(RTLD_DEFAULT, uvSetAsyncStackFnName));
    if (uvSetAsyncStackFn != nullptr) {
        uvSetAsyncStackFn(DfxCollectAsyncStack, DfxSetSubmitterStackId);
    }

    const char* ffrtSetAsyncStackFnName = "FFRTSetAsyncStackFunc";
    auto ffrtSetAsyncStackFn = reinterpret_cast<FFRTSetAsyncStackFn>(dlsym(RTLD_DEFAULT, ffrtSetAsyncStackFnName));
    if (ffrtSetAsyncStackFn != nullptr) {
        ffrtSetAsyncStackFn(DfxCollectAsyncStack, DfxSetSubmitterStackId);
    }
}
#endif

bool DfxInitAsyncStack()
{
#if defined(__aarch64__)
    // init unique stack table
    if (!OHOS::HiviewDFX::UniqueStackTable::Instance()->Init()) {
        DFXLOGE("failed to init unique stack table?.");
        return false;
    }

    if (pthread_key_create(&g_stackidKey, nullptr) != 0) {
        DFXLOGE("failed to create key for stackId.");
        return false;
    }
    g_fpBacktrace =  OHOS::HiviewDFX::FpBacktrace::CreateInstance();
    DfxSetAsyncStackCallback();
    g_init = true;
#endif
    return g_init;
}

extern "C" uint64_t DfxGetSubmitterStackId()
{
#if defined(__aarch64__)
    if (!g_init) {
        return 0;
    }
    return reinterpret_cast<uint64_t>(pthread_getspecific(g_stackidKey));
#else
    return 0;
#endif
}

extern "C" int DfxGetSubmitterStackLocal(char* stackTraceBuf, size_t bufferSize)
{
#if defined(__aarch64__)
    uint64_t stackId = reinterpret_cast<uint64_t>(pthread_getspecific(g_stackidKey));
    std::vector<uintptr_t> pcs;
    StackId id;
    id.value = stackId;
    if (!OHOS::HiviewDFX::UniqueStackTable::Instance()->GetPcsByStackId(id, pcs)) {
        DFXLOGW("Failed to get pcs by stackId");
        return -1;
    }
    std::vector<DfxFrame> submitterFrames;
    Unwinder unwinder;
    std::string stackTrace;
    unwinder.GetFramesByPcs(submitterFrames, pcs);
    for (const auto& frame : submitterFrames) {
        stackTrace += DfxFrameFormatter::GetFrameStr(frame);
    }
    auto result = strncpy_s(stackTraceBuf, bufferSize, stackTrace.c_str(), stackTrace.size());
    if (result != EOK) {
        DFXLOGE("strncpy failed, err = %{public}d.", result);
        return -1;
    }
    return 0;
#else
    return -1;
#endif
}