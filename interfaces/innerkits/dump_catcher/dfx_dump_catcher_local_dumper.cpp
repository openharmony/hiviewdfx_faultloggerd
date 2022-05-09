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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <securec.h>
#include <unistd.h>

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
std::vector<DfxDumpCatcherFrame> DfxDumpCatcherLocalDumper::g_FrameV;
std::mutex DfxDumpCatcherLocalDumper::g_localDumperMutx;
static sigset_t g_mask;
static std::unique_ptr<DfxSymbolsCache> g_cache;
static unw_addr_space_t g_localAddrSpace = nullptr;

bool DfxDumpCatcherLocalDumper::InitLocalDumper()
{
    unw_init_local_address_space(&g_localAddrSpace);
    if (g_localAddrSpace == nullptr) {
        return false;
    }

    DfxDumpCatcherLocalDumper::g_FrameV = std::vector<DfxDumpCatcherFrame>(MAX_FRAME_SIZE);
    DfxDumpCatcherLocalDumper::DFX_InstallLocalDumper(SIGLOCAL_DUMP);
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_SETMASK, &mask, &g_mask);
    g_cache = std::make_unique<DfxSymbolsCache>();
    DfxDumpCatcherLocalDumper::g_isLocalDumperInited = true;
    return true;
}

void DfxDumpCatcherLocalDumper::DestroyLocalDumper()
{
    DfxDumpCatcherLocalDumper::g_FrameV.clear();
    DfxDumpCatcherLocalDumper::g_FrameV.shrink_to_fit();
    DfxDumpCatcherLocalDumper::DFX_UninstallLocalDumper(SIGLOCAL_DUMP);
    sigprocmask(SIG_SETMASK, &g_mask, nullptr);
    unw_destroy_local_address_space(g_localAddrSpace);
    g_localAddrSpace = nullptr;
    g_cache = nullptr;
    DfxDumpCatcherLocalDumper::g_isLocalDumperInited = false;
}

bool DfxDumpCatcherLocalDumper::SendLocalDumpRequest(int32_t tid)
{
    g_localDumpRequest.tid = tid;
    g_localDumpRequest.timeStamp = (uint64_t)time(NULL);
    return syscall(SYS_tkill, tid, SIGLOCAL_DUMP) == 0;
}

DfxDumpCatcherLocalDumper::DfxDumpCatcherLocalDumper()
{
#ifdef LOCAL_DUMPER_DEBUG
    DfxLogDebug("%{public}s :: construct.", LOG_TAG);
#endif
}

DfxDumpCatcherLocalDumper::~DfxDumpCatcherLocalDumper()
{
#ifdef LOCAL_DUMPER_DEBUG
    DfxLogDebug("%{public}s :: destructor.", LOG_TAG);
#endif
}

std::string DfxDumpCatcherLocalDumper::CollectUnwindResult(int32_t tid)
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

    if (g_curIndex == 0) {
        result << "Failed to get stacktrace." << std::endl;
    }

    for (uint32_t i = 0; i < g_curIndex; ++i) {
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

    for (uint32_t i = 0; i < g_curIndex; ++i) {
        ResolveFrameInfo(g_FrameV[i]);
        frames.push_back(std::make_shared<DfxDumpCatcherFrame>(g_FrameV[i]));
    }
}

void DfxDumpCatcherLocalDumper::ResolveFrameInfo(DfxDumpCatcherFrame& frame)
{
    if (g_cache == nullptr) {
        return;
    }

    if (!g_cache->GetNameAndOffsetByPc(g_localAddrSpace, frame.pc_, frame.funcName_, frame.funcOffset_)) {
        frame.funcName_ = "";
        frame.funcOffset_ = 0;
    }
}

void DfxDumpCatcherLocalDumper::WriteFrameInfo(std::ostringstream& ss, size_t index, DfxDumpCatcherFrame& frame)
{
#ifdef __LP64__
    char format[] = "#%02zu pc %016" PRIx64 " ";
#else
    char format[] = "#%02zu pc %08" PRIx64 " ";
#endif
    char buf[SYMBOL_BUF_SIZE] = { 0 };
    (void)sprintf_s(buf, sizeof(buf), format, index, frame.relativePc_);
    if (strlen(buf) > 100) { // 100 : expected result length
        ss << " Illegal frame" << std::endl;
        return;
    }

    ss << std::string(buf, strlen(buf)) << " ";
    ss << std::string(frame.mapName_);

    if (frame.funcName_.empty()) {
        ss << std::endl;
        return;
    }

    ss << "(";
    ss << frame.funcName_;
    ss << "+" << frame.funcOffset_ << ")" << std::endl;
}

bool DfxDumpCatcherLocalDumper::ExecLocalDump(int tid, size_t skipFramNum)
{
    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local_with_as(g_localAddrSpace, &cursor, &context);

    size_t index = 0;
    DfxDumpCatcherLocalDumper::g_curIndex = 0;
    while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS)) {
        if (tid != gettid()) {
            break;
        }
        // skip 0 stack, as this is dump catcher. Caller don't need it.
        if (index < skipFramNum) {
            index++;
            continue;
        }

        unw_word_t pc;
        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(pc)))) {
            break;
        }

        unw_word_t relPc = unw_get_rel_pc(&cursor);
        unw_word_t sz = unw_get_previous_instr_sz(&cursor);
        if (index - skipFramNum != 0) {
            pc -= sz;
            relPc -= sz;
        }

        auto& curFrame = g_FrameV[index - skipFramNum];
        struct map_info* map = unw_get_map(&cursor);
        if ((map != NULL) && (strlen(map->path) < SYMBOL_BUF_SIZE - 1)) {
            strcpy_s(curFrame.mapName_, SYMBOL_BUF_SIZE, map->path);
        } else {
            strcpy_s(curFrame.mapName_, SYMBOL_BUF_SIZE, "Unknown");
        }

        curFrame.SetFramePc((uint64_t)pc);
        curFrame.SetFrameRelativePc((uint64_t)relPc);
        DfxDumpCatcherLocalDumper::g_curIndex = static_cast<uint32_t>(index - skipFramNum);
        index++;
    }
    return true;
}

void DfxDumpCatcherLocalDumper::DFX_LocalDumperUnwindLocal(int sig, siginfo_t *si, void *context)
{
    ExecLocalDump(g_localDumpRequest.tid, DUMP_CATCHER_NUMBER_TWO);
    if (g_localDumpRequest.tid == gettid()) {
        g_localDumperCV.notify_one();
    }
}

void DfxDumpCatcherLocalDumper::DFX_LocalDumper(int sig, siginfo_t *si, void *context)
{
    pthread_mutex_lock(&g_localDumperMutex);
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
