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

#include "dfx_signal_hook.h"

#include <dlfcn.h>
#include <securec.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_log.h"
#include "pthread.h"

#include "dfx_hook_utils.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSigHook"
#endif

#ifndef SIGDUMP
#define SIGDUMP 35
#endif

#define MIN_FRAME 4
#define MAX_FRAME 64
#define BUF_SZ 512
#define MAPINFO_SIZE 256
#define MAX_SIGNO 63

void __attribute__((constructor)) InitHook(void)
{
    StartHookFunc();
}

typedef int (*SigactionFunc)(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact);
typedef int (*SigprocmaskFunc)(int how, const sigset_t *restrict set, sigset_t *restrict oldset);
typedef int (*PthreadSigmaskFunc)(int how, const sigset_t *restrict set, sigset_t *restrict oldset);
typedef sighandler_t (*SignalFunc)(int signum, sighandler_t handler);
static SigactionFunc g_hookedSigaction = NULL;
static SigprocmaskFunc g_hookedSigprocmask = NULL;
static SignalFunc g_hookedSignal = NULL;
static PthreadSigmaskFunc g_hookedPthreadSigmask = NULL;

static bool IsPlatformHandleSignal(int sig)
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

int pthread_sigmask(int how, const sigset_t *restrict set, sigset_t *restrict oldset)
{
    if (set != NULL) {
        for (int i = 1; i < MAX_SIGNO; i++) {
            if (sigismember(set, i) && (IsPlatformHandleSignal(i)) &&
                ((how == SIG_BLOCK) || (how == SIG_SETMASK))) {
                DFXLOGI("%{public}d:%{public}d pthread_sigmask signal(%{public}d)\n", getpid(), gettid(), i);
            }
        }
    }

    if (g_hookedPthreadSigmask == NULL) {
        DFXLOGE("hooked procmask is NULL?\n");
        return -1;
    }
    return g_hookedPthreadSigmask(how, set, oldset);
}

int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oldset)
{
    if (set != NULL) {
        for (int i = 1; i < MAX_SIGNO; i++) {
            if (sigismember(set, i) && (IsPlatformHandleSignal(i)) &&
                ((how == SIG_BLOCK) || (how == SIG_SETMASK))) {
                DFXLOGI("%{public}d:%{public}d sigprocmask signal(%{public}d)\n", getpid(), gettid(), i);
            }
        }
    }
    if (g_hookedSigprocmask == NULL) {
        DFXLOGE("hooked procmask is NULL?\n");
        return -1;
    }
    return g_hookedSigprocmask(how, set, oldset);
}

sighandler_t signal(int signum, sighandler_t handler)
{
    if (IsPlatformHandleSignal(signum)) {
        DFXLOGI("%{public}d register signal handler for signal(%{public}d)\n", getpid(), signum);
    }

    if (g_hookedSignal == NULL) {
        DFXLOGE("hooked signal is NULL?\n");
        return NULL;
    }
    return g_hookedSignal(signum, handler);
}

static bool IsSigactionAddr(uintptr_t sigactionAddr)
{
    bool ret = false;
    char path[NAME_BUF_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, PROC_SELF_MAPS_PATH) <= 0) {
        DFXLOGW("Fail to print path.");
        return false;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        DFXLOGW("Fail to open maps info.");
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
            DFXLOGW("Fail to parse maps info.");
            continue;
        }

        if ((strstr(mapInfo, "r-xp") != NULL) && (strstr(mapInfo, "ld-musl") != NULL)) {
            DFXLOGI("begin: %{public}" PRIu64 ", end: %{public}" PRIu64 ", sigactionAddr: %{public}" PRIuPTR  "",
                begin, end, sigactionAddr);
            if ((sigactionAddr >= begin) && (sigactionAddr <= end)) {
                ret = true;
                break;
            }
        } else {
            continue;
        }
    }
    if (fclose(fp) != 0) {
        DFXLOGW("Fail to close maps info.");
    }
    return ret;
}

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact)
{
    if (g_hookedSigaction == NULL) {
        DFXLOGE("hooked sigaction is NULL?");
        return -1;
    }

    if (IsPlatformHandleSignal(sig) && (act == NULL || !IsSigactionAddr((uintptr_t)(act->sa_sigaction)))) {
        DFXLOGI("%{public}d call sigaction and signo is %{public}d\n", getpid(), sig);
    }

    return g_hookedSigaction(sig, act, oact);
}

GEN_HOOK_FUNC(StartHookSigactionFunction, SigactionFunc, "sigaction", g_hookedSigaction)
GEN_HOOK_FUNC(StartHookSignalFunction, SignalFunc, "signal", g_hookedSignal)
GEN_HOOK_FUNC(StartHookSigprocmaskFunction, SigprocmaskFunc, "sigprocmask", g_hookedSigprocmask)
GEN_HOOK_FUNC(StartHookPthreadSigmaskFunction, PthreadSigmaskFunc, "pthread_sigmask", g_hookedPthreadSigmask)

void StartHookFunc(void)
{
    StartHookSigactionFunction();
    StartHookSignalFunction();
    StartHookSigprocmaskFunction();
    StartHookPthreadSigmaskFunction();
}
