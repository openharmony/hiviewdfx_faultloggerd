/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <mutex>

#include <sys/syscall.h>      /* Definition of SYS_* constants */
#include <unistd.h>

#include <libunwind.h>
#include <securec.h>

#include "dfx_define.h"
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxBacktraceLocal"
constexpr int32_t timeout = 2000;
static std::atomic<int32_t> g_tid = -1;
static std::condition_variable g_localCv;
static std::mutex g_localMutex;
static struct sigaction g_sigaction;
}
BacktraceLocalStatic& BacktraceLocalStatic::GetInstance()
{
    static BacktraceLocalStatic instance;
    return instance;
}

BacktraceLocalStatic::BacktraceLocalStatic()
{
}

bool BacktraceLocalStatic::GetThreadContext(int32_t tid, unw_context_t& ctx)
{
    if (g_tid != -1) {
        DfxLogWarn("Failed to getcontext for %d, %d is unwind now.", tid, g_tid.load());
        return false;
    }

    std::unique_lock<std::mutex> lock(g_localMutex);
    if (!InstallSigHandler()) {
        DfxLogWarn("Failed to install local handler for %d.", tid);
        return false;
    }

    if (!SignalRequestThread(tid, ctx)) {
        DfxLogWarn("Failed to signal target thread:%d.", tid);
        return false;
    }

    g_localCv.wait_for(lock, std::chrono::milliseconds(timeout));
    UninstallSigHandler();
    return true;
}

void BacktraceLocalStatic::ReleaseThread(int32_t tid)
{
    if (tid != g_tid) {
        return;
    }

    g_localCv.notify_one();
    g_tid = -1;
}

void BacktraceLocalStatic::CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context)
{
    std::unique_lock<std::mutex> lock(g_localMutex);
    g_localCv.notify_one();

    unw_context_t* ctxPtr = (unw_context_t*)(si->si_value.sival_ptr);
    if (ctxPtr == nullptr) {
        return;
    }

#if defined(__arm__)
    ucontext_t *uc = (ucontext_t *)context;
    ctxPtr->regs[UNW_ARM_R0] = uc->uc_mcontext.arm_r0;
    ctxPtr->regs[UNW_ARM_R1] = uc->uc_mcontext.arm_r1;
    ctxPtr->regs[UNW_ARM_R2] = uc->uc_mcontext.arm_r2;
    ctxPtr->regs[UNW_ARM_R3] = uc->uc_mcontext.arm_r3;
    ctxPtr->regs[UNW_ARM_R4] = uc->uc_mcontext.arm_r4;
    ctxPtr->regs[UNW_ARM_R5] = uc->uc_mcontext.arm_r5;
    ctxPtr->regs[UNW_ARM_R6] = uc->uc_mcontext.arm_r6;
    ctxPtr->regs[UNW_ARM_R7] = uc->uc_mcontext.arm_r7;
    ctxPtr->regs[UNW_ARM_R8] = uc->uc_mcontext.arm_r8;   
    ctxPtr->regs[UNW_ARM_R9] = uc->uc_mcontext.arm_r9;
    ctxPtr->regs[UNW_ARM_R10] = uc->uc_mcontext.arm_r10;
    ctxPtr->regs[UNW_ARM_R11] = uc->uc_mcontext.arm_fp;
    ctxPtr->regs[UNW_ARM_R12] = uc->uc_mcontext.arm_ip;
    ctxPtr->regs[UNW_ARM_R13] = uc->uc_mcontext.arm_sp;
    ctxPtr->regs[UNW_ARM_R14] = uc->uc_mcontext.arm_lr;
    ctxPtr->regs[UNW_ARM_R15] = uc->uc_mcontext.arm_pc;
#else
    // the ucontext.uc_mcontext.__reserved of libunwind is simplified with the system's own in aarch64
    if (memcpy_s(ctxPtr, sizeof(unw_context_t), context, sizeof(unw_context_t)) != 0) {
        DfxLogWarn("Failed to copy local unwind context.");
    }
#endif
    g_localCv.wait_for(lock, std::chrono::milliseconds(timeout));
    // if timeout or force release, reset g_tid
    g_tid = -1;
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
        DfxLogWarn("InstallSigHandler for local backtrace(%d).", errno);
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

bool BacktraceLocalStatic::SignalRequestThread(int32_t tid, unw_context_t& ctx)
{
    siginfo_t si {0};
    si.si_signo = SIGLOCAL_DUMP;
    si.si_value.sival_ptr = &ctx;
    si.si_errno = 0;
    si.si_code = -SIGLOCAL_DUMP;
    g_tid = tid;
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, si.si_signo, &si) != 0) {
        DfxLogError("Failed to queue signal(%d) to %d, errno(%d).", si.si_signo, tid, errno);
        g_tid = -1;
        return false;
    }

    return true;
}
} // namespace HiviewDFX
} // namespace OHOS
