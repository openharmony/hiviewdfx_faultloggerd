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
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>

#include <securec.h>
#include "log_wrapper.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxFuncHook"
#endif

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

void SetPlatformSignalHandler(uintptr_t handler)
{
    if (g_signalHandler == 0) {
        g_signalHandler = handler;
    }
}

int kill(pid_t pid, int sig)
{
    // debug kill, abort if kill themself
    LOGI("%d send signal(%d) to %d\n", getpid(), sig, pid);
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
            if (sigismember(set, i) &&
                ((how == SIG_BLOCK) || (how == SIG_SETMASK))) {
                LOGI("%d:%d pthread_sigmask signal(%d)\n", getpid(), gettid(), i);
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
            if (sigismember(set, i) &&
                ((how == SIG_BLOCK) || (how == SIG_SETMASK))) {
                LOGI("%d:%d sigprocmask signal(%d)\n", getpid(), gettid(), i);
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
    LOGI("%d register signal handler for signal(%d)\n", getpid(), signum);
    if (hookedSignal == NULL) {
        LOGE("hooked signal is NULL?\n");
        return NULL;
    }
    return hookedSignal(signum, handler);
}

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact)
{
    LOGI("%d call sigaction and signo is %d\n", getpid(), sig);
    if (hookedSigaction == NULL) {
        LOGE("hooked sigaction is NULL?");
        return -1;
    }

    if (act != NULL && ((uintptr_t)(act->sa_sigaction) != (uintptr_t)g_signalHandler)) {
        LOGE("current signalhandler addr:%p, original addr:%p\n",
            (uintptr_t)act->sa_sigaction, (uintptr_t)g_signalHandler);
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
