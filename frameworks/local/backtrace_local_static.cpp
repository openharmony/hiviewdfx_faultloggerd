/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "backtrace_local_static.h"

#include <cinttypes>
#include <condition_variable>
#include <csignal>
#include <map>
#include <mutex>

#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <securec.h>
#include <unistd.h>
#include "dfx_log.h"
#include "dfx_define.h"
#include "libunwind.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxBacktraceLocal"
static struct sigaction g_sigaction;
static std::mutex g_localMutex;
static std::map<int32_t, std::shared_ptr<ThreadContext>> g_contextMap;
static std::chrono::seconds TIME_OUT = std::chrono::seconds(2); // 2 : 2 seconds
static std::shared_ptr<ThreadContext> CreateContext(int32_t tid)
{
    auto threadContext = std::make_shared<ThreadContext>();
    threadContext->tid = tid;
    threadContext->ctx = new unw_context_t;
    (void)memset_s(threadContext->ctx, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    return threadContext;
}

static std::shared_ptr<ThreadContext> GetContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end()) {
        auto threadContext = CreateContext(tid);
        g_contextMap[tid] = threadContext;
        return threadContext;
    }

    if (it->second->tid == ThreadContextStatus::ContextUnused) {
        it->second->tid = tid;
        (void)memset_s(it->second->ctx, sizeof(unw_context_t), 0, sizeof(unw_context_t));
        return it->second;
    }

    return nullptr;
}

static bool RemoveContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end()) {
        DfxLogWarn("Context of %d is already removed.", tid);
        return true;
    }

    if (it->second->tid == ThreadContextStatus::ContextUnused) {
        g_contextMap.erase(it);
        return true;
    }

    DfxLogWarn("Failed to remove context of %d, still using?.", tid);
    return false;
}
}

BacktraceLocalStatic& BacktraceLocalStatic::GetInstance()
{
    static BacktraceLocalStatic instance;
    return instance;
}

std::shared_ptr<ThreadContext> BacktraceLocalStatic::GetThreadContext(int32_t tid)
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    auto context = GetContextLocked(tid);
    if (context == nullptr) {
        DfxLogWarn("Failed to get context of %d, still using?", tid);
        return nullptr;
    }

    if (!InstallSigHandler()) {
        RemoveContextLocked(tid);
        return nullptr;
    }

    if (!SignalRequestThread(tid, context.get())) {
        UninstallSigHandler();
        return nullptr;
    }
    UninstallSigHandler();
    return context;
}

void BacktraceLocalStatic::ReleaseThread(int32_t tid)
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end()) {
        return;
    }

    it->second->cv.notify_one();
}

void BacktraceLocalStatic::CleanUp()
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    auto it = g_contextMap.begin();
    while (it != g_contextMap.end()) {
        if (it->second == nullptr) {
            it = g_contextMap.erase(it);
        } else if (it->second->tid == ThreadContextStatus::ContextUnused) {
            it = g_contextMap.erase(it);
        } else {
            it++;
        }
    }
}

void BacktraceLocalStatic::CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context)
{
    auto ctxPtr = static_cast<ThreadContext*>(si->si_value.sival_ptr);
    if (ctxPtr == nullptr) {
        // should never happen
        return;
    }
    std::unique_lock<std::mutex> lock(ctxPtr->lock);
    int32_t tid = syscall(SYS_gettid);
    if (!ctxPtr->tid.compare_exchange_strong(tid,
        static_cast<int32_t>(ThreadContextStatus::ContextUsing))) {
        return;
    }
    if (ctxPtr->ctx != nullptr) {
#if defined(__arm__)
        ucontext_t *uc = static_cast<ucontext_t *>(context);
        ctxPtr->ctx->regs[UNW_ARM_R0] = uc->uc_mcontext.arm_r0;
        ctxPtr->ctx->regs[UNW_ARM_R1] = uc->uc_mcontext.arm_r1;
        ctxPtr->ctx->regs[UNW_ARM_R2] = uc->uc_mcontext.arm_r2;
        ctxPtr->ctx->regs[UNW_ARM_R3] = uc->uc_mcontext.arm_r3;
        ctxPtr->ctx->regs[UNW_ARM_R4] = uc->uc_mcontext.arm_r4;
        ctxPtr->ctx->regs[UNW_ARM_R5] = uc->uc_mcontext.arm_r5;
        ctxPtr->ctx->regs[UNW_ARM_R6] = uc->uc_mcontext.arm_r6;
        ctxPtr->ctx->regs[UNW_ARM_R7] = uc->uc_mcontext.arm_r7;
        ctxPtr->ctx->regs[UNW_ARM_R8] = uc->uc_mcontext.arm_r8;
        ctxPtr->ctx->regs[UNW_ARM_R9] = uc->uc_mcontext.arm_r9;
        ctxPtr->ctx->regs[UNW_ARM_R10] = uc->uc_mcontext.arm_r10;
        ctxPtr->ctx->regs[UNW_ARM_R11] = uc->uc_mcontext.arm_fp;
        ctxPtr->ctx->regs[UNW_ARM_R12] = uc->uc_mcontext.arm_ip;
        ctxPtr->ctx->regs[UNW_ARM_R13] = uc->uc_mcontext.arm_sp;
        ctxPtr->ctx->regs[UNW_ARM_R14] = uc->uc_mcontext.arm_lr;
        ctxPtr->ctx->regs[UNW_ARM_R15] = uc->uc_mcontext.arm_pc;
#else
        // the ucontext.uc_mcontext.__reserved of libunwind is simplified with the system's own in aarch64
        if (memcpy_s(ctxPtr->ctx, sizeof(unw_context_t), context, sizeof(unw_context_t)) != 0) {
            DfxLogWarn("Failed to copy local unwind context.");
        }
#endif
    } else {
        ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::ContextUnused);
        return;
    }
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::ContextReady);
    ctxPtr->cv.wait_for(lock, TIME_OUT);
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::ContextUnused);
}

bool BacktraceLocalStatic::InstallSigHandler()
{
    struct sigaction action;
    (void)memset_s(&action, sizeof(action), 0, sizeof(action));
    (void)memset_s(&g_sigaction, sizeof(g_sigaction), 0, sizeof(g_sigaction));
    sigfillset(&action.sa_mask);
    // do not block crash signals
    sigdelset(&action.sa_mask, SIGABRT);
    sigdelset(&action.sa_mask, SIGBUS);
    sigdelset(&action.sa_mask, SIGILL);
    sigdelset(&action.sa_mask, SIGSEGV);
    action.sa_sigaction = BacktraceLocalStatic::CopyContextAndWaitTimeout;
    action.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGLOCAL_DUMP, &action, &g_sigaction) != EOK) {
        DfxLogWarn("Failed to install SigHandler for local backtrace(%d).", errno);
        return false;
    }
    return true;
}

void BacktraceLocalStatic::UninstallSigHandler()
{
    if (g_sigaction.sa_sigaction == nullptr) {
        signal(SIGLOCAL_DUMP, SIG_DFL);
        return;
    }

    if (sigaction(SIGLOCAL_DUMP, &g_sigaction, nullptr) != EOK) {
        DfxLogWarn("UninstallSigHandler :: Failed to reset signal(%d).", errno);
        signal(SIGLOCAL_DUMP, SIG_DFL);
    }
}

bool BacktraceLocalStatic::SignalRequestThread(int32_t tid, ThreadContext* ctx)
{
    siginfo_t si {0};
    si.si_signo = SIGLOCAL_DUMP;
    si.si_value.sival_ptr = ctx; // pass context pointer by sival_ptr
    si.si_errno = 0;
    si.si_code = -SIGLOCAL_DUMP;
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, si.si_signo, &si) != 0) {
        DfxLogWarn("Failed to queue signal(%d) to %d, errno(%d).", si.si_signo, tid, errno);
        ctx->tid = static_cast<int32_t>(ThreadContextStatus::ContextUnused);
        return false;
    }

    int left = 10000; // 10000 : max wait 10ms for single thread
    constexpr int pollTime = 10; // 10 : 10us
    while (ctx->tid.load() != ThreadContextStatus::ContextReady) {
        int ret = usleep(pollTime);
        if (ret == 0) {
            left -= pollTime;
        } else {
            left -= ret;
        }

        if (left <= 0 &&
            ctx->tid.compare_exchange_strong(tid,
            static_cast<int32_t>(ThreadContextStatus::ContextUnused))) {
            DfxLogWarn("Failed to wait for %d to write context.", tid);
            return false;
        }
    }
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
