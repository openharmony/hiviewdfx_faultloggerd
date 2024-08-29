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
#include <threads.h>
#include "dfx_param.h"
#include "unique_stack_table.h"
#include "fp_unwinder.h"

#include "dfx_log.h"

#if defined(__aarch64__)
static pthread_key_t g_stackidKey;
static bool g_init = false;

extern "C" void SetAsyncStackCallbackFunc(void* func) __attribute__((weak));
static void InitAsyncStackInner(void)
{
    if (SetAsyncStackCallbackFunc == nullptr) {
        LOGE("%s", "failed to init async stack, could not find SetAsyncStackCallbackFunc.");
        return;
    }

    // init unique stack table
    if (!OHOS::HiviewDFX::UniqueStackTable::Instance()->Init()) {
        LOGE("%s", "failed to init unique stack table?.");
        return;
    }

    if (pthread_key_create(&g_stackidKey, nullptr) == 0) {
        g_init = true;
    } else {
        LOGE("%s", "failed to create key for stackId.");
        return;
    }

    // set callback for DfxSignalHandler to read stackId
    SetAsyncStackCallbackFunc((void*)(&GetStackId));
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
    const int32_t maxSize = 16;
    uintptr_t pcs[maxSize] = {0};
    int32_t skipFrameNum = 2;
    size_t sz = static_cast<size_t>(OHOS::HiviewDFX::FpUnwinder::Unwind(pcs, maxSize, skipFrameNum));
    uint64_t stackId = 0;
    auto stackIdPtr = reinterpret_cast<OHOS::HiviewDFX::StackId*>(&stackId);
    OHOS::HiviewDFX::UniqueStackTable::Instance()->PutPcsInTable(stackIdPtr, pcs, sz);
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