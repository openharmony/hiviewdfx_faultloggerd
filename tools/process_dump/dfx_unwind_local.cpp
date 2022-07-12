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
#include <pthread.h>
#include <sched.h>
#include <securec.h>
#include <unistd.h>
#include <iostream>
#include <sstream>

#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <securec.h>

#include <libunwind.h>
#include <libunwind_i-ohos.h>

#include "file_ex.h"

#include "dfx_symbols_cache.h"

namespace OHOS {
namespace HiviewDFX {
static constexpr int SINGLE_THREAD_UNWIND_TIMEOUT = 100; // 100 millseconds

DfxUnwindLocal &DfxUnwindLocal::GetInstance()
{
    static DfxUnwindLocal ins;
    return ins;
}

DfxUnwindLocal::DfxUnwindLocal()
{
    as_ = nullptr;
    frames_.clear();
    curIndex_ = 0;
    memset(&oldSigaction_, 0, sizeof(struct sigaction));
    sigemptyset(&mask_);
}

bool DfxUnwindLocal::Init()
{
    unw_init_local_address_space(&as_);
    if (as_ == nullptr) {
        return false;
    }

    frames_ = std::vector<DfxFrame>(BACK_STACK_MAX_STEPS);
    InstallLocalDumper(SIGLOCAL_DUMP);
    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGABRT);
    sigdelset(&mask, SIGBUS);
    sigdelset(&mask, SIGILL);
    sigdelset(&mask, SIGSEGV);
    sigprocmask(SIG_SETMASK, &mask, &mask_);
    std::unique_ptr<DfxSymbolsCache> cache(new DfxSymbolsCache());
    cache_ = std::move(cache);
    return true;
}

void DfxUnwindLocal::Destroy()
{
    frames_.clear();
    frames_.shrink_to_fit();
    UninstallLocalDumper(SIGLOCAL_DUMP);
    sigprocmask(SIG_SETMASK, &mask_, nullptr);
    unw_destroy_local_address_space(as_);
    as_ = nullptr;
    cache_ = nullptr;
}

bool DfxUnwindLocal::SendLocalDumpRequest(int32_t tid)
{
    localDumpRequest_.tid = tid;
    localDumpRequest_.timeStamp = (uint64_t)time(NULL);
    return syscall(SYS_tkill, tid, SIGLOCAL_DUMP) == 0;
}

bool DfxUnwindLocal::WaitLocalDumpRequest()
{
    bool ret = true;
    std::unique_lock<std::mutex> lck(localDumperMutex_);
    if (localDumperCV_.wait_for(lck, \
        std::chrono::milliseconds(SINGLE_THREAD_UNWIND_TIMEOUT)) == std::cv_status::timeout) {
        // time out means we didn't got any back trace msg, just return false.
        ret = false;
    }
    return ret;
}

std::string DfxUnwindLocal::CollectUnwindResult(int32_t tid)
{
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

    for (uint32_t i = 0; i < curIndex_; ++i) {
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

bool DfxUnwindLocal::ExecLocalDumpUnwind(int tid, size_t skipFramNum)
{
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local_with_as(as_, &cursor, &context);

    size_t index = 0;
    curIndex_ = 0;
    unw_word_t pc = 0;
    unw_word_t prevPc = 0;
    while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS)) {
        if (tid != gettid()) {
            break;
        }
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFramNum) {
            index++;
            continue;
        }

        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) {
            DfxLogWarn("%s :: Failed to get current pc, stop.", __func__);
            break;
        }
        if (curIndex_ > 1 && prevPc == pc) {
            DfxLogWarn("%s :: repeated pc, stop.", __func__);
            break;
        }
        prevPc = pc;

        unw_word_t relPc = unw_get_rel_pc(&cursor);
        unw_word_t sz = unw_get_previous_instr_sz(&cursor);
        if (index - skipFramNum != 0) {
            relPc -= sz;
        }

        auto& curFrame = frames_[index - skipFramNum];
        struct map_info* map = unw_get_map(&cursor);
        errno_t err = EOK;
        bool isValidFrame = true;
        char mapName[SYMBOL_BUF_SIZE] = {0};
        if ((map != NULL) && (strlen(map->path) < SYMBOL_BUF_SIZE - 1)) {
            err = strcpy_s(mapName, SYMBOL_BUF_SIZE, map->path);
        } else {
            isValidFrame = false;
            err = strcpy_s(mapName, SYMBOL_BUF_SIZE, "Unknown");
        }
        if (err != EOK) {
            DfxLogError("%s :: strcpy_s failed.", __func__);
            return false;
        }

        curIndex_ = static_cast<uint32_t>(index - skipFramNum);
        curFrame.SetFrameIndex((size_t)curIndex_);
        curFrame.SetFramePc((uint64_t)pc);
        curFrame.SetFrameRelativePc((uint64_t)relPc);
        curFrame.SetFrameMapName(std::string(mapName));
        
        index++;
        if (!isValidFrame) {
            break;
        }
    }
    return true;
}

void DfxUnwindLocal::LocalDumperUnwind(int sig, siginfo_t *si, void *context)
{
    std::unique_lock<std::mutex> lck(localDumperMutex_);
    ExecLocalDumpUnwind(localDumpRequest_.tid, DUMP_CATCHER_NUMBER_TWO);
    if (localDumpRequest_.tid == gettid()) {
        localDumperCV_.notify_one();
    }
}

void DfxUnwindLocal::LocalDumpering(int sig, siginfo_t *si, void *context)
{
    DfxUnwindLocal::GetInstance().LocalDumperUnwind(sig, si, context);
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
        DfxLogToSocket("InstallLocalDumper :: Failed to register signal.");
    }
}

void DfxUnwindLocal::UninstallLocalDumper(int sig)
{
    if (oldSigaction_.sa_sigaction == nullptr) {
        signal(sig, SIG_DFL);
        return;
    }

    if (sigaction(sig, &oldSigaction_, NULL) != EOK) {
        DfxLogToSocket("UninstallLocalDumper :: Failed to reset signal.");
        signal(sig, SIG_DFL);
    }
}
} // namespace HiviewDFX
} // namespace OHOS