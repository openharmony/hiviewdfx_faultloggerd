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

#include "dfx_dump_catcher_local_dumper.h"

#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <securec.h>

#include <hilog_base/log_base.h>
#include <libunwind.h>

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxDumpCatcherLocalDumper"
#endif

#define SECONDS_TO_MILLSECONDS 1000000
#define NANOSECONDS_TO_MILLSECONDS 1000
#ifndef NSIG
#define NSIG 64
#endif

#define NUMBER_SIXTYFOUR 64
#define INHERITABLE_OFFSET 32

#ifndef LOCAL_DUMPER_DEBUG
#define LOCAL_DUMPER_DEBUG
#endif
#undef LOCAL_DUMPER_DEBUG

namespace OHOS {
namespace HiviewDFX {
static const int SYMBOL_BUF_SIZE = 4096;

static struct LocalDumperRequest g_localDumpRequest;
static pthread_mutex_t g_localDumperMutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_localDumperHasInit = false;
static struct sigaction g_localDumperOldSigactionList[NSIG] = {};

char* DfxDumpCatcherLocalDumper::g_StackInfo_ = nullptr;
long long DfxDumpCatcherLocalDumper::g_CurrentPosition = 0;

DfxDumpCatcherLocalDumper::DfxDumpCatcherLocalDumper()
{
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: construct.", LOG_TAG);
#endif
}

DfxDumpCatcherLocalDumper::~DfxDumpCatcherLocalDumper()
{
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: destructor.", LOG_TAG);
#endif
}

long DfxDumpCatcherLocalDumper::WriteDumpInfo(long current_position, size_t index, std::shared_ptr<DfxFrames> frame)
{
    int writeNumber = 0;
    char* current_pos = g_StackInfo_ + current_position;
    if (frame->GetFrameFuncName() == "") {
        writeNumber = sprintf_s(current_pos, BACK_STACK_INFO_SIZE-current_position,
            "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s\n",
             index, frame->GetFrameRelativePc(), frame->GetFramePc(), (frame->GetFrameMap() == nullptr) ?
             "Unknown" : frame->GetFrameMap()->GetMapPath().c_str());
    } else {
        writeNumber = sprintf_s(current_pos, BACK_STACK_INFO_SIZE-current_position,
             "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s(%s+%" PRIu64 ")\n", index,
             frame->GetFrameRelativePc(), frame->GetFramePc(), (frame->GetFrameMap() == nullptr) ?
             "Unknown" : frame->GetFrameMap()->GetMapPath().c_str(), frame->GetFrameFuncName().c_str(),
             frame->GetFrameFuncOffset());
    }
    return writeNumber;
}

bool DfxDumpCatcherLocalDumper::ExecLocalDump(const int pid, const int tid, const int skipFramNum)
{
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: %{public}s : pid(%{public}d), tid(%{public}d), skipFramNum(%{public}d).",
        LOG_TAG, __func__, pid, tid, skipFramNum);
#endif

    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    size_t index = 0;
    auto maps = DfxElfMaps::Create(pid);
    while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS)) {
        std::shared_ptr<DfxFrames> frame = std::make_shared<DfxFrames>();

        unw_word_t pc;
        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) {
            break;
        }
        frame->SetFramePc((uint64_t)pc);

        unw_word_t sp;
        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(sp)))) {
            break;
        }
        frame->SetFrameSp((uint64_t)sp);

        auto frameMap = frame->GetFrameMap();
        if (maps->FindMapByAddr(frame->GetFramePc(), frameMap)) {
            frame->SetFrameRelativePc(frame->GetRelativePc(maps));
        }

        char sym[SYMBOL_BUF_SIZE] = {0};
        uint64_t frameOffset = frame->GetFrameFuncOffset();
        if (unw_get_proc_name(&cursor, sym, SYMBOL_BUF_SIZE, (unw_word_t*)(&frameOffset)) == 0) {
            std::string funcName;
            std::string strSym(sym, sym + strlen(sym));
            TrimAndDupStr(strSym, funcName);
            frame->SetFrameFuncName(funcName);
            frame->SetFrameFuncOffset(frameOffset);
        }

        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index >= skipFramNum) {
            int writeNumber = WriteDumpInfo(g_CurrentPosition, index - skipFramNum, frame);
            g_CurrentPosition = g_CurrentPosition + writeNumber;
        }
        index++;
    }

#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: ExecLocalDump :: return true.", LOG_TAG);
#endif
    return true;
}

void DfxDumpCatcherLocalDumper::DFX_LocalDumperUnwindLocal(int sig, siginfo_t *si, void *context)
{
    DfxLogToSocket("DFX_LocalDumperUnwindLocal");
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: sig(%{public}d), callerPid(%{public}d), callerTid(%{public}d).",
        __func__, sig, si->si_pid, si->si_uid);
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_LocalDumperUnwindLocal :: sig(%{public}d), pid(%{public}d), tid(%{public}d).",
        sig, g_localDumpRequest.pid, g_localDumpRequest.tid);
#endif

    ExecLocalDump(g_localDumpRequest.pid, g_localDumpRequest.tid, NUMBER_ONE);
}

void DfxDumpCatcherLocalDumper::DFX_LocalDumper(int sig, siginfo_t *si, void *context)
{
    pthread_mutex_lock(&g_localDumperMutex);

    (void)memset_s(&g_localDumpRequest, sizeof(g_localDumpRequest), 0, sizeof(g_localDumpRequest));
    g_localDumpRequest.type = sig;
    g_localDumpRequest.tid = gettid();
    g_localDumpRequest.pid = getpid();
    g_localDumpRequest.uid = getuid();
    g_localDumpRequest.reserved = 0;
    g_localDumpRequest.timeStamp = time(NULL);
    if (memcpy_s(&(g_localDumpRequest.siginfo), sizeof(g_localDumpRequest.siginfo),
        si, sizeof(siginfo_t)) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to copy siginfo.");
        pthread_mutex_unlock(&g_localDumperMutex);
        return;
    }
    if (memcpy_s(&(g_localDumpRequest.context), sizeof(g_localDumpRequest.context),
        context, sizeof(ucontext_t)) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to copy ucontext.");
        pthread_mutex_unlock(&g_localDumperMutex);
        return;
    }

#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_LocalDumper :: sig(%{public}d).", sig);
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_LocalDumper :: pid(%{public}d), tid(%{public}d).",
        g_localDumpRequest.pid, g_localDumpRequest.tid);
    if (sig == SIGDUMP) {
        HILOG_BASE_DEBUG(LOG_CORE, "DFX_LocalDumper :: callerPid(%{public}d), callerTid(%{public}d).",
            si->si_pid, si->si_uid);
    }
#endif

    if (sig == SIGDUMP) {
        if ((g_localDumpRequest.pid == si->si_pid) && (g_StackInfo_ != nullptr)) {
#ifdef LOCAL_DUMPER_DEBUG
            HILOG_BASE_DEBUG(LOG_CORE, "DFX_LocalDumper :: call DFX_LocalDumperUnwindLocal.");
#endif
            DFX_LocalDumperUnwindLocal(sig, si, context);
        }
    }
    pthread_mutex_unlock(&g_localDumperMutex);
}

void DfxDumpCatcherLocalDumper::DFX_InstallLocalDumper(int sig)
{
    if (sig != SIGDUMP) {
        return;
    }

    pthread_mutex_lock(&g_localDumperMutex);
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_InstallLocalDumper :: sig(%{public}d). -S-.", sig);
#endif

    if (g_localDumperHasInit == true) {
        pthread_mutex_unlock(&g_localDumperMutex);
        return;
    }

    struct sigaction action;
    memset_s(&action, sizeof(action), 0, sizeof(action));
    memset_s(&g_localDumperOldSigactionList, sizeof(g_localDumperOldSigactionList), \
        0, sizeof(g_localDumperOldSigactionList));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DfxDumpCatcherLocalDumper::DFX_LocalDumper;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(sig, &action, &(g_localDumperOldSigactionList[sig])) != 0) {
        DfxLogToSocket("DFX_InstallLocalDumper :: Failed to register signal.");
    }
    g_localDumperHasInit = true;

#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_InstallLocalDumper :: g_localDumperHasInit(%{public}d). -E-.",
        g_localDumperHasInit);
#endif
    pthread_mutex_unlock(&g_localDumperMutex);
}

void DfxDumpCatcherLocalDumper::DFX_UninstallLocalDumper(int sig)
{
    pthread_mutex_lock(&g_localDumperMutex);
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_UninstallLocalDumper :: sig(%{public}d). -S-.", sig);
#endif
    if (g_localDumperHasInit == false) {
        pthread_mutex_unlock(&g_localDumperMutex);
        return;
    }

    if (g_localDumperOldSigactionList[sig].sa_sigaction == NULL) {
        signal(sig, SIG_DFL);
        HILOG_BASE_ERROR(LOG_CORE, "DFX_UninstallLocalDumper :: old sig action is null.");
#ifdef LOCAL_DUMPER_DEBUG
        HILOG_BASE_DEBUG(LOG_CORE, "DFX_UninstallLocalDumper -E-.");
#endif
        pthread_mutex_unlock(&g_localDumperMutex);
        return;
    }

    if (sigaction(sig, &(g_localDumperOldSigactionList[sig]), NULL) != 0) {
        DfxLogToSocket("DFX_UninstallLocalDumper :: Failed to reset signal.");
        signal(sig, SIG_DFL);
    }
    g_localDumperHasInit = false;
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_UninstallLocalDumper :: g_localDumperHasInit(%{public}d) -E-.",
        g_localDumperHasInit);
#endif
    pthread_mutex_unlock(&g_localDumperMutex);
}
}
}
