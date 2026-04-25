/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#ifndef ASYNC_CONTEXT_MANAGER_H
#define ASYNC_CONTEXT_MANAGER_H

#include <stdint.h>
#include <pthread.h>
#include <mutex>
#include <atomic>
#include "async_stack.h"

namespace OHOS {
namespace HiviewDFX {

constexpr int32_t DEFAULT_MAX_ASYNC_CHAIN_LAYERS = 10;
constexpr uint32_t CHAIN_POOL_SIZE = 1024 * 64;
constexpr int32_t MAX_THREAD_ASYNC_CTX_DEPTH = 64;
constexpr uint32_t THREAD_POOL_SIZE = 1024;
constexpr uint32_t PRINT_CHAIN_INTERVAL = 10;

typedef struct DfxAsyncContext {
    DfxAsyncContext* next;
    bool valid;
    DfxAsyncCtx ctxs[DEFAULT_MAX_ASYNC_CHAIN_LAYERS];
} DfxAsyncContext;

typedef struct DfxThreadAsyncContext {
    DfxAsyncContext* contexts[MAX_THREAD_ASYNC_CTX_DEPTH];
    DfxThreadAsyncContext* next;
    bool valid;
    int32_t curAsyncContextsCnt;
} DfxThreadAsyncContext;

/**
 * @brief Object Pool for Async Context
 */
class DfxAsyncContextPool {
public:
    static DfxAsyncContextPool* Instance();
    bool Init();
    void DeInit();
    DfxAsyncContext* AcquireAsyncContext();
    void ReleaseAsyncContext(DfxAsyncContext* ctx);
    DfxThreadAsyncContext* AcquireThreadContext();
    void ReleaseThreadContext(DfxThreadAsyncContext* ctx);
    void GetAsyncContextRange(DfxAsyncContext** begin, DfxAsyncContext** end)
    {
        *begin = &pool_[0];
        *end = &pool_[CHAIN_POOL_SIZE - 1];
    }
private:
    DfxAsyncContextPool() = default;
    ~DfxAsyncContextPool() = default;
    DfxAsyncContextPool(const DfxAsyncContextPool&) = delete;
    DfxAsyncContextPool& operator=(const DfxAsyncContextPool&) = delete;
    DfxAsyncContext pool_[CHAIN_POOL_SIZE];
    DfxThreadAsyncContext threadCtxPool_[THREAD_POOL_SIZE];
    DfxAsyncContext* freeListHead_{nullptr};
    DfxAsyncContext* freeListTail_{nullptr};
    DfxThreadAsyncContext* freeThreadList_{nullptr};
    std::mutex mutex_;
    std::atomic<bool> initialized_{false};
};

class DfxAsyncContextManager {
public:
    static DfxAsyncContextManager* Instance();
    bool Init();
    void DeInit();
    DfxAsyncContext* HandleCollectAsyncStack(uint64_t stackId, uint64_t asyncType);
    DfxAsyncContext* GetCurrentContext();
    void SetCurrentThreadContext(uint64_t stackId);

    bool RecycleAsyncContext(DfxAsyncContext* ctx);
    bool IsValidAsyncContext(DfxAsyncContext* ctx);
    void PrintChainStack();
private:
    DfxAsyncContextManager() = default;
    ~DfxAsyncContextManager() = default;
    DfxAsyncContextManager(const DfxAsyncContextManager&) = delete;
    DfxAsyncContextManager& operator=(const DfxAsyncContextManager&) = delete;
    void ClearThreadContext(DfxThreadAsyncContext* threadCtx);
    void PushAsyncContext(DfxThreadAsyncContext* threadCtx, DfxAsyncContext* ctx);
    void PopAsyncContext(DfxThreadAsyncContext* threadCtx);
    pthread_key_t threadAsyncCtxKey_;
    std::atomic<bool> initialized_;
};

} // namespace HiviewDFX
} // namespace OHOS

#endif // ASYNC_CONTEXT_MANAGER_H