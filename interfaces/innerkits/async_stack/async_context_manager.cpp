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

void PrintStackWithStackId(uint64_t type, uint64_t stackId)
{
    std::vector<uintptr_t> pcs;
    StackId id;
    id.value = stackId;
    if (!OHOS::HiviewDFX::UniqueStackTable::Instance()->GetPcsByStackId(id, pcs)) {
        DFXLOGD("failed to get pcs by stackId");
        return;
    }

    std::vector<DfxFrame> submitterFrames;
    Unwinder unwinder;
    std::string stackTrace;
    unwinder.GetFramesByPcs(submitterFrames, pcs);
    for (const auto& frame : submitterFrames) {
        std::string framestr = DfxFrameFormatter::GetFrameStr(frame);
        DFXLOGD("frame:%{public}s", framestr.c_str());
    }
}

bool DfxAsyncContextPool::Init()
{
    if (initialized_) {
        return true;
    }
    DFXLOGI("init async context pool");
    std::lock_guard<std::mutex> lock(mutex_);
    memset_s(&(pool_[0]), sizeof(pool_), 0, sizeof(pool_));
    freeListHead_ = &pool_[0];
    for (uint32_t i = 0; i < CHAIN_POOL_SIZE - 1; i++) {
        pool_[i].valid = 0;
        pool_[i].next = &pool_[i + 1];
    }
    pool_[CHAIN_POOL_SIZE - 1].next = 0;
    pool_[CHAIN_POOL_SIZE - 1].valid = 0;
    freeListTail_ = &pool_[CHAIN_POOL_SIZE - 1];

    freeThreadList_ = &threadCtxPool_[0];
    for (uint32_t i = 0; i < THREAD_POOL_SIZE - 1; i++) {
        memset_s(&(threadCtxPool_[i].contexts), sizeof(threadCtxPool_[i].contexts),
            0, sizeof(threadCtxPool_[i].contexts));
        threadCtxPool_[i].valid = 0;
        threadCtxPool_[i].curIndex = 0;
        threadCtxPool_[i].next = &threadCtxPool_[i + 1];
    }
    threadCtxPool_[THREAD_POOL_SIZE - 1].next = 0;

    initialized_ = true;
    DFXLOGI("async context pool init success");
    return true;
}

void DfxAsyncContextPool::DeInit()
{
    if (!initialized_) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    DFXLOGI("async contextPool deinit success");
}

DfxAsyncContext* DfxAsyncContextPool::AcquireAsyncContext()
{
    if (!initialized_) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (freeListHead_ == freeListTail_) {
        return nullptr;
    }
    DfxAsyncContext* ctx = freeListHead_;
    freeListHead_ = freeListHead_->next;
    ctx->valid = 1;
    return ctx;
}

void DfxAsyncContextPool::ReleaseAsyncContext(DfxAsyncContext* ctx)
{
    if (ctx == nullptr || !initialized_) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ctx->valid) {
        return;
    }
    memset_s(&ctx->ctxs[0], sizeof(ctx->ctxs), 0, sizeof(ctx->ctxs));
    freeListTail_->next = ctx;
    freeListTail_ = ctx;
    ctx->valid = 0;
    ctx->next = 0;
}

DfxThreadAsyncContext* DfxAsyncContextPool::AcquireThreadContext()
{
    if (!initialized_) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (freeThreadList_ == nullptr) {
        DFXLOGW("DfxThreadAsyncContext exhausted");
        return nullptr;
    }
    DfxThreadAsyncContext* ctx = freeThreadList_;
    freeThreadList_ = freeThreadList_->next;
    ctx->valid = 1;
    ctx->curIndex = 0;
    return ctx;
}

void DfxAsyncContextPool::ReleaseThreadContext(DfxThreadAsyncContext* ctx)
{
    if (ctx == nullptr || !initialized_) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    ctx->valid = 0;
    ctx->next = freeThreadList_;
    ctx->curIndex = 0;
    freeThreadList_ = ctx;
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
        return;
    }

    if (getpid() == gettid()) {
        return;
    }

    DfxAsyncContextPool::Instance()->ReleaseThreadContext(ctx);
}

bool DfxAsyncContextManager::Init()
{
    if (initialized_) {
        return true;
    }
    if (!DfxAsyncContextPool::Instance()->Init()) {
        DFXLOGE("async context pool init failed");
        return false;
    }
    if (pthread_key_create(&threadAsyncCtxKey_, ThreadAsyncContextDestructor) != 0) {
        DFXLOGE("pthread_key_create failed");
        return false;
    }
    initialized_ = true;
    DFXLOGI("AsyncContextManager init success");
    return true;
}

void DfxAsyncContextManager::DeInit()
{
    if (!initialized_) {
        return;
    }
    DfxAsyncContextPool::Instance()->DeInit();
    pthread_key_delete(threadAsyncCtxKey_);
    initialized_ = false;
    DFXLOGI("AsyncContextManager deinit success");
}

DfxAsyncContext* DfxAsyncContextManager::GetCurrentContext()
{
    if (!initialized_) {
        return nullptr;
    }
    auto threadCtx = static_cast<DfxThreadAsyncContext*>(pthread_getspecific(threadAsyncCtxKey_));
    if (threadCtx == nullptr) {
        DFXLOGW("GetCurrentContext thread context is nullptr");
        return nullptr;
    }
    if (threadCtx->curIndex <= 0 || threadCtx->curIndex >= MAX_THREAD_ASYNC_CTX_DEPTH ||
        !threadCtx->valid) {
        return nullptr;
    }
    int index = threadCtx->curIndex - 1;
    return threadCtx->contexts[index];
}

void DfxAsyncContextManager::PrintChainStack()
{
    static int i = 0;
    auto ctx = GetCurrentContext();
    if (ctx == nullptr) {
        return;
    }
    if (i % PRINT_CHAIN_INTERVAL != 0) {
        i++;
        return;
    }
    i++;
    for (int j = 0; j < DEFAULT_MAX_ASYNC_CHAIN_LAYERS; j++) {
        if (ctx->ctxs[j].id == 0) {
            break;
        }
        DFXLOGD("PrintChainStack: layer %{public}d type %{public}llu, stackId %{public}llu",
            j, (unsigned long long)ctx->ctxs[j].type, (unsigned long long)ctx->ctxs[j].id);
        PrintStackWithStackId(ctx->ctxs[j].type, ctx->ctxs[j].id);
    }
}

void DfxAsyncContextManager::ClearThreadContext(DfxThreadAsyncContext* threadCtx)
{
    if (threadCtx == nullptr || !initialized_) {
        return;
    }
    if (threadCtx->curIndex < 0 || threadCtx->curIndex >= MAX_THREAD_ASYNC_CTX_DEPTH ||
        !threadCtx->valid) {
        DFXLOGW("ClearThreadContext invalid thread context");
        return;
    }
    threadCtx->curIndex = 0;
}

bool DfxAsyncContextManager::IsValidAsyncContext(DfxAsyncContext* ctx)
{
    DfxAsyncContext* begin = 0;
    DfxAsyncContext* end = 0;
    DfxAsyncContextPool::Instance()->GetAsyncContextRange(&begin, &end);
    return ((ctx >= begin) && (ctx <= end));
}

bool DfxAsyncContextManager::RecycleAsyncContext(DfxAsyncContext* ctx)
{
    if (ctx == nullptr) {
        return false;
    }
    DfxAsyncContext* begin = 0;
    DfxAsyncContext* end = 0;
    DfxAsyncContextPool::Instance()->GetAsyncContextRange(&begin, &end);
    if (ctx < begin || ctx > end) {
        DFXLOGW("RecycleAsyncContext invalid");
        return false;
    }
    DFXLOGD("RecycleAsyncContext tid:%{public}d, type %{public}llu",
        gettid(), (unsigned long long)ctx->ctxs[0].type);
    DfxAsyncContextPool::Instance()->ReleaseAsyncContext(ctx);
    return true;
}

void DfxAsyncContextManager::PushAsyncContext(DfxThreadAsyncContext* threadCtx, DfxAsyncContext* ctx)
{
    if (threadCtx->curIndex >= MAX_THREAD_ASYNC_CTX_DEPTH) {
        DFXLOGW("PushAsyncContext curThread layer reach max:%{public}d", gettid());
        return;
    }

    if (!IsValidAsyncContext(ctx)) {
        DFXLOGD("PushAsyncContext invalid tid:%{public}d, ctx:%{public}llu, at index:%{public}d",
            gettid(), (unsigned long long)ctx, threadCtx->curIndex);
        return;
    }
    threadCtx->contexts[threadCtx->curIndex++] = ctx;
}

void DfxAsyncContextManager::PopAsyncContext(DfxThreadAsyncContext* threadCtx)
{
    if (threadCtx->curIndex <= 0) {
        DFXLOGW("PopAsyncContext invalid thread index:%{public}d", gettid());
        return;
    }
    threadCtx->curIndex--;
}

void DfxAsyncContextManager::SetCurrentContext(DfxAsyncContext* ctx)
{
    if (!initialized_) {
        return;
    }
    auto threadCtx = static_cast<DfxThreadAsyncContext*>(pthread_getspecific(threadAsyncCtxKey_));
    if (threadCtx == nullptr) {
        threadCtx = DfxAsyncContextPool::Instance()->AcquireThreadContext();
        if (threadCtx == nullptr) {
            DFXLOGW("SetCurrentContext acquire thread context failed");
            return;
        }
        pthread_setspecific(threadAsyncCtxKey_, threadCtx);
    }

    constexpr uint64_t CLEAR_THREAD_CTX_TYPE = ASYNC_TYPE_FFRT_POOL | ASYNC_TYPE_FFRT_QUEUE | ASYNC_TYPE_EVENTHANDLER;
    if (ctx == nullptr) {
        PopAsyncContext(threadCtx);
        return;
    } else if (IsValidAsyncContext(ctx) && (ctx->ctxs[0].type & CLEAR_THREAD_CTX_TYPE)) {
        ClearThreadContext(threadCtx);
    }
    PushAsyncContext(threadCtx, ctx);
}

DfxAsyncContext* DfxAsyncContextManager::HandleCollectAsyncStack(uint64_t stackId, uint64_t asyncType)
{
    auto ctx = DfxAsyncContextPool::Instance()->AcquireAsyncContext();
    if (ctx == nullptr) {
        DFXLOGW("HandleCollectAsyncStack acquire async context failed");
        return nullptr;
    }
    ctx->ctxs[0].id = stackId;
    ctx->ctxs[0].type = asyncType;
    auto curCtx = GetCurrentContext();
    if (curCtx != nullptr) {
        for (int i = 0; i < DEFAULT_MAX_ASYNC_CHAIN_LAYERS - 1; i++) {
            if (curCtx->ctxs[i].id == 0) {
                break;
            }
            ctx->ctxs[i + 1] = curCtx->ctxs[i];
            ctx->curIndex = i + 1;
        }
    } else {
        ctx->curIndex = 0;
    }
    return ctx;
}

void DfxAsyncContextManager::HandleSetStackId(uint64_t stackId)
{
    auto ctx = reinterpret_cast<DfxAsyncContext*>(stackId);
    SetCurrentContext(ctx);
}
#endif
}
}