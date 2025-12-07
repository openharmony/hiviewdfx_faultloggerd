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
#include "dfx_elf.h"
#ifndef is_ohos_lite
#include "file_ex.h"
#else
#include "file_util.h"
#endif
#include "fp_unwinder.h"
#include "string_printf.h"
#if defined(__aarch64__)
#include "unwind_arm64_define.h"
#endif
#include "safe_reader.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxThreadContext"

#if defined(__aarch64__) || defined(__loongarch_lp64)
std::mutex g_localMutex;
std::map<int32_t, std::shared_ptr<ThreadContext>> g_contextMap {};
#endif
constexpr std::chrono::seconds TIME_OUT = std::chrono::seconds(1);

void PrintThreadStatus(int32_t tid)
{
    std::string content;
    std::string path = StringPrintf("/proc/%d/status", tid);
    LoadStringFromFile(path, content);
    DFXLOGI("%{public}s", content.c_str());
}

#if defined(__aarch64__) || defined(__loongarch_lp64)
std::shared_ptr<ThreadContext> GetContextLocked(int32_t tid)
{
    auto it = g_contextMap.find(tid);
    if (it == g_contextMap.end() || it->second == nullptr) {
        auto threadContext = std::make_shared<ThreadContext>();
        threadContext->tid = tid;
        threadContext->frameSz = 0;
        g_contextMap[tid] = threadContext;
        return threadContext;
    }

    if (it->second->tid == ThreadContextStatus::CONTEXT_UNUSED) {
        it->second->tid = tid;
        it->second->frameSz = 0;
        return it->second;
    }
    DFXLOGE("GetContextLocked nullptr, tid: %{public}d", tid);
    return nullptr;
}

bool RemoveAllContextLocked()
{
    auto it = g_contextMap.begin();
    while (it != g_contextMap.end()) {
        if (it->second == nullptr) {
            it = g_contextMap.erase(it);
            continue;
        }
        it++;
    }
    return true;
}
#endif

NO_SANITIZE void CopyContextAndWaitTimeoutMix(int sig, siginfo_t *si, void *context)
{
    if (si == nullptr || context == nullptr) {
        return;
    }
    auto& instance = LocalThreadContextMix::GetInstance();
    if (!instance.CheckStatusValidate(SyncStatus::WAIT_CTX, gettid())) {
        return;
    }
    if ((gettid() != getpid()) && !instance.GetSelfStackRangeInSignal()) {
        return;
    }
    instance.CopyRegister(context);
    instance.CopyStackBuf();
}

#if defined(__aarch64__) || defined(__loongarch_lp64)
NO_SANITIZE void CopyContextAndWaitTimeout(int sig, siginfo_t *si, void *context)
{
    if (si == nullptr || si->si_value.sival_ptr == nullptr || context == nullptr) {
        return;
    }

    DFXLOGU("tid(%{public}d) recv sig(%{public}d)", gettid(), sig);
    auto ctxPtr = static_cast<ThreadContext *>(si->si_value.sival_ptr);
    uintptr_t fp = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.regs[REG_FP];
    uintptr_t pc = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.pc;
    ctxPtr->firstFrameSp = reinterpret_cast<ucontext_t*>(context)->uc_mcontext.sp;
    ctxPtr->frameSz = FpUnwinder::GetPtr()->UnwindSafe(pc, fp, ctxPtr->pcs, DEFAULT_MAX_LOCAL_FRAME_NUM);
    ctxPtr->cv.notify_all();
    ctxPtr->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
    return;
}
#endif

void DfxBacktraceLocalSignalHandler(int sig, siginfo_t *si, void *context)
{
    if (si == nullptr) {
        return;
    }
    int savedErrno = errno;
    switch (si->si_code) {
#if defined(__aarch64__) || defined(__loongarch_lp64)
        case DUMP_TYPE_LOCAL:
            CopyContextAndWaitTimeout(sig, si, context);
            break;
#endif
        case DUMP_TYPE_LOCAL_MIX:
            CopyContextAndWaitTimeoutMix(sig, si, context);
            break;
        default:
            break;
    }
    errno = savedErrno;
}

void InitSignalHandler()
{
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        FpUnwinder::GetPtr();
        struct sigaction action;
        (void)memset_s(&action, sizeof(action), 0, sizeof(action));
        sigemptyset(&action.sa_mask);
        sigaddset(&action.sa_mask, SIGLOCAL_DUMP);
        action.sa_flags = SA_RESTART | SA_SIGINFO;
        action.sa_sigaction = DfxBacktraceLocalSignalHandler;
        DFXLOGU("Install local signal handler: %{public}d", SIGLOCAL_DUMP);
        sigaction(SIGLOCAL_DUMP, &action, nullptr);
    });
}
}

#if defined(__aarch64__) || defined(__loongarch_lp64)
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
    if (threadContext->cv.wait_for(lock, TIME_OUT) == std::cv_status::timeout) {
        DFXLOGE("wait_for timeout. tid = %{public}d", tid);
        PrintThreadStatus(tid);
        return nullptr;
    }
    return threadContext;
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

bool LocalThreadContext::SignalRequestThread(int32_t tid, ThreadContext* threadContext)
{
#if defined(__aarch64__) || defined(__loongarch_lp64)
    siginfo_t si {0};
    si.si_signo = SIGLOCAL_DUMP;
    si.si_errno = 0;
    si.si_code = DUMP_TYPE_LOCAL;
    si.si_value.sival_ptr = reinterpret_cast<void *>(threadContext);
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, si.si_signo, &si) != 0) {
        DFXLOGW("Failed to send signal(%{public}d) to tid(%{public}d), errno(%{public}d).", si.si_signo, tid, errno);
        threadContext->tid = static_cast<int32_t>(ThreadContextStatus::CONTEXT_UNUSED);
        return false;
    }
    return true;
#else
    return false;
#endif
}
#endif

NO_SANITIZE LocalThreadContextMix& LocalThreadContextMix::GetInstance()
{
    static LocalThreadContextMix instance;
    return instance;
}

bool LocalThreadContextMix::CollectThreadContext(int32_t tid)
{
    maps_ = DfxMaps::Create();
    StartCollectThreadContext(tid);
    InitSignalHandler();
    if (!SignalRequestThread(tid)) {
        return false;
    }
    {
        std::unique_lock<std::mutex> lock(mtx_);
        if (cv_.wait_for(lock, TIME_OUT) == std::cv_status::timeout) {
            DFXLOGE("wait_for timeout. tid = %{public}d, status = %{public}d", tid, status_);
            PrintThreadStatus(tid);
            return false;
        }
    }
    return CheckStatusValidate(SyncStatus::COPY_SUCCESS, tid);
}

bool LocalThreadContextMix::SignalRequestThread(int32_t tid)
{
    siginfo_t si {0};
    si.si_signo = SIGLOCAL_DUMP;
    si.si_errno = 0;
    si.si_code = DUMP_TYPE_LOCAL_MIX;
    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), tid, si.si_signo, &si) != 0) {
        return false;
    }
    return true;
}

void LocalThreadContextMix::StartCollectThreadContext(int32_t tid)
{
    std::unique_lock<std::mutex> lock(mtx_);
    tid_ = tid;
    status_ = SyncStatus::WAIT_CTX;
    stackBuf_.resize(STACK_BUFFER_SIZE, 0);
}

void LocalThreadContextMix::ReleaseCollectThreadContext()
{
    std::unique_lock<std::mutex> lock(mtx_);
    pc_ = 0;
    sp_ = 0;
    fp_ = 0;
    lr_ = 0;
    stackBottom_ = 0;
    stackTop_ = 0;
    stackBuf_ = {};
    tid_ = -1;
    status_ = SyncStatus::INIT;
    maps_ = nullptr;
}

NO_SANITIZE bool LocalThreadContextMix::CheckStatusValidate(int status, int32_t tid)
{
    std::unique_lock<std::mutex> lock(mtx_);
    return status_ == status && tid_ == tid;
}

NO_SANITIZE bool LocalThreadContextMix::GetSelfStackRangeInSignal()
{
    std::unique_lock<std::mutex> lock(mtx_);
    return StackUtils::GetSelfStackRange(stackBottom_, stackTop_);
}

NO_SANITIZE void LocalThreadContextMix::CopyRegister(void *context)
{
    std::unique_lock<std::mutex> lock(mtx_);
#if defined(__arm__)
    fp_ = static_cast<ucontext_t*>(context)->uc_mcontext.arm_fp;
    lr_ = static_cast<ucontext_t*>(context)->uc_mcontext.arm_lr;
    sp_ = static_cast<ucontext_t*>(context)->uc_mcontext.arm_sp;
    pc_ = static_cast<ucontext_t*>(context)->uc_mcontext.arm_pc;
#elif defined(__aarch64__)
    fp_ = static_cast<ucontext_t*>(context)->uc_mcontext.regs[RegsEnumArm64::REG_FP];
    lr_ = static_cast<ucontext_t*>(context)->uc_mcontext.regs[RegsEnumArm64::REG_LR];
    sp_ = static_cast<ucontext_t*>(context)->uc_mcontext.sp;
    pc_ = static_cast<ucontext_t*>(context)->uc_mcontext.pc;
#endif
}

NO_SANITIZE void LocalThreadContextMix::CopyStackBuf()
{
    if (stackTop_ <= sp_) {
        return;
    }
    uintptr_t curStackSz = stackTop_ - sp_;
    size_t cpySz = std::min(static_cast<size_t>(curStackSz), static_cast<size_t>(STACK_BUFFER_SIZE));
    std::unique_lock<std::mutex> lock(mtx_);

    SafeReader reader;
    if (reader.CopyReadableBufSafe(reinterpret_cast<uintptr_t>(stackBuf_.data()), stackBuf_.size(), sp_, cpySz) > 0) {
        status_ = SyncStatus::COPY_SUCCESS;
    } else {
        status_ = SyncStatus::COPY_FAILED;
    }
    cv_.notify_all();
}

void LocalThreadContextMix::SetRegister(std::shared_ptr<DfxRegs> regs)
{
#if defined(__arm__) || defined(__aarch64__)
    std::unique_lock<std::mutex> lock(mtx_);
    regs->SetSp(sp_);
    regs->SetPc(pc_);
    regs->SetFp(fp_);
    regs->SetReg(REG_LR, &(lr_));
#endif
}

void LocalThreadContextMix::SetStackRang(uintptr_t stackTop, uintptr_t stackBottom)
{
    std::unique_lock<std::mutex> lock(mtx_);
    stackTop_ = stackTop;
    stackBottom_ = stackBottom;
}

int LocalThreadContextMix::GetMapByPc(uintptr_t pc, std::shared_ptr<DfxMap>& map) const
{
    if (maps_ == nullptr) {
        DFXLOGE("maps_ is nullptr.");
        return -1;
    }
    return maps_->FindMapByAddr(pc, map) ? 0 : -1;
}

int LocalThreadContextMix::FindUnwindTable(uintptr_t pc, UnwindTableInfo& outTableInfo) const
{
    std::shared_ptr<DfxMap> dfxMap;
    if (maps_->FindMapByAddr(pc, dfxMap)) {
        if (dfxMap == nullptr) {
            return -1;
        }
        auto elf = dfxMap->GetElf(getpid());
        if (elf != nullptr) {
            return elf->FindUnwindTableInfo(pc, dfxMap, outTableInfo);
        }
    }
    return -1;
}

int LocalThreadContextMix::AccessMem(uintptr_t addr, uintptr_t *val)
{
    if (val == nullptr) {
        return -1;
    }
    *val = 0;
    uintptr_t result;
    if (__builtin_add_overflow(addr, sizeof(uintptr_t), &result)) {
        DFXLOGE("Failed to access addr, the addr is invalid");
        return -1;
    }
    if (addr < sp_ - stackForward_ || result > sp_ + STACK_BUFFER_SIZE) {
        std::shared_ptr<DfxMap> map;
        if (!(maps_->FindMapByAddr(addr, map)) || map == nullptr) {
            return -1;
        }
        auto elf = map->GetElf(getpid());
        if (elf != nullptr) {
            uint64_t foff = addr - map->begin + map->offset - elf->GetBaseOffset();
            if (elf->Read(foff, val, sizeof(uintptr_t))) {
                return 0;
            }
        }
        return -1;
    }
    size_t stackOffset = addr - (sp_ - stackForward_);
    if (stackOffset > stackBuf_.size() - sizeof(uintptr_t)) {
        DFXLOGE("Failed to access addr, the stackOffset is invalid");
        return -1;
    }
    *val = *(reinterpret_cast<uintptr_t *>(&stackBuf_[stackOffset]));
    return 0;
}

std::shared_ptr<DfxMaps> LocalThreadContextMix::GetMaps() const
{
    return maps_;
}

namespace OtherThread {
int GetMapByPc(uintptr_t pc, std::shared_ptr<DfxMap>& map, void *arg)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    return instance.GetMapByPc(pc, map);
}

int FindUnwindTable(uintptr_t pc, UnwindTableInfo& outTableInfo, void *arg)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    return instance.FindUnwindTable(pc, outTableInfo);
}

int AccessMem(uintptr_t addr, uintptr_t *val, void *arg)
{
    auto& instance = LocalThreadContextMix::GetInstance();
    return instance.AccessMem(addr, val);
}
}

std::shared_ptr<UnwindAccessors> LocalThreadContextMix::CreateAccessors()
{
    std::shared_ptr<UnwindAccessors> accssors = std::make_shared<UnwindAccessors>();
    accssors->AccessReg = nullptr;
    accssors->AccessMem = &OtherThread::AccessMem;
    accssors->GetMapByPc = &OtherThread::GetMapByPc;
    accssors->FindUnwindTable = &OtherThread::FindUnwindTable;
    return accssors;
}
} // namespace HiviewDFX
} // namespace OHOS
