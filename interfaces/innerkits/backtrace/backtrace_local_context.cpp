/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#include "backtrace_local_context.h"

#include <cinttypes>
#include <condition_variable>
#include <csignal>
#include <map>
#include <mutex>

#include <fcntl.h>
#include <securec.h>
#include <sigchain.h>
#include <sys/syscall.h>
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
static std::chrono::seconds g_timeOut = std::chrono::seconds(1);
static std::shared_ptr<ThreadContext> CreateContext(int32_t tid)
{
    auto threadContext = std::make_shared<ThreadContext>();
    threadContext->tid = tid;
    std::unique_lock<std::mutex> mlock(threadContext->lock);
    threadContext->frameSz = 0;
    threadContext->ctx = new unw_context_t;
    (void)memset_s(threadContext->ctx, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    return threadContext;
}

static std::shared_ptr<ThreadContext> GetContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end() || it->second == nullptr) {
        auto threadContext = CreateContext(tid);
        g_contextMap[tid] = threadContext;
        return threadContext;
    }

    if (it->second->tid == ThreadContextStatus::CONTEXT_UNUSED) {
        it->second->tid = tid;
        it->second->frameSz = 0;
        std::unique_lock<std::mutex> mlock(it->second->lock);
        if (it->second->ctx == nullptr) {
            it->second->ctx = new unw_context_t;
        }
        (void)memset_s(it->second->ctx, sizeof(unw_context_t), 0, sizeof(unw_context_t));
        return it->second;
    }

    return nullptr;
}

static void ReleaseUnwindContext(std::shared_ptr<ThreadContext> context)
{
    std::unique_lock<std::mutex> mlock(context->lock);
    if (context->ctx != nullptr) {
        delete context->ctx;
        context->ctx = nullptr;
    }
}

static bool RemoveContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end()) {
        DFXLOG_WARN("Context of %d is already removed.", tid);
        return true;
    }
    if (it->second == nullptr) {
        g_contextMap.erase(it);
        return true;
    }

    // only release unw_context_t object
    if (it->second->tid == ThreadContextStatus::CONTEXT_UNUSED) {
        ReleaseUnwindContext(it->second);
        return true;
    }

    DFXLOG_WARN("Failed to release unwind context of %d, still using?.", tid);
    return false;
}
}

BacktraceLocalContext& BacktraceLocalContext::GetInstance()
{
    static BacktraceLocalContext instance;
    return instance;
}

std::shared_ptr<ThreadContext> BacktraceLocalContext::CollectThreadContext(int32_t tid)
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    auto context = GetContextLocked(tid);
    if (context == nullptr) {
        DFXLOG_WARN("Failed to get context of %d, still using?", tid);
        return nullptr;
    }

    if (!Init()) {
        RemoveContextLocked(tid);
        DFXLOG_WARN("%s", "Failed to install local dump signal handler.");
        return nullptr;
    }

    if (!SignalRequestThread(tid, context.get())) {
        return nullptr;
    }
    context->cv.wait_for(lock, g_timeOut);
    return context;
}

std::shared_ptr<ThreadContext> BacktraceLocalContext::GetThreadContext(int32_t tid)
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    auto it = g_contextMap.find(tid);
    if (it != g_contextMap.end()) {
        return it->second;
    }
    return nullptr;
}

void BacktraceLocalContext::ReleaseThread(int32_t tid)
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end() || it->second == nullptr) {
        return;
    }

    it->second->cv.notify_all();
}

void BacktraceLocalContext::CleanUp()
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    auto it = g_contextMap.begin();
    while (it != g_contextMap.end()) {
        if (it->second == nullptr) {
            it = g_contextMap.erase(it);
            continue;
        }
        if (it->second->tid == ThreadContextStatus::CONTEXT_UNUSED) {
            ReleaseUnwindContext(it->second);
        }
        it++;
    }
}

bool BacktraceLocalContext::CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context)
{
    if (si->si_code != DUMP_TYPE_LOCAL) {
        return false;
    }

    auto ctxPtr = BacktraceLocalContext::GetInstance().GetThreadContext(getproctid());
    if (ctxPtr == nullptr || context == nullptr) {
        return true;
    }

#if defined(__aarch64__)
    uintptr_t fp = static_cast<ucontext_t*>(context)->uc_mcontext.regs[UNW_AARCH64_X29];
    ctxPtr->pcs[0] = static_cast<ucontext_t*>(context)->uc_mcontext.pc;
    int index = 1;
    for (; index < DEFAULT_MAX_LOCAL_FRAME_NUM; index++) {
        uintptr_t prevFp = fp;
        if (!BacktraceLocalContext::GetInstance().ReadUintptrSafe(prevFp, fp)) {
            break;
        }

        if (!BacktraceLocalContext::GetInstance().ReadUintptrSafe(fp + sizeof(uintptr_t), ctxPtr->pcs[index])) {
            break;
        }

        if (fp == prevFp || fp == 0 || ctxPtr->pcs[index] == 0) {
            break;
        }
    }
    ctxPtr->frameSz = index;
    ctxPtr->cv.notify_all();
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
    return true;
#else
    std::unique_lock<std::mutex> lock(ctxPtr->lock);
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
            DFXLOG_WARN("%s", "Failed to copy local unwind context.");
        }
#endif
    } else {
        ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
        return true;
    }
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_READY);
    ctxPtr->cv.notify_all();
    ctxPtr->cv.wait_for(lock, g_timeOut);
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
    return true;
#endif
}

bool BacktraceLocalContext::SignalRequestThread(int32_t tid, ThreadContext* ctx)
{
    siginfo_t si {0};
    si.si_signo = SIGDUMP;
    si.si_errno = 0;
    si.si_code = DUMP_TYPE_LOCAL;
    if (syscall(SYS_rt_tgsigqueueinfo, getprocpid(), tid, si.si_signo, &si) != 0) {
        DFXLOG_WARN("Failed to queue signal(%d) to %d, errno(%d).", si.si_signo, tid, errno);
        ctx->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
        return false;
    }
    return true;
}

bool BacktraceLocalContext::Init()
{
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        if (pipe2(pipe2_, O_CLOEXEC | O_NONBLOCK) != 0) {
            DFXLOG_WARN("Failed to create pipe, errno(%d).", errno);
            init_ = false;
        } else {
            struct signal_chain_action sigchain = {
                .sca_sigaction = BacktraceLocalContext::CopyContextAndWaitTimeout,
                .sca_mask = {},
                .sca_flags = 0,
            };
            add_special_signal_handler(SIGDUMP, &sigchain);
            init_ = true;
        }
    });

    return init_;
}

#if defined(__has_feature) && __has_feature(address_sanitizer)
__attribute__((no_sanitize("address"))) bool BacktraceLocalContext::ReadUintptrSafe(uintptr_t addr, uintptr_t& value)
#else
bool BacktraceLocalContext::ReadUintptrSafe(uintptr_t addr, uintptr_t& value)
#endif
{
    if (!init_) {
        return false;
    }

    if (OHOS_TEMP_FAILURE_RETRY(syscall(SYS_write, pipe2_[PIPE_WRITE], addr, sizeof(uintptr_t))) == -1) {
        return false;
    }

    uintptr_t dummy;
    value = *reinterpret_cast<uintptr_t *>(addr);
    return OHOS_TEMP_FAILURE_RETRY(syscall(SYS_read, pipe2_[PIPE_READ], &dummy, sizeof(uintptr_t))) ==
        sizeof(uintptr_t);
}
} // namespace HiviewDFX
} // namespace OHOS
