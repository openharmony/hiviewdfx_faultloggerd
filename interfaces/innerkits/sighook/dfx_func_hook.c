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

#include <dlfcn.h>                // for dlsym, RTLD_DEFAULT, RTLD_NEXT
#include <libunwind_i-ohos.h>     // for _Uarm_init_local_with_as, unw_destr...
#include <map_info.h>             // for map_info
#include <securec.h>              // for memset_s
#include <signal.h>               // for sigset_t, sigaction, sigismember
#include <stdio.h>                // for NULL, size_t
#include <unistd.h>               // for getpid, gettid, pid_t
#include "dfx_log.h"              // for LOGE, LOGI
#include "hilog_base/log_base.h"  // for LOG_DOMAIN, LOG_TAG
#include "libunwind-arm.h"        // for unw_word_t, _Uarm_get_reg, _Uarm_step
#include "libunwind_i.h"          // for unw_addr_space
#include "pthread.h"              // for pthread_mutex_unlock, pthread_mutex...
#include "stdbool.h"              // for true, bool, false
#include "stdlib.h"               // for abort

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
static uintptr_t g_signalHandler = 0;
static pthread_mutex_t g_backtraceLock = PTHREAD_MUTEX_INITIALIZER;
void SetPlatformSignalHandler(uintptr_t handler)
{
    if (g_signalHandler == 0) {
        g_signalHandler = handler;
    }
}

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

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact)
{
    if (hookedSigaction == NULL) {
        LOGE("hooked sigaction is NULL?");
        return -1;
    }

    if (IsPlatformHandleSignal(sig) && ((act == NULL) ||
        ((act != NULL) && ((uintptr_t)(act->sa_sigaction) != (uintptr_t)g_signalHandler)))) {
        LOGI("%d call sigaction and signo is %d\n", getpid(), sig);
        LogBacktrace();
    }

    return hookedSigaction(sig, act, oact);
}

static void StartHookKillFunction(void)
{
    hookedKill = (KillFunc)dlsym(RTLD_NEXT, "kill");
    if (hookedKill != NULL) {
        return;
    }
    LOGE("Failed to find hooked kill use RTLD_NEXT\n");

    hookedKill = (KillFunc)dlsym(RTLD_DEFAULT, "kill");
    if (hookedKill != NULL) {
        return;
    }
    LOGE("Failed to find hooked kill use RTLD_DEFAULT\n");
}

static void StartHookSigactionFunction(void)
{
    hookedSigaction = (SigactionFunc)dlsym(RTLD_NEXT, "sigaction");
    if (hookedSigaction != NULL) {
        return;
    }
    LOGE("Failed to find hooked sigaction use RTLD_NEXT\n");

    hookedSigaction = (SigactionFunc)dlsym(RTLD_DEFAULT, "sigaction");
    if (hookedSigaction != NULL) {
        return;
    }
    LOGE("Failed to find hooked sigaction use RTLD_DEFAULT\n");
}

static void StartHookSigprocmaskFunction(void)
{
    hookedSigprocmask = (SigprocmaskFunc)dlsym(RTLD_NEXT, "sigprocmask");
    if (hookedSigprocmask != NULL) {
        return;
    }
    LOGE("Failed to find hooked Sigprocmask use RTLD_NEXT\n");

    hookedSigprocmask = (SigprocmaskFunc)dlsym(RTLD_DEFAULT, "sigprocmask");
    if (hookedSigprocmask != NULL) {
        return;
    }
    LOGE("Failed to find hooked Sigprocmask use RTLD_DEFAULT\n");
}

static void StartHookPthreadSigmaskFunction(void)
{
    hookedPthreadSigmask = (SigprocmaskFunc)dlsym(RTLD_NEXT, "pthread_sigmask");
    if (hookedPthreadSigmask != NULL) {
        return;
    }
    LOGE("Failed to find hooked Sigprocmask use RTLD_NEXT\n");

    hookedPthreadSigmask = (SigprocmaskFunc)dlsym(RTLD_DEFAULT, "pthread_sigmask");
    if (hookedPthreadSigmask != NULL) {
        return;
    }
    LOGE("Failed to find hooked Sigprocmask use RTLD_DEFAULT\n");
}

static void StartHookSignalFunction(void)
{
    hookedSignal = (SignalFunc)dlsym(RTLD_NEXT, "signal");
    if (hookedSignal != NULL) {
        return;
    }

    hookedSignal = (SignalFunc)dlsym(RTLD_DEFAULT, "signal");
    if (hookedSignal != NULL) {
        return;
    }
}

void StartHookFunc(void)
{
    StartHookKillFunction();
    StartHookSigactionFunction();
    StartHookSignalFunction();
    StartHookSigprocmaskFunction();
    StartHookPthreadSigmaskFunction();
}
