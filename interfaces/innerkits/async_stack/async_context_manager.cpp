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

#include "async_context_manager.h"

#include <pthread.h>
#include <string>
#include <cerrno>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "dfx_frame_formatter.h"
#include "dfx_log.h"
#include "fp_backtrace.h"
#include "unique_stack_table.h"
#include "unwinder.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {

#if defined(__aarch64__)
DfxAsyncContextPool* DfxAsyncContextPool::Instance()
{
    static DfxAsyncContextPool pool;
    return &pool;
}

bool DfxAsyncContextPool::Init()
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (initialized_.load()) {
        return true;
    }
    DFXLOGI("init async context pool");
    poolSize_ = GetChainPoolSize();
    if (poolSize_ == 0) {
        DFXLOGE("init async context pool invalid poolSize %{public}u", poolSize_);
        return false;
    }
    size_t poolMemSize = static_cast<size_t>(poolSize_) * sizeof(DfxAsyncContext);
    pool_ = reinterpret_cast<DfxAsyncContext*>(static_cast<uintptr_t>(
        syscall(SYS_mmap, nullptr, poolMemSize, PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0)));
    if (pool_ == MAP_FAILED) {
        pool_ = nullptr;
        DFXLOGE("init async context pool mmap failed poolSize %{public}u, errno %{public}d",
            poolSize_, errno);
        return false;
    }
    freeListHead_ = &pool_[0];
    for (uint32_t i = 0; i < poolSize_ - 1; i++) {
        pool_[i].next = &pool_[i + 1];
        pool_[i].valid.store(false, std::memory_order_relaxed);
    }
    pool_[poolSize_ - 1].next = nullptr;
    pool_[poolSize_ - 1].valid.store(false, std::memory_order_relaxed);
    freeListTail_ = &pool_[poolSize_ - 1];

    freeThreadList_ = &threadCtxPool_[0];
    for (uint32_t i = 0; i < THREAD_POOL_SIZE; i++) {
        (void)memset_s(&(threadCtxPool_[i].contexts), sizeof(threadCtxPool_[i].contexts),
            0, sizeof(threadCtxPool_[i].contexts));
        threadCtxPool_[i].valid.store(false, std::memory_order_relaxed);
        threadCtxPool_[i].curAsyncContextsCnt = 0;
        threadCtxPool_[i].next = (i == THREAD_POOL_SIZE - 1) ? nullptr : &threadCtxPool_[i + 1];
    }

    initialized_.store(true);
    DFXLOGI("async context pool init success");
    return true;
}

void DfxAsyncContextPool::DeInit()
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!initialized_.load()) {
        return;
    }
    initialized_.store(false);
    if (pool_ != nullptr) {
        size_t poolMemSize = static_cast<size_t>(poolSize_) * sizeof(DfxAsyncContext);
        (void)syscall(SYS_munmap, pool_, poolMemSize);
        pool_ = nullptr;
    }
    poolSize_ = 0;
    freeListHead_ = nullptr;
    freeListTail_ = nullptr;
    DFXLOGI("async contextPool deinit success");
}

void DfxAsyncContextPool::ReleaseAsyncContext(DfxAsyncContext* ctx)
{
    if (ctx == nullptr) {
        DFXLOGW("ReleaseAsyncContext ctx is nullptr");
        return;
    }
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!IsValidAsyncContextLocked(ctx)) {
        return;
    }
    DFXLOGD("ReleaseAsyncContext tid:%{public}d, type %{public}llu",
        gettid(), static_cast<unsigned long long>(ctx->ctxs[0].type));
    (void)memset_s(&ctx->ctxs[0], sizeof(ctx->ctxs), 0, sizeof(ctx->ctxs));
    freeListTail_->next = ctx;
    freeListTail_ = ctx;
    ctx->valid.store(false, std::memory_order_release);
    ctx->next = nullptr;
}

DfxThreadAsyncContext* DfxAsyncContextPool::AcquireThreadContext()
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!initialized_.load()) {
        return nullptr;
    }
    if (freeThreadList_ == nullptr) {
        DFXLOGW("DfxThreadAsyncContext exhausted");
        return nullptr;
    }
    DfxThreadAsyncContext* ctx = freeThreadList_;
    freeThreadList_ = freeThreadList_->next;
    ctx->valid.store(true, std::memory_order_release);
    ctx->curAsyncContextsCnt = 0;
    (void)memset_s(&(ctx->contexts), sizeof(ctx->contexts), 0, sizeof(ctx->contexts));
    return ctx;
}

void DfxAsyncContextPool::ReleaseThreadContext(DfxThreadAsyncContext* ctx)
{
    if (ctx == nullptr) {
        DFXLOGW("ReleaseThreadContext ctx is nullptr");
        return;
    }
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!IsValidThreadContextAddressLocked(ctx)) {
        return;
    }
    if (!ctx->valid.load(std::memory_order_acquire)) {
        DFXLOGW("ReleaseThreadContext ctx is invalid");
        return;
    }
    ctx->valid.store(false, std::memory_order_release);
    ctx->next = freeThreadList_;
    ctx->curAsyncContextsCnt = 0;
    freeThreadList_ = ctx;
}

bool DfxAsyncContextPool::IsValidAsyncContextAddress(DfxAsyncContext* ctx)
{
    if (ctx == nullptr) {
        return false;
    }
    std::shared_lock<std::shared_mutex> lock(sharedMutex_);
    return IsValidAsyncContextAddressLocked(ctx);
}

bool DfxAsyncContextPool::IsValidAsyncContextAddressLocked(DfxAsyncContext* ctx)
{
    if (ctx == nullptr) {
        return false;
    }
    if (!initialized_.load() || pool_ == nullptr || poolSize_ == 0) {
        return false;
    }
    return (ctx >= &pool_[0] && ctx <= &pool_[poolSize_ - 1]);
}

bool DfxAsyncContextPool::IsValidThreadContextAddressLocked(DfxThreadAsyncContext* threadCtx)
{
    if (threadCtx == nullptr) {
        return false;
    }
    if (!initialized_.load()) {
        return false;
    }
    return (threadCtx >= &threadCtxPool_[0] &&
        threadCtx <= &threadCtxPool_[THREAD_POOL_SIZE - 1]);
}

bool DfxAsyncContextPool::IsValidThreadContextLocked(DfxThreadAsyncContext* threadCtx)
{
    if (!IsValidThreadContextAddressLocked(threadCtx)) {
        return false;
    }
    return threadCtx->valid.load(std::memory_order_acquire);
}

bool DfxAsyncContextPool::IsValidAsyncContextLocked(DfxAsyncContext* ctx)
{
    if (!IsValidAsyncContextAddressLocked(ctx)) {
        return false;
    }
    return ctx->valid.load(std::memory_order_acquire);
}

DfxAsyncContext* DfxAsyncContextPool::AcquireAndInitAsyncContext(
    DfxThreadAsyncContext* threadCtx, uint64_t stackId, uint64_t asyncType)
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!initialized_.load() || pool_ == nullptr) {
        return nullptr;
    }
    if (freeListHead_ == freeListTail_) {
        DFXLOGW("DfxAsyncContext exhausted");
        return nullptr;
    }
    DfxAsyncContext* ctx = freeListHead_;
    freeListHead_ = freeListHead_->next;
    ctx->valid.store(true, std::memory_order_release);
    ctx->ctxs[0].id = stackId;
    ctx->ctxs[0].type = asyncType;
    if (!IsValidThreadContextLocked(threadCtx) ||
        threadCtx->curAsyncContextsCnt <= 0 ||
        threadCtx->curAsyncContextsCnt > MAX_THREAD_ASYNC_CTX_DEPTH) {
        return ctx;
    }
    int32_t index = threadCtx->curAsyncContextsCnt - 1;
    DfxAsyncContext* curCtx = threadCtx->contexts[index];
    if (!IsValidAsyncContextLocked(curCtx)) {
        return ctx;
    }
    uint32_t copyLimit = GetMaxAsyncChainLayers() - 1;
    for (uint32_t i = 0; i < copyLimit; i++) {
        if (curCtx->ctxs[i].id == 0) {
            break;
        }
        ctx->ctxs[i + 1] = curCtx->ctxs[i];
    }
    return ctx;
}

int32_t DfxAsyncContextPool::GetCurrentChainedContext(
    DfxThreadAsyncContext* threadCtx, DfxAsyncCtx buffer[], size_t sz)
{
    std::shared_lock<std::shared_mutex> lock(sharedMutex_);
    if (threadCtx == nullptr || sz == 0) {
        return 0;
    }
    if (!IsValidThreadContextLocked(threadCtx) ||
        threadCtx->curAsyncContextsCnt <= 0 ||
        threadCtx->curAsyncContextsCnt > MAX_THREAD_ASYNC_CTX_DEPTH) {
        return 0;
    }
    int32_t index = threadCtx->curAsyncContextsCnt - 1;
    DfxAsyncContext* ctx = threadCtx->contexts[index];
    if (!IsValidAsyncContextLocked(ctx)) {
        return 0;
    }
    int32_t count = 0;
    uint32_t layerLimit = GetMaxAsyncChainLayers();
    size_t loopCount = sz > static_cast<size_t>(layerLimit) ? static_cast<size_t>(layerLimit) : sz;
    for (size_t i = 0; i < loopCount; i++) {
        if (ctx->ctxs[i].id == 0) {
            break;
        }
        buffer[i].type = ctx->ctxs[i].type;
        buffer[i].id = ctx->ctxs[i].id;
        count++;
    }
    return count;
}

bool DfxAsyncContextPool::IsAsyncContextHeadTypeMatched(DfxAsyncContext* ctx, uint64_t typeMask)
{
    std::shared_lock<std::shared_mutex> lock(sharedMutex_);
    if (!IsValidAsyncContextLocked(ctx)) {
        return false;
    }
    return (ctx->ctxs[0].type & typeMask) != 0;
}

bool DfxAsyncContextPool::PushContextToThread(DfxThreadAsyncContext* threadCtx, DfxAsyncContext* ctx)
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!IsValidThreadContextLocked(threadCtx)) {
        return false;
    }
    if (threadCtx->curAsyncContextsCnt >= MAX_THREAD_ASYNC_CTX_DEPTH) {
        DFXLOGW("PushContextToThread curThread layer reach max:%{public}d", gettid());
        return false;
    }
    if (!IsValidAsyncContextLocked(ctx)) {
        DFXLOGD("PushContextToThread invalid tid:%{public}d, ctx:%{public}llu, at index:%{public}d",
            gettid(), static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(ctx)),
            threadCtx->curAsyncContextsCnt);
        return false;
    }
    threadCtx->contexts[threadCtx->curAsyncContextsCnt] = ctx;
    threadCtx->curAsyncContextsCnt++;
    return true;
}

bool DfxAsyncContextPool::PopContextFromThread(DfxThreadAsyncContext* threadCtx)
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!IsValidThreadContextLocked(threadCtx)) {
        return false;
    }
    if (threadCtx->curAsyncContextsCnt <= 0) {
        DFXLOGD("PopContextFromThread invalid thread index:%{public}d", gettid());
        return false;
    }
    threadCtx->curAsyncContextsCnt--;
    return true;
}

bool DfxAsyncContextPool::PopContextFromThreadIfMatch(
    DfxThreadAsyncContext* threadCtx, DfxAsyncContext* ctx)
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!IsValidThreadContextLocked(threadCtx)) {
        return false;
    }
    if (threadCtx->curAsyncContextsCnt <= 0 ||
        threadCtx->curAsyncContextsCnt > MAX_THREAD_ASYNC_CTX_DEPTH) {
        DFXLOGW("PopContextFromThreadIfMatch empty stack tid:%{public}d", gettid());
        return false;
    }
    int32_t index = threadCtx->curAsyncContextsCnt - 1;
    if (ctx != threadCtx->contexts[index]) {
        return false;
    }
    threadCtx->curAsyncContextsCnt--;
    return true;
}

bool DfxAsyncContextPool::ClearThreadContext(DfxThreadAsyncContext* threadCtx)
{
    std::lock_guard<std::shared_mutex> lock(sharedMutex_);
    if (!IsValidThreadContextLocked(threadCtx)) {
        DFXLOGW("ClearThreadContext invalid thread context");
        return false;
    }
    if (threadCtx->curAsyncContextsCnt < 0 ||
        threadCtx->curAsyncContextsCnt > MAX_THREAD_ASYNC_CTX_DEPTH) {
        DFXLOGW("ClearThreadContext invalid thread context");
        return false;
    }
    threadCtx->curAsyncContextsCnt = 0;
    return true;
}

DfxAsyncContextManager* DfxAsyncContextManager::Instance()
{
    static DfxAsyncContextManager manager;
    return &manager;
}

void ThreadAsyncContextDestructor(void *arg)
{
    DfxThreadAsyncContext* ctx = (DfxThreadAsyncContext*)arg;
    if (ctx == nullptr) {
        DFXLOGW("ThreadAsyncContextDestructor ctx is nullptr");
        return;
    }

    if (getpid() == gettid()) {
        DFXLOGW("ThreadAsyncContextDestructor pid is equal to tid");
        return;
    }

    DfxAsyncContextPool::Instance()->ReleaseThreadContext(ctx);
}

bool DfxAsyncContextManager::Init()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_.load()) {
        return true;
    }
    if (!DfxAsyncContextPool::Instance()->Init()) {
        DFXLOGE("async context pool init failed");
        return false;
    }
    if (pthread_key_create(&threadAsyncCtxKey_, ThreadAsyncContextDestructor) != 0) {
        DfxAsyncContextPool::Instance()->DeInit();
        DFXLOGE("pthread_key_create failed");
        return false;
    }
    initialized_.store(true);
    DFXLOGI("AsyncContextManager init success");
    return true;
}

void DfxAsyncContextManager::DeInit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_.load()) {
        return;
    }
    initialized_.store(false);
    DfxAsyncContextPool::Instance()->DeInit();
    pthread_key_delete(threadAsyncCtxKey_);
    DFXLOGI("AsyncContextManager deinit success");
}

bool DfxAsyncContextManager::IsValidAsyncContext(DfxAsyncContext* ctx)
{
    return DfxAsyncContextPool::Instance()->IsValidAsyncContextAddress(ctx);
}

bool DfxAsyncContextManager::RecycleAsyncContext(DfxAsyncContext* ctx)
{
    if (ctx == nullptr) {
        DFXLOGD("RecycleAsyncContext ctx is nullptr");
        return false;
    }
    if (!IsValidAsyncContext(ctx)) {
        DFXLOGW("RecycleAsyncContext ctx invalid");
        return false;
    }
    DfxAsyncContextPool::Instance()->ReleaseAsyncContext(ctx);
    return true;
}

DfxThreadAsyncContext* DfxAsyncContextManager::GetOrCreateThreadContext()
{
    auto threadCtx = static_cast<DfxThreadAsyncContext*>(pthread_getspecific(threadAsyncCtxKey_));
    if (threadCtx != nullptr) {
        return threadCtx;
    }
    threadCtx = DfxAsyncContextPool::Instance()->AcquireThreadContext();
    if (threadCtx == nullptr) {
        DFXLOGW("GetOrCreateThreadContext acquire thread context failed");
        return nullptr;
    }
    pthread_setspecific(threadAsyncCtxKey_, threadCtx);
    return threadCtx;
}

int32_t DfxAsyncContextManager::GetCurrentChainedContext(DfxAsyncCtx buffer[], size_t sz)
{
    if (!initialized_.load()) {
        return 0;
    }
    auto threadCtx = static_cast<DfxThreadAsyncContext*>(pthread_getspecific(threadAsyncCtxKey_));
    return DfxAsyncContextPool::Instance()->GetCurrentChainedContext(threadCtx, buffer, sz);
}

void DfxAsyncContextManager::PopCurrentThreadContext(uint64_t stackId)
{
    if (!initialized_.load()) {
        return;
    }
    if (stackId == 0) {
        return;
    }
    DfxAsyncContext* ctx = reinterpret_cast<DfxAsyncContext*>(stackId);
    if (ctx == nullptr) {
        return;
    }
    DfxThreadAsyncContext* threadCtx = GetOrCreateThreadContext();
    if (threadCtx == nullptr) {
        return;
    }
    DfxAsyncContextPool::Instance()->PopContextFromThreadIfMatch(threadCtx, ctx);
}

void DfxAsyncContextManager::SetCurrentThreadContext(uint64_t stackId)
{
    if (!initialized_.load()) {
        return;
    }
    DfxAsyncContext* ctx = (stackId == 0) ? nullptr : reinterpret_cast<DfxAsyncContext*>(stackId);
    DfxThreadAsyncContext* threadCtx = GetOrCreateThreadContext();
    if (threadCtx == nullptr) {
        return;
    }
    constexpr uint64_t CLEAR_THREAD_CTX_TYPE =
        ASYNC_TYPE_FFRT_POOL | ASYNC_TYPE_FFRT_QUEUE | ASYNC_TYPE_EVENTHANDLER;
    if (ctx == nullptr) {
        DfxAsyncContextPool::Instance()->PopContextFromThread(threadCtx);
        return;
    }
    if (DfxAsyncContextPool::Instance()->IsAsyncContextHeadTypeMatched(ctx, CLEAR_THREAD_CTX_TYPE)) {
        DfxAsyncContextPool::Instance()->ClearThreadContext(threadCtx);
    }
    DfxAsyncContextPool::Instance()->PushContextToThread(threadCtx, ctx);
}

DfxAsyncContext* DfxAsyncContextManager::HandleCollectAsyncStack(uint64_t stackId, uint64_t asyncType)
{
    auto threadCtx = static_cast<DfxThreadAsyncContext*>(pthread_getspecific(threadAsyncCtxKey_));
    DfxAsyncContext* ctx =
        DfxAsyncContextPool::Instance()->AcquireAndInitAsyncContext(threadCtx, stackId, asyncType);
    if (ctx == nullptr) {
        DFXLOGW("HandleCollectAsyncStack acquire async context failed");
        return nullptr;
    }
    return ctx;
}
#endif
}
}
