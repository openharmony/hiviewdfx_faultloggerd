/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextern-c-compat"
#endif

#include "dfx_unwind_local.h"

#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <libunwind.h>
#include <libunwind_i-ohos.h>
#include <pthread.h>
#include <sched.h>
#include <securec.h>
#include <sstream>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dfx_symbols_cache.h"
#include "file_ex.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
static constexpr int MIN_VALID_FRAME_COUNT = 3;
}
DfxUnwindLocal &DfxUnwindLocal::GetInstance()
{
    static DfxUnwindLocal ins;
    return ins;
}

DfxUnwindLocal::DfxUnwindLocal()
{
    as_ = nullptr;
    curIndex_ = 0;
    insideSignalHandler_ = false;
    sigemptyset(&mask_);
    (void)memset_s(&oldSigaction_, sizeof(oldSigaction_), 0, sizeof(oldSigaction_));
    (void)memset_s(&context_, sizeof(context_), 0, sizeof(context_));
}

bool DfxUnwindLocal::Init()
{
    std::unique_lock<std::mutex> lck(localDumperMutex_);
    initTimes_++;

    if (isInited_) {
        DfxLogError("local handler has been inited.");
        return isInited_;
    }

    unw_init_local_address_space(&as_);
    if (as_ == nullptr) {
        DfxLogError("Failed to init local address aspace.");
        return false;
    }

    (void)memset_s(&context_, sizeof(context_), 0, sizeof(context_));
    frames_ = std::vector<DfxFrame>(BACK_STACK_MAX_STEPS);
    InstallLocalDumper(SIGLOCAL_DUMP);
    sigemptyset(&mask_);
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGABRT);
    sigdelset(&mask, SIGBUS);
    sigdelset(&mask, SIGILL);
    sigdelset(&mask, SIGSEGV);
    sigprocmask(SIG_SETMASK, &mask, &mask_);
    std::unique_ptr<DfxSymbolsCache> cache(new DfxSymbolsCache());
    cache_ = std::move(cache);
    isInited_ = true;
    return isInited_;
}

void DfxUnwindLocal::Destroy()
{
    std::unique_lock<std::mutex> lck(localDumperMutex_);
    if (initTimes_ >= 0) {
        initTimes_--;
    } else {
        DfxLogError("%s :: Init must be called before Destroy.", __func__);
    }

    if (!isInited_) {
        return;
    }

    if (initTimes_ > 0) {
        return;
    }

    frames_.clear();
    frames_.shrink_to_fit();
    UninstallLocalDumper(SIGLOCAL_DUMP);
    sigprocmask(SIG_SETMASK, &mask_, nullptr);
    unw_destroy_local_address_space(as_);
    as_ = nullptr;
    cache_ = nullptr;
    isInited_ = false;
}

bool DfxUnwindLocal::HasInit()
{
    return isInited_;
}

bool DfxUnwindLocal::SendAndWaitRequest(int32_t tid)
{
    if (SendLocalDumpRequest(tid)) {
        return WaitLocalDumpRequest();
    }
    return false;
}

bool DfxUnwindLocal::SendLocalDumpRequest(int32_t tid)
{
    insideSignalHandler_ = false;
    return syscall(SYS_tkill, tid, SIGLOCAL_DUMP) == 0;
}

std::string DfxUnwindLocal::CollectUnwindResult(int32_t tid)
{
    if (tid < 0) {
        return std::string("");
    }

    std::ostringstream result;
    result << "Tid:" << tid;
    std::string path = "/proc/self/task/" + std::to_string(tid) + "/comm";
    std::string threadComm;
    if (OHOS::LoadStringFromFile(path, threadComm)) {
        result << " comm:" << threadComm;
    } else {
        result << std::endl;
    }

    if (curIndex_ == 0) {
        result << "Failed to get stacktrace." << std::endl;
        return result.str();
    }

    for (uint32_t i = 0; i < curIndex_; ++i) {
        ResolveFrameInfo(i, frames_[i]);
        result << frames_[i].PrintFrame();
    }

    result << std::endl;
    return result.str();
}

void DfxUnwindLocal::CollectUnwindFrames(std::vector<std::shared_ptr<DfxFrame>>& frames)
{
    if (curIndex_ == 0) {
        return;
    }

    for (uint32_t i = 0; i <= curIndex_; ++i) {
        ResolveFrameInfo(i, frames_[i]);
        frames.emplace_back(std::make_shared<DfxFrame>(frames_[i]));
    }
}

void DfxUnwindLocal::ResolveFrameInfo(size_t index, DfxFrame& frame)
{
    if (cache_ == nullptr) {
        return;
    }

    frame.SetFrameIndex(index);
    uint64_t pc = frame.GetFramePc();
    uint64_t funcOffset = frame.GetFrameFuncOffset();
    std::string funcName = frame.GetFrameFuncName();
    if (!cache_->GetNameAndOffsetByPc(as_, pc, funcName, funcOffset)) {
        frame.SetFrameFuncName("");
        frame.SetFrameFuncOffset(0);
    } else {
        frame.SetFramePc(pc);
        frame.SetFrameFuncName(funcName);
        frame.SetFrameFuncOffset(funcOffset);
    }
}

bool DfxUnwindLocal::ExecLocalDumpUnwindByWait()
{
    bool ret = false;
    if (!insideSignalHandler_) {
        DfxLogError("%s :: must be send request first.", __func__);
        return ret;
    }
    std::unique_lock<std::mutex> lck(localDumperMutex_);
    ret = ExecLocalDumpUnwinding(&context_, 0);
    localDumperCV_.notify_one();
    return ret;
}

bool DfxUnwindLocal::WaitLocalDumpRequest()
{
    int left = 1000; // 1000 : 1000us
    constexpr int pollTime = 10; // 10 : 10us
    while (!insideSignalHandler_) {
        int ret = usleep(pollTime);
        if (ret == 0) {
            left -= pollTime;
        } else {
            left -= ret;
        }
        if (left <= 0) {
            return false;
        }
    }
    return true;
}

bool DfxUnwindLocal::ExecLocalDumpUnwind(size_t skipFramNum)
{
    unw_context_t context;
    unw_getcontext(&context);
    return ExecLocalDumpUnwinding(&context, skipFramNum);
}

bool DfxUnwindLocal::ExecLocalDumpUnwinding(unw_context_t *ctx, size_t skipFramNum)
{
    unw_cursor_t cursor;
    unw_init_local_with_as(as_, &cursor, ctx);

    int ret = 0;
    size_t index = 0;
    curIndex_ = 0;
    unw_word_t pc = 0;
    unw_word_t sp = 0;
    unw_word_t prevPc = 0;
    char mapName[SYMBOL_BUF_SIZE] = {0};
    do {
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFramNum) {
            index++;
            continue;
        }

        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) {
            DfxLogWarn("%s :: Failed to get current pc, stop.", __func__);
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(sp)))) {
            DfxLogWarn("%s :: Failed to get current sp, stop.", __func__);
            break;
        }

        curIndex_ = static_cast<uint32_t>(index - skipFramNum);
        DfxLogDebug("%s :: curIndex_: %d", __func__, curIndex_);
        if (curIndex_ > 1 && prevPc == pc) {
            break;
        }
        prevPc = pc;

        unw_word_t relPc = unw_get_rel_pc(&cursor);
        unw_word_t sz = unw_get_previous_instr_sz(&cursor);
        if ((curIndex_ > 0) && (relPc > sz)) {
            relPc -= sz;
            pc -= sz;
#if defined(__arm__)
            unw_set_adjust_pc(&cursor, pc);
#endif
        }

        struct map_info* map = unw_get_map(&cursor);
        errno_t err = EOK;
        bool isValidFrame = true;
        (void)memset_s(mapName, SYMBOL_BUF_SIZE, 0, SYMBOL_BUF_SIZE);
        if ((map != NULL) && (strlen(map->path) < SYMBOL_BUF_SIZE - 1)) {
            err = strcpy_s(mapName, SYMBOL_BUF_SIZE, map->path);
        } else {
            isValidFrame = false;
            err = strcpy_s(mapName, SYMBOL_BUF_SIZE, "Unknown");
        }
        if (err != EOK) {
            DfxLogError("%s :: strcpy_s failed.", __func__);
            break;
        }

        if (curIndex_ < MIN_VALID_FRAME_COUNT || isValidFrame) {
            auto& curFrame = frames_[curIndex_];
            curFrame.SetFrameIndex((size_t)curIndex_);
            curFrame.SetFramePc((uint64_t)pc);
            curFrame.SetFrameSp((uint64_t)sp);
            curFrame.SetFrameRelativePc((uint64_t)relPc);
            curFrame.SetFrameMapName(std::string(mapName));
        } else {
            curIndex_--;
            DfxLogError("%s :: unw_get_map failed.", __func__);
            break;
        }

        index++;
    } while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS));
    return true;
}

void DfxUnwindLocal::LocalDumper(int sig, siginfo_t *si, void *context)
{
    std::unique_lock<std::mutex> lck(localDumperMutex_);
    insideSignalHandler_ = true;
#if defined(__arm__)
    (void)memset_s(&context_, sizeof(context_), 0, sizeof(context_));
    ucontext_t *uc = (ucontext_t *)context;
    context_.regs[UNW_ARM_R0] = uc->uc_mcontext.arm_r0;
    context_.regs[UNW_ARM_R1] = uc->uc_mcontext.arm_r1;
    context_.regs[UNW_ARM_R2] = uc->uc_mcontext.arm_r2;
    context_.regs[UNW_ARM_R3] = uc->uc_mcontext.arm_r3;
    context_.regs[UNW_ARM_R4] = uc->uc_mcontext.arm_r4;
    context_.regs[UNW_ARM_R5] = uc->uc_mcontext.arm_r5;
    context_.regs[UNW_ARM_R6] = uc->uc_mcontext.arm_r6;
    context_.regs[UNW_ARM_R7] = uc->uc_mcontext.arm_r7;
    context_.regs[UNW_ARM_R8] = uc->uc_mcontext.arm_r8;
    context_.regs[UNW_ARM_R9] = uc->uc_mcontext.arm_r9;
    context_.regs[UNW_ARM_R10] = uc->uc_mcontext.arm_r10;
    context_.regs[UNW_ARM_R11] = uc->uc_mcontext.arm_fp;
    context_.regs[UNW_ARM_R12] = uc->uc_mcontext.arm_ip;
    context_.regs[UNW_ARM_R13] = uc->uc_mcontext.arm_sp;
    context_.regs[UNW_ARM_R14] = uc->uc_mcontext.arm_lr;
    context_.regs[UNW_ARM_R15] = uc->uc_mcontext.arm_pc;
#else
    // the ucontext.uc_mcontext.__reserved of libunwind is simplified with the system's own in aarch64
    if (memcpy_s(&context_, sizeof(unw_context_t), context, sizeof(unw_context_t)) != 0) {
        DfxLogToSocket("Failed to copy local unwind context.");
    }
#endif

    localDumperCV_.wait_for(lck, std::chrono::milliseconds(2000)); // 2000 : 2000ms
}

void DfxUnwindLocal::LocalDumpering(int sig, siginfo_t *si, void *context)
{
    DfxUnwindLocal::GetInstance().LocalDumper(sig, si, context);
}

void DfxUnwindLocal::InstallLocalDumper(int sig)
{
    struct sigaction action;
    memset_s(&action, sizeof(action), 0, sizeof(action));
    memset_s(&oldSigaction_, sizeof(oldSigaction_), 0, sizeof(oldSigaction_));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DfxUnwindLocal::LocalDumpering;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(sig, &action, &oldSigaction_) != EOK) {
        DfxLogWarn("InstallLocalDumper :: Failed to register signal.");
    }
}

void DfxUnwindLocal::UninstallLocalDumper(int sig)
{
    if (oldSigaction_.sa_sigaction == nullptr) {
        signal(sig, SIG_DFL);
        return;
    }

    if (sigaction(sig, &oldSigaction_, NULL) != EOK) {
        DfxLogWarn("UninstallLocalDumper :: Failed to reset signal.");
        signal(sig, SIG_DFL);
    }
}
} // namespace HiviewDFX
} // namespace OHOS
