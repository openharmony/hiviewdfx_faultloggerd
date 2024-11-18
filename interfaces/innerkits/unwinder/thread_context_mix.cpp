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

#include "thread_context_mix.h"

#include <chrono>
#include <csignal>
#include <map>
#include <memory>
#include <mutex>
#include <securec.h>
#include <sigchain.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "dfx_define.h"
#include "stack_util.h"
#if defined(__arm__)
#include "unwind_arm_define.h"
#elif defined(__aarch64__)
#include "unwind_arm64_define.h"
#else
#include "unwind_x86_64_define.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxThreadContext"

const std::chrono::seconds g_timeOut = std::chrono::seconds(1);
}

LocalThreadContextSupportMix& LocalThreadContextSupportMix::GetInstance()
{
    static LocalThreadContextSupportMix instance;
    return instance;
}

bool LocalThreadContextSupportMix::CollectThreadContext(int32_t tid,
    std::shared_ptr<ThreadContextSupportMix> ctx)
{
    std::unique_lock<std::mutex> lock(localMutex_);
    auto& instance = LocalThreadContextSupportMix::GetInstance();
    instance.SetContext(ctx);
    instance.SetTid(tid);
    instance.SetStatus(SyncStatus::WAIT_CTX);
    InitSignalHandler();
    if (!SignalRequestThread(tid)) {
        return false;
    }

    ctx->cv.wait_for(lock, g_timeOut);
    if (instance.GetTid() == tid &&
        instance.GetStatus() == SyncStatus::COPY_SUCCESS) {
        return true;
    } else {
        return false;
    }
}

NO_SANITIZE void LocalThreadContextSupportMix::CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context)
{
    auto& instance = LocalThreadContextSupportMix::GetInstance();
    if (instance.GetStatus() != SyncStatus::WAIT_CTX ||
        instance.GetTid() != gettid())
    {
        return;
    }
    if (si == nullptr || si->si_code != DUMP_TYPE_LOCAL || context == nullptr) {
        return;
    }

    auto ctx = instance.GetContext();
    if (gettid() != getpid()) {
        if (!GetSelfStackRange(ctx->stackBottom, ctx->stackTop)) {
            DFXLOGE("GetSelfStackRange faile.");
        }
    }
    // copy fp、lr、sp、pc
#if defined(__arm__)
    ctx->fp = static_cast<ucontext_t*>(context)->uc_mcontext.arm_fp;
    ctx->lr = static_cast<ucontext_t*>(context)->uc_mcontext.arm_lr;
    ctx->sp = static_cast<ucontext_t*>(context)->uc_mcontext.arm_sp;
    ctx->pc = static_cast<ucontext_t*>(context)->uc_mcontext.arm_pc;
#elif defined(__aarch64__)
    ctx->fp = static_cast<ucontext_t*>(context)->uc_mcontext.regs[RegsEnumArm64::REG_FP];
    ctx->lr = static_cast<ucontext_t*>(context)->uc_mcontext.regs[RegsEnumArm64::REG_LR];
    ctx->sp = static_cast<ucontext_t*>(context)->uc_mcontext.sp;
    ctx->pc = static_cast<ucontext_t*>(context)->uc_mcontext.pc;
#else
    ctx->fp = static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RBP];
    ctx->lr = static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP];
    ctx->sp = static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RSP];
    ctx->pc = static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP];
#endif

    // copy stack
    uintptr_t curStackSz = ctx->stackTop - ctx->sp;
    uintptr_t cpySz = fmin(curStackSz, STACK_BUFFER_SIZE);
    if (memcpy_s(reinterpret_cast<void *>(ctx->buffer), STACK_BUFFER_SIZE, reinterpret_cast<void *>(ctx->sp), cpySz) != 0) {
        DFXLOGW("Failed to copy stack buff with tid(%{public}d)", gettid() );
    }
    std::unique_lock<std::mutex> lock(ctx->mtx);
    instance.SetStatus(SyncStatus::COPY_SUCCESS);
    ctx->cv.notify_all();
}

void LocalThreadContextSupportMix::InitSignalHandler()
{
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        struct sigaction action;
        (void)memset_s(&action, sizeof(action), 0, sizeof(action));
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGLOCAL_DUMP);
        action.sa_flags = SA_RESTART | SA_SIGINFO;
        action.sa_sigaction = LocalThreadContextSupportMix::CopyContextAndWaitTimeout;
        DFXLOGU("Install local signal handler: %{public}d", SIGLOCAL_DUMP);
        sigaction(SIGLOCAL_DUMP, &action, nullptr);
    });
}

bool LocalThreadContextSupportMix::SignalRequestThread(int32_t tid)
{
    siginfo_t si {0};
    si.si_signo = SIGLOCAL_DUMP;
    si.si_errno = 0;
    si.si_code = DUMP_TYPE_LOCAL;
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, si.si_signo, &si) != 0) {
        return false;
    }
    return true;
}

void LocalThreadContextSupportMix::SetContext(std::shared_ptr<ThreadContextSupportMix> ctx)
{
    context_ = ctx;
}

std::shared_ptr<ThreadContextSupportMix> LocalThreadContextSupportMix::GetContext()
{
    return context_;
}

void LocalThreadContextSupportMix::SetTid(int32_t tid)
{
    tid_ = tid;
}

int32_t LocalThreadContextSupportMix::GetTid()
{
    return tid_;
}

void LocalThreadContextSupportMix::SetStatus(int status)
{
    status_ = status;
}

int LocalThreadContextSupportMix::GetStatus()
{
    return status_;
}
} // namespace HiviewDFX
} // namespace OHOS
