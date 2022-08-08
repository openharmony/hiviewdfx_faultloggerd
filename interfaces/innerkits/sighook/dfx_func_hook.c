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

#include "dfx_func_hook.h"

#include <dlfcn.h>
#include <libunwind_i-ohos.h>
#include <map_info.h>
#include <securec.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "hilog_base/log_base.h"
#include "libunwind-arm.h"
#include "libunwind_i.h"
#include "pthread.h"
#include "stdbool.h"
#include "stdlib.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxFuncHook"
#endif

#ifndef SIGDUMP
#define SIGDUMP 35
#endif

#define MIN_FRAME 4
#define MAX_FRAME 64
#define BUF_SZ 512
#define MAPINFO_SIZE 256

void __attribute__((constructor)) InitHook(void)
{
    StartHookFunc();
}

typedef int (*KillFunc)(pid_t pid, int sig);
typedef int (*SigactionFunc)(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact);
typedef int (*SigprocmaskFunc)(int how, const sigset_t *restrict set, sigset_t *restrict oldset);
typedef int (*PthreadSigmaskFunc)(int how, const sigset_t *restrict set, sigset_t *restrict oldset);
typedef sighandler_t (*SignalFunc)(int signum, sighandler_t handler);
static KillFunc hookedKill = NULL;
static SigactionFunc hookedSigaction = NULL;
static SigprocmaskFunc hookedSigprocmask = NULL;
static SignalFunc hookedSignal = NULL;
static PthreadSigmaskFunc hookedPthreadSigmask = NULL;
static pthread_mutex_t g_backtraceLock = PTHREAD_MUTEX_INITIALIZER;

void LogBacktrace()
{
    pthread_mutex_lock(&g_backtraceLock);
    unw_addr_space_t as = NULL;
    unw_init_local_address_space(&as);
    if (as == NULL) {
        pthread_mutex_unlock(&g_backtraceLock);
        return;
    }

    unw_context_t context;
    unw_getcontext(&context);

    unw_cursor_t cursor;
    unw_init_local_with_as(as, &cursor, &context);

    int index = 0;
    int ret;
    unw_word_t pc;
    unw_word_t relPc;
    unw_word_t prevPc;
    unw_word_t sz;
    uint64_t start;
    uint64_t end;
    struct map_info* mapInfo;
    bool shouldContinue = true;
    while (true) {
        if (index > MAX_FRAME) {
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&pc))) {
            break;
        }

        if (index > MIN_FRAME && prevPc == pc) {
            break;
        }
        prevPc = pc;

        relPc = unw_get_rel_pc(&cursor);
        mapInfo = unw_get_map(&cursor);
        if (mapInfo == NULL && index > 1) {
            break;
        }

        sz = unw_get_previous_instr_sz(&cursor);
        if (index != 0 && relPc > sz) {
            relPc -= sz;
        }

        char buf[BUF_SZ];
        (void)memset_s(&buf, sizeof(buf), 0, sizeof(buf));
        if (unw_get_symbol_info_by_pc(as, pc, BUF_SZ, buf, &start, &end) == 0) {
            LOGI("#%02d %016p(%016p) %s(%s)\n", index, relPc, pc,
                mapInfo == NULL ? "Unknown" : mapInfo->path,
                buf);
        } else {
            LOGI("#%02d %016p(%016p) %s\n", index, relPc, pc,
                mapInfo == NULL ? "Unknown" : mapInfo->path);
        }
        index++;

        if (!shouldContinue) {
            break;
        }

        ret = unw_step(&cursor);
        if (ret == 0) {
            shouldContinue = false;
        } else if (ret < 0) {
            break;
        }
    }
    unw_destroy_local_address_space(as);
    pthread_mutex_unlock(&g_backtraceLock);
}

bool IsPlatformHandleSignal(int sig)
{
    int platformSignals[] = {
        SIGABRT, SIGBUS, SIGILL, SIGSEGV, SIGDUMP
    };
    for (size_t i = 0; i < sizeof(platformSignals) / sizeof(platformSignals[0]); i++) {
        if (platformSignals[i] == sig) {
            return true;
        }
    }
    return false;
}

int kill(pid_t pid, int sig)
{
    LOGI("%d send signal(%d) to %d\n", getpid(), sig, pid);
    if ((sig == SIGKILL) && (pid == getpid())) {
        abort();
    } else if (sig == SIGKILL) {
        LogBacktrace();
    }

    if (hookedKill == NULL) {
        LOGE("hooked kill is NULL?\n");
        return -1;
    }
    return hookedKill(pid, sig);
}

int pthread_sigmask(int how, const sigset_t *restrict set, sigset_t *restrict oldset)
{
    if (set != NULL) {
        for (int i = 1; i < 63; i++) {
            if (sigismember(set, i) && (IsPlatformHandleSignal(i)) &&
                ((how == SIG_BLOCK) || (how == SIG_SETMASK))) {
                LOGI("%d:%d pthread_sigmask signal(%d)\n", getpid(), gettid(), i);
                LogBacktrace();
            }
        }
    }

    if (hookedPthreadSigmask == NULL) {
        LOGE("hooked procmask is NULL?\n");
        return -1;
    }
    return hookedPthreadSigmask(how, set, oldset);
}

int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oldset)
{
    if (set != NULL) {
        for (int i = 1; i < 63; i++) {
            if (sigismember(set, i) && (IsPlatformHandleSignal(i)) &&
                ((how == SIG_BLOCK) || (how == SIG_SETMASK))) {
                LOGI("%d:%d sigprocmask signal(%d)\n", getpid(), gettid(), i);
                LogBacktrace();
            }
        }
    }
    if (hookedSigprocmask == NULL) {
        LOGE("hooked procmask is NULL?\n");
        return -1;
    }
    return hookedSigprocmask(how, set, oldset);
}

sighandler_t signal(int signum, sighandler_t handler)
{
    if (IsPlatformHandleSignal(signum)) {
        LOGI("%d register signal handler for signal(%d)\n", getpid(), signum);
        LogBacktrace();
    }

    if (hookedSignal == NULL) {
        LOGE("hooked signal is NULL?\n");
        return NULL;
    }
    return hookedSignal(signum, handler);
}

static bool IsSigactionAddr(uintptr_t sigactionAddr)
{
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/self/maps") <= 0) {
        LOGW("Fail to print path.");
        return false;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        LOGW("Fail to open maps info.");
        return false;
    }

    char mapInfo[MAPINFO_SIZE] = {0};
    int pos = 0;
    uint64_t begin = 0;
    uint64_t end = 0;
    uint64_t offset = 0;
    char perms[5] = {0}; // 5:rwxp
    while (fgets(mapInfo, sizeof(mapInfo), fp) != NULL) {
        // f79d6000-f7a62000 r-xp 0004b000 b3:06 1605                               /system/lib/ld-musl-arm.so.1
        if (sscanf_s(mapInfo, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNxPTR " %*x:%*x %*d%n", &begin, &end,
            &perms, sizeof(perms), &offset, &pos) != 4) { // 4:scan size
            LOGW("Fail to parse maps info.");
            continue;
        }

        if ((strstr(mapInfo, "r-xp") != NULL) && (strstr(mapInfo, "ld-musl") != NULL)) {
            LOGI("begin: %lu, end: %lu, sigactionAddr: %lu", begin, end, sigactionAddr);
            if ((sigactionAddr >= begin) && (sigactionAddr <= end)) {
                return true;
            }
        } else {
            continue;
        }
    }
    if (fclose(fp) != 0) {
        LOGW("Fail to close maps info.");
    }
    return false;
}

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact)
{
    if (hookedSigaction == NULL) {
        LOGE("hooked sigaction is NULL?");
        return -1;
    }

    if (IsPlatformHandleSignal(sig) && ((act == NULL) ||
        ((act != NULL) && (IsSigactionAddr((uintptr_t)(act->sa_sigaction)))))) {
        LOGI("%d call sigaction and signo is %d\n", getpid(), sig);
        LogBacktrace();
    }

    return hookedSigaction(sig, act, oact);
}

#define GenHookFunc(StartHookFunction, RealHookFunc, FuncName, RealFuncName) \
void StartHookFunction(void) \
{ \
    RealFuncName = (RealHookFunc)dlsym(RTLD_NEXT, FuncName); \
    if (RealFuncName != NULL) { \
        return; \
    } \
    LOGE("Failed to find hooked %s use RTLD_NEXT\n", FuncName); \
    RealFuncName = (RealHookFunc)dlsym(RTLD_DEFAULT, FuncName); \
    if (RealFuncName != NULL) { \
        return; \
    } \
    LOGE("Failed to find hooked %s use RTLD_DEFAULT\n", FuncName); \
}

GenHookFunc(StartHookKillFunction, KillFunc, "kill", hookedKill)
GenHookFunc(StartHookSigactionFunction, SigactionFunc, "sigaction", hookedSigaction)
GenHookFunc(StartHookSignalFunction, SignalFunc, "signal", hookedSignal)
GenHookFunc(StartHookSigprocmaskFunction, SigprocmaskFunc, "sigprocmask", hookedSigprocmask)
GenHookFunc(StartHookPthreadSigmaskFunction, PthreadSigmaskFunc, "pthread_sigmask", hookedPthreadSigmask)

void StartHookFunc(void)
{
    StartHookKillFunction();
    StartHookSigactionFunction();
    StartHookSignalFunction();
    StartHookSigprocmaskFunction();
    StartHookPthreadSigmaskFunction();
}
