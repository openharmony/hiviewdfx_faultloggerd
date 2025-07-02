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

#include <pthread.h>
#include <securec.h>
#include <threads.h>

#include "dfx_frame_formatter.h"
#include "dfx_log.h"
#include "fp_backtrace.h"
#include "unique_stack_table.h"
#include "unwinder.h"

using namespace OHOS::HiviewDFX;
#if defined(__aarch64__)
static pthread_key_t g_stackidKey;
static bool g_init = false;
static OHOS::HiviewDFX::FpBacktrace* g_fpBacktrace = nullptr;

extern "C" void SetAsyncStackCallbackFunc(void* func) __attribute__((weak));
static void InitAsyncStackInner(void)
{
    if (SetAsyncStackCallbackFunc == nullptr) {
        DFXLOGE("failed to init async stack, could not find SetAsyncStackCallbackFunc.");
        return;
    }

    // init unique stack table
    if (!OHOS::HiviewDFX::UniqueStackTable::Instance()->Init()) {
        DFXLOGE("failed to init unique stack table?.");
        return;
    }

    if (pthread_key_create(&g_stackidKey, nullptr) == 0) {
        g_init = true;
    } else {
        DFXLOGE("failed to create key for stackId.");
        return;
    }

    // set callback for DfxSignalHandler to read stackId
    SetAsyncStackCallbackFunc((void*)(&GetStackId));
    g_fpBacktrace =  OHOS::HiviewDFX::FpBacktrace::CreateInstance();
}

static bool InitAsyncStack(void)
{
    static once_flag onceFlag = ONCE_FLAG_INIT;
    call_once(&onceFlag, InitAsyncStackInner);
    return g_init;
}
#endif

extern "C" uint64_t CollectAsyncStack(void)
{
#if defined(__aarch64__)
    if (!InitAsyncStack()) {
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
#else
    return 0;
#endif
}

extern "C" void SetStackId(uint64_t stackId)
{
#if defined(__aarch64__)
    if (!InitAsyncStack()) {
        return;
    }
    pthread_setspecific(g_stackidKey, reinterpret_cast<void *>(stackId));
#else
    return;
#endif
}

extern "C" uint64_t GetStackId()
{
#if defined(__aarch64__)
    if (!InitAsyncStack()) {
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