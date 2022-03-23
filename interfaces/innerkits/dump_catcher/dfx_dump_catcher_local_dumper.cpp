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

#include <libunwind.h>

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxDumpCatcherLocalDumper"
#endif

#ifndef NSIG
#define NSIG 64
#endif

#ifndef LOCAL_DUMPER_DEBUG
#define LOCAL_DUMPER_DEBUG
#endif
#undef LOCAL_DUMPER_DEBUG

namespace OHOS {
namespace HiviewDFX {
static constexpr int SYMBOL_BUF_SIZE = 1024;
static constexpr int SECONDS_TO_MILLSECONDS = 1000000;
static constexpr int NANOSECONDS_TO_MILLSECONDS = 1000;
static constexpr int NUMBER_SIXTYFOUR = 64;
static constexpr int INHERITABLE_OFFSET = 32;
constexpr int SIGLOCAL_DUMP = 36;
constexpr int MAX_FRAME_SIZE = 64;

static struct LocalDumperRequest g_localDumpRequest;
static pthread_mutex_t g_localDumperMutex = PTHREAD_MUTEX_INITIALIZER;
static struct sigaction g_localOldSigaction = {};

uint32_t DfxDumpCatcherLocalDumper::g_curIndex = 0;
bool DfxDumpCatcherLocalDumper::g_isLocalDumperInited = false;
std::condition_variable DfxDumpCatcherLocalDumper::g_localDumperCV;
std::shared_ptr<DfxElfMaps> DfxDumpCatcherLocalDumper::g_localDumperMaps = nullptr;
std::vector<DfxDumpCatcherFrame> DfxDumpCatcherLocalDumper::g_FrameV;
std::mutex DfxDumpCatcherLocalDumper::g_localDumperMutx;

bool DfxDumpCatcherLocalDumper::InitLocalDumper()
{
    DfxDumpCatcherLocalDumper::g_localDumperMaps = DfxElfMaps::Create(getpid());
    DfxDumpCatcherLocalDumper::g_FrameV = std::vector<DfxDumpCatcherFrame>(MAX_FRAME_SIZE);
    DfxDumpCatcherLocalDumper::DFX_InstallLocalDumper(SIGLOCAL_DUMP);
    DfxDumpCatcherLocalDumper::g_isLocalDumperInited = true;
    return true;
}

void DfxDumpCatcherLocalDumper::DestroyLocalDumper()
{
    DfxDumpCatcherLocalDumper::g_localDumperMaps = nullptr;
    DfxDumpCatcherLocalDumper::g_FrameV.clear();
    DfxDumpCatcherLocalDumper::DFX_UninstallLocalDumper(SIGLOCAL_DUMP);
    DfxDumpCatcherLocalDumper::g_isLocalDumperInited = false;
}

bool DfxDumpCatcherLocalDumper::SendLocalDumpRequest(int32_t tid)
{
    return syscall(SYS_tkill, tid, SIGLOCAL_DUMP) == 0;
}

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

std::string DfxDumpCatcherLocalDumper::CollectUnwindResult()
{
    std::ostringstream result;
    result << "Tid:" << g_localDumpRequest.tid << std::endl;
    if (g_curIndex == 0) {
        result << "Failed to get stacktrace." << std::endl;
    }

    for (int i = 0; i < g_curIndex; ++i) {
        ResolveFrameInfo(g_FrameV[i]);
        WriteFrameInfo(result, i, g_FrameV[i]);
    }

    result << std::endl;
    return result.str();
}

void DfxDumpCatcherLocalDumper::CollectUnwindFrames(std::vector<std::shared_ptr<DfxDumpCatcherFrame>>& frames)
{
    if (g_curIndex == 0) {
        return;
    }

    for (int i = 0; i < g_curIndex; ++i) {
        ResolveFrameInfo(g_FrameV[i]);
        frames.push_back(std::make_shared<DfxDumpCatcherFrame>(g_FrameV[i]));
    }
}

void DfxDumpCatcherLocalDumper::ResolveFrameInfo(DfxDumpCatcherFrame& frame)
{
    if (g_localDumperMaps->FindMapByAddr(frame.GetFramePc(), frame.map_)) {
        frame.SetFrameRelativePc(frame.GetRelativePc(g_localDumperMaps));
    }
}

void DfxDumpCatcherLocalDumper::WriteFrameInfo(std::ostringstream& ss, size_t index, DfxDumpCatcherFrame& frame)
{
    char buf[SYMBOL_BUF_SIZE] = { 0 };
    (void)sprintf_s(buf, sizeof(buf), "#%02zu pc %016" PRIx64 " ", index, frame.relativePc_);
    if (strlen(buf) > 100) { // 100 : expected result length
        ss << " Illegal frame" << std::endl;
        return;
    }

    ss << std::string(buf, strlen(buf)) << " ";
    if (frame.GetFrameMap() == nullptr) {
        ss << "Unknown" << std::endl;
        return;
    }

    ss << frame.GetFrameMap()->GetMapPath() << "(";
    ss << std::string(frame.funcName_);
    ss << "+" << frame.funcOffset_ << ")" << std::endl;
}

bool DfxDumpCatcherLocalDumper::ExecLocalDump(int pid, int tid, size_t skipFramNum)
{
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: %{public}s : pid(%{public}d), tid(%{public}d), skpFram(%{public}d).", \
        LOG_TAG, __func__, pid, tid, skipFramNum);
#endif

    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local(&cursor, &context);

    size_t index = 0;
    DfxDumpCatcherLocalDumper::g_curIndex = 0;
    while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS)) {
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFramNum) {
            index++;
            continue;
        }

        unw_word_t pc;
        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) {
            break;
        }
        g_FrameV[index - skipFramNum].SetFramePc((uint64_t)pc);

        unw_word_t sp;
        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(sp)))) {
            break;
        }

        g_FrameV[index - skipFramNum].SetFrameSp((uint64_t)sp);
        (void)unw_get_proc_name(&cursor, g_FrameV[index - skipFramNum].funcName_,
            SYMBOL_BUF_SIZE, (unw_word_t*)(&g_FrameV[index - skipFramNum].funcOffset_));
        DfxDumpCatcherLocalDumper::g_curIndex = static_cast<uint32_t>(index - skipFramNum);
        index++;
    }

#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: ExecLocalDump :: return true.", LOG_TAG);
#endif
    return true;
}

void DfxDumpCatcherLocalDumper::DFX_LocalDumperUnwindLocal(int sig, siginfo_t *si, void *context)
{
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: DFX_LocalDumperUnwindLocal.", LOG_TAG);
    DfxLogToSocket("DFX_LocalDumperUnwindLocal -S-");
#ifdef LOCAL_DUMPER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "%{public}s :: sig(%{public}d), callerPid(%{public}d), callerTid(%{public}d).",
        __func__, sig, si->si_pid, si->si_uid);
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_LocalDumperUnwindLocal :: sig(%{public}d), pid(%{public}d), tid(%{public}d).",
        sig, g_localDumpRequest.pid, g_localDumpRequest.tid);
#endif
    ExecLocalDump(g_localDumpRequest.pid, g_localDumpRequest.tid, DUMP_CATCHER_NUMBER_ONE);
    g_localDumperCV.notify_one();
    DfxLogToSocket("DFX_LocalDumperUnwindLocal -E-");
}

void DfxDumpCatcherLocalDumper::DFX_LocalDumper(int sig, siginfo_t *si, void *context)
{
    pthread_mutex_lock(&g_localDumperMutex);
    (void)memset_s(&g_localDumpRequest, sizeof(g_localDumpRequest), 0, sizeof(g_localDumpRequest));
    g_localDumpRequest.type = sig;
    g_localDumpRequest.tid = gettid();
    g_localDumpRequest.pid = getpid();
    g_localDumpRequest.timeStamp = (uint64_t)time(NULL);
    DFX_LocalDumperUnwindLocal(sig, si, context);
    pthread_mutex_unlock(&g_localDumperMutex);
}

void DfxDumpCatcherLocalDumper::DFX_InstallLocalDumper(int sig)
{
    struct sigaction action;
    memset_s(&action, sizeof(action), 0, sizeof(action));
    memset_s(&g_localOldSigaction, sizeof(g_localOldSigaction), \
        0, sizeof(g_localOldSigaction));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DfxDumpCatcherLocalDumper::DFX_LocalDumper;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

    if (sigaction(sig, &action, &g_localOldSigaction) != EOK) {
        DfxLogToSocket("DFX_InstallLocalDumper :: Failed to register signal.");
    }
}

void DfxDumpCatcherLocalDumper::DFX_UninstallLocalDumper(int sig)
{
    if (g_localOldSigaction.sa_sigaction == nullptr) {
        signal(sig, SIG_DFL);
        return;
    }

    if (sigaction(sig, &g_localOldSigaction, NULL) != EOK) {
        DfxLogToSocket("DFX_UninstallLocalDumper :: Failed to reset signal.");
        signal(sig, SIG_DFL);
    }
}
} // namespace HiviewDFX
} // namespace OHOS
