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
#include <fcntl.h>
#include <securec.h>
#include <sigchain.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "stack_util.h"
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
    std::unique_lock<std::mutex> lock(threadContext->mtx);
    if (threadContext->ctx == nullptr) {
        threadContext->ctx = new ucontext_t;
    }
    (void)memset_s(threadContext->ctx, sizeof(ucontext_t), 0, sizeof(ucontext_t));
}

void ReleaseContext(std::shared_ptr<ThreadContext> threadContext)
{
    std::unique_lock<std::mutex> lock(threadContext->mtx);
    if (threadContext->ctx != nullptr) {
        delete threadContext->ctx;
        threadContext->ctx = nullptr;
    }
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
    LOGE("GetContextLocked nullptr, tid: %d", tid);
    return nullptr;
}

AT_UNUSED bool RemoveContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end()) {
        LOGW("Context of tid(%d) is already removed.", tid);
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

    LOGW("Failed to release context of tid(%d), still using?", tid);
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

LocalThreadContext::~LocalThreadContext()
{
    if (pfd_[PIPE_READ] > 0) {
        syscall(SYS_close, pfd_[PIPE_READ]);
    }
    if (pfd_[PIPE_WRITE] > 0) {
        syscall(SYS_close, pfd_[PIPE_WRITE]);
    }
}

std::shared_ptr<ThreadContext> LocalThreadContext::GetThreadContext(int32_t tid)
{
    std::unique_lock<std::mutex> lock(localMutex_);
    auto it = g_contextMap.find(tid);
    if (it != g_contextMap.end()) {
        return it->second;
    }
    LOGW("Failed to get context of tid(%d)", tid);
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
        LOGW("Failed to get context of tid(%d), still using?", tid);
        return nullptr;
    }

    InitSignalHandler();
    if (!SignalRequestThread(tid, threadContext.get())) {
        return nullptr;
    }
    threadContext->cv.wait_for(lock, g_timeOut);
    return threadContext;
}

bool LocalThreadContext::CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context)
{
    if (si == nullptr || si->si_code != DUMP_TYPE_LOCAL || context == nullptr) {
        return false;
    }

    int tid = getproctid();
    LOGU("tid(%d) recv sig(%d)", tid, sig);
    auto ctxPtr = LocalThreadContext::GetInstance().GetThreadContext(tid);
#if defined(__aarch64__)
    if (ctxPtr == nullptr) {
        return true;
    }
    uintptr_t fp = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.regs[REG_FP];
    ctxPtr->pcs[0] = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.pc;
    size_t index = 1;
    while (index < DEFAULT_MAX_LOCAL_FRAME_NUM) {
        uintptr_t prevFp = fp;
        if (!LocalThreadContext::GetInstance().ReadUintptrSafe(prevFp, fp)) {
            break;
        }

        if (!LocalThreadContext::GetInstance().ReadUintptrSafe(prevFp + sizeof(uintptr_t), ctxPtr->pcs[index])) {
            break;
        }

        if (fp == prevFp || fp == 0 || ctxPtr->pcs[index] == 0) {
            break;
        }
        index++;
    }
    ctxPtr->frameSz = index;
    ctxPtr->cv.notify_all();
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
    return true;
#else

    std::unique_lock<std::mutex> lock(ctxPtr->mtx);
    if (ctxPtr->ctx == nullptr) {
        ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
        return true;
    }

    if (memcpy_s(ctxPtr->ctx, sizeof(ucontext_t), context, sizeof(ucontext_t)) != 0) {
        LOGW("Failed to copy local ucontext with tid(%d)", tid);
    }
    if (GetSelfStackRange(ctxPtr->stackBottom, ctxPtr->stackTop) != 0) {
        LOGW("Failed to get stack range with tid(%d)", tid);
    }

    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_READY);
    ctxPtr->cv.notify_all();
    ctxPtr->cv.wait_for(lock, g_timeOut);
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
    return true;
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

#if defined(__has_feature) && __has_feature(address_sanitizer)
__attribute__((no_sanitize("address"))) bool LocalThreadContext::ReadUintptrSafe(uintptr_t addr, uintptr_t& value)
#else
bool LocalThreadContext::ReadUintptrSafe(uintptr_t addr, uintptr_t& value)
#endif
{
    if ((pfd_[PIPE_READ] < 0) || (pfd_[PIPE_WRITE] < 0)) {
        return false;
    }

    if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_write, pfd_[PIPE_WRITE], addr, sizeof(uintptr_t))) == -1) {
        return false;
    }

    uintptr_t dummy = 0;
    value = *reinterpret_cast<uintptr_t *>(addr);
    if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_read, pfd_[PIPE_READ], &dummy, sizeof(uintptr_t))) == sizeof(uintptr_t)) {
        return true;
    }
    return false;
}

void LocalThreadContext::InitSignalHandler()
{
    static std::once_flag flag;
    std::call_once(flag, [&]() {
#if defined(__aarch64__)
        if (pipe2(pfd_, O_CLOEXEC | O_NONBLOCK) != 0) {
            LOGW("Failed to create pipe, errno(%d).", errno);
        }
#endif
        struct signal_chain_action sigchain = {
            .sca_sigaction = LocalThreadContext::CopyContextAndWaitTimeout,
            .sca_mask = {},
            .sca_flags = 0,
        };
        LOGU("Install local signal handler: %d", SIGLOCAL_DUMP);
        add_special_signal_handler(SIGLOCAL_DUMP, &sigchain);
    });
}

bool LocalThreadContext::SignalRequestThread(int32_t tid, ThreadContext* threadContext)
{
    siginfo_t si {0};
    si.si_signo = SIGLOCAL_DUMP;
    si.si_errno = 0;
    si.si_code = DUMP_TYPE_LOCAL;
#if defined(is_ohos) && is_ohos
    if (syscall(SYS_rt_tgsigqueueinfo, getprocpid(), tid, si.si_signo, &si) != 0) {
#else
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, si.si_signo, &si) != 0) {
#endif
        LOGW("Failed to send signal(%d) to tid(%d), errno(%d).", si.si_signo, tid, errno);
        threadContext->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
        return false;
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
