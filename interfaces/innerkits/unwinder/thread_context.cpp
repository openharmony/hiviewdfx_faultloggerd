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

#include "thread_context.h"

#include <chrono>
#include <csignal>
#include <map>
#include <memory>
#include <mutex>
#include <securec.h>
#include <sigchain.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "fp_unwinder.h"
#if defined(__aarch64__)
#include "unwind_arm64_define.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxThreadContext"

std::mutex g_localMutex;
std::map<int32_t, std::shared_ptr<ThreadContext>> g_contextMap {};
const std::chrono::seconds g_timeOut = std::chrono::seconds(1);

void CreateContext(std::shared_ptr<ThreadContext>& threadContext)
{
#ifndef __aarch64__
    std::unique_lock<std::mutex> lock(threadContext->mtx);
    if (threadContext->ctx == nullptr) {
        threadContext->ctx = new ucontext_t;
    }
    (void)memset_s(threadContext->ctx, sizeof(ucontext_t), 0, sizeof(ucontext_t));
#endif
}

void ReleaseContext(std::shared_ptr<ThreadContext> threadContext)
{
#ifndef __aarch64__
    std::unique_lock<std::mutex> lock(threadContext->mtx);
    if (threadContext->ctx != nullptr) {
        delete threadContext->ctx;
        threadContext->ctx = nullptr;
    }
#endif
}

std::shared_ptr<ThreadContext> GetContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end() || it->second == nullptr) {
        auto threadContext = std::make_shared<ThreadContext>();
        threadContext->tid = tid;
        threadContext->frameSz = 0;
        CreateContext(threadContext);
        g_contextMap[tid] = threadContext;
        return threadContext;
    }

    if (it->second->tid == ThreadContextStatus::CONTEXT_UNUSED) {
        it->second->tid = tid;
        it->second->frameSz = 0;
        CreateContext(it->second);
        return it->second;
    }
    DFXLOGE("GetContextLocked nullptr, tid: %{public}d", tid);
    return nullptr;
}

AT_UNUSED bool RemoveContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end()) {
        DFXLOGW("Context of tid(%{public}d) is already removed.", tid);
        return true;
    }
    if (it->second == nullptr) {
        g_contextMap.erase(it);
        return true;
    }

    // only release ucontext_t object
    if (it->second->tid == ThreadContextStatus::CONTEXT_UNUSED) {
        ReleaseContext(it->second);
        return true;
    }

    DFXLOGW("Failed to release context of tid(%{public}d), still using?", tid);
    return false;
}

bool RemoveAllContextLocked()
{
    auto it = g_contextMap.begin();
    while (it != g_contextMap.end()) {
        if (it->second == nullptr) {
            it = g_contextMap.erase(it);
            continue;
        }
        if (it->second->tid == ThreadContextStatus::CONTEXT_UNUSED) {
            ReleaseContext(it->second);
        }
        it++;
    }
    return true;
}
}

LocalThreadContext& LocalThreadContext::GetInstance()
{
    static LocalThreadContext instance;
    return instance;
}

NO_SANITIZE std::shared_ptr<ThreadContext> LocalThreadContext::GetThreadContext(int32_t tid)
{
    std::unique_lock<std::mutex> lock(localMutex_);
    auto it = g_contextMap.find(tid);
    if (it != g_contextMap.end()) {
        return it->second;
    }
    DFXLOGW("Failed to get context of tid(%{public}d)", tid);
    return nullptr;
}

void LocalThreadContext::ReleaseThread(int32_t tid)
{
    std::unique_lock<std::mutex> lock(localMutex_);
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end() || it->second == nullptr) {
        return;
    }
    it->second->cv.notify_all();
}

void LocalThreadContext::CleanUp()
{
    std::unique_lock<std::mutex> lock(localMutex_);
    RemoveAllContextLocked();
}

std::shared_ptr<ThreadContext> LocalThreadContext::CollectThreadContext(int32_t tid)
{
    std::unique_lock<std::mutex> lock(localMutex_);
    auto threadContext = GetContextLocked(tid);
    if (threadContext == nullptr) {
        DFXLOGW("Failed to get context of tid(%{public}d), still using?", tid);
        return nullptr;
    }

    InitSignalHandler();
    if (!SignalRequestThread(tid, threadContext.get())) {
        return nullptr;
    }
    threadContext->cv.wait_for(lock, g_timeOut);
    return threadContext;
}

NO_SANITIZE void LocalThreadContext::CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context)
{
    if (si == nullptr || si->si_code != DUMP_TYPE_LOCAL || context == nullptr) {
        return;
    }

    int tid = gettid();
    DFXLOGU("tid(%{public}d) recv sig(%{public}d)", tid, sig);
    auto ctxPtr = LocalThreadContext::GetInstance().GetThreadContext(tid);
#if defined(__aarch64__)
    if (ctxPtr == nullptr) {
        return;
    }
    uintptr_t fp = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.regs[REG_FP];
    uintptr_t pc = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.pc;
    ctxPtr->firstFrameSp = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.sp;
    ctxPtr->frameSz = FpUnwinder::GetPtr()->UnwindSafe(pc, fp, ctxPtr->pcs, DEFAULT_MAX_LOCAL_FRAME_NUM);
    ctxPtr->cv.notify_all();
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
    return;
#else

    std::unique_lock<std::mutex> lock(ctxPtr->mtx);
    if (ctxPtr->ctx == nullptr) {
        ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
        return;
    }
    ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
    if (memcpy_s(&ctxPtr->ctx->uc_mcontext, sizeof(ucontext->uc_mcontext),
        &ucontext->uc_mcontext, sizeof(ucontext->uc_mcontext)) != 0) {
        DFXLOGW("Failed to copy local ucontext with tid(%{public}d)", tid);
    }

    if (tid != getpid()) {
        if (!GetSelfStackRange(ctxPtr->stackBottom, ctxPtr->stackTop)) {
            DFXLOGW("Failed to get stack range with tid(%{public}d)", tid);
        }
    }

    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_READY);
    ctxPtr->cv.notify_all();
    ctxPtr->cv.wait_for(lock, g_timeOut);
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
#endif
}

bool LocalThreadContext::GetStackRange(int32_t tid, uintptr_t& stackBottom, uintptr_t& stackTop)
{
    auto ctxPtr = LocalThreadContext::GetInstance().GetThreadContext(tid);
    if (ctxPtr == nullptr) {
        return false;
    }
    stackBottom = ctxPtr->stackBottom;
    stackTop = ctxPtr->stackTop;
    return true;
}

void LocalThreadContext::InitSignalHandler()
{
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        FpUnwinder::GetPtr();
        struct sigaction action;
        (void)memset_s(&action, sizeof(action), 0, sizeof(action));
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGLOCAL_DUMP);
        action.sa_flags = SA_RESTART | SA_SIGINFO;
        action.sa_sigaction = LocalThreadContext::CopyContextAndWaitTimeout;
        DFXLOGU("Install local signal handler: %{public}d", SIGLOCAL_DUMP);
        sigaction(SIGLOCAL_DUMP, &action, nullptr);
    });
}

bool LocalThreadContext::SignalRequestThread(int32_t tid, ThreadContext* threadContext)
{
    siginfo_t si {0};
    si.si_signo = SIGLOCAL_DUMP;
    si.si_errno = 0;
    si.si_code = DUMP_TYPE_LOCAL;
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, si.si_signo, &si) != 0) {
        DFXLOGW("Failed to send signal(%{public}d) to tid(%{public}d), errno(%{public}d).", si.si_signo, tid, errno);
        threadContext->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
        return false;
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
