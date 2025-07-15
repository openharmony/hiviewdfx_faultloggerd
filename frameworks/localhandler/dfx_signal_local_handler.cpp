/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#include "dfx_signal_local_handler.h"

#include <securec.h>
#include <csignal>
#include <sigchain.h>
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <linux/futex.h>
#include "dfx_allocator.h"
#include "dfx_crash_local_handler.h"
#include "dfx_cutil.h"
#include "dfx_log.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSignalLocalHandler"
#endif

constexpr uint32_t FAULTLOGGERD_UID = 1202;

static CrashFdFunc g_crashFdFn = nullptr;
static SigAlarmFunc g_sigAlarmCallbackFn = nullptr;
#if !defined(__aarch64__) && !defined(__loongarch_lp64)
constexpr uint32_t LOCAL_HANDLER_STACK_SIZE = 128 * 1024; // 128K
static void *g_reservedChildStack = nullptr;
#endif
static struct ProcessDumpRequest g_request;
static pthread_mutex_t g_signalHandlerMutex = PTHREAD_MUTEX_INITIALIZER;

static constexpr int CATCHER_STACK_SIGNALS[] = {
    SIGABRT, SIGBUS, SIGILL, SIGSEGV, SIGALRM,
};

#if !defined(__aarch64__) && !defined(__loongarch_lp64)
static void ReserveChildThreadSignalStack(void)
{
    // reserve stack for fork
    g_reservedChildStack = mmap(nullptr, LOCAL_HANDLER_STACK_SIZE, \
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 1, 0);
    if (g_reservedChildStack == MAP_FAILED) {
        DFXLOGE("[%{public}d]: Failed to alloc memory for child stack.", __LINE__);
        return;
    }
    g_reservedChildStack = static_cast<void *>(static_cast<uint8_t *>(g_reservedChildStack) +
        LOCAL_HANDLER_STACK_SIZE - 1);
}
#endif

AT_UNUSED static void FutexWait(volatile void* ftx, int value)
{
    syscall(__NR_futex, ftx, FUTEX_WAIT, value, NULL, NULL, 0);
}

static int DoCrashHandler(void* arg)
{
    int sig = *(static_cast<int *>(arg));
    RegisterAllocator();
    DFXLOGI("DoCrashHandler::start handle sig(%d)", sig);
    if (sig == SIGALRM) {
        if (g_sigAlarmCallbackFn != nullptr) {
            g_sigAlarmCallbackFn();
        }
    } else {
        if (g_crashFdFn == nullptr) {
            CrashLocalHandler(&g_request);
        } else {
            int fd = g_crashFdFn(&g_request);
            CrashLocalHandlerFd(fd, &g_request);
            if (fd >= 0) {
                close(fd);
            }
        }
    }
    DFXLOGI("DoCrashHandler::finish handle sig(%d)", sig);
    UnregisterAllocator();
    pthread_mutex_unlock(&g_signalHandlerMutex);
    _exit(0);
    return 0;
}

void DFX_SignalLocalHandler(int sig, siginfo_t *si, void *context)
{
    pthread_mutex_lock(&g_signalHandlerMutex);
    if (sig != SIGALRM) {
        (void)memset_s(&g_request, sizeof(g_request), 0, sizeof(g_request));
        g_request.tid = gettid();
        g_request.pid = getpid();
        g_request.uid = FAULTLOGGERD_UID;
        g_request.timeStamp = GetTimeMilliseconds();
        DFXLOGI("DFX_SignalLocalHandler :: sig(%{public}d), pid(%{public}d), tid(%{public}d).",
            sig, g_request.pid, g_request.tid);

        GetThreadNameByTid(g_request.tid, g_request.threadName, sizeof(g_request.threadName));
        GetProcessName(g_request.processName, sizeof(g_request.processName));

        (void)memcpy_s(&(g_request.siginfo), sizeof(siginfo_t), si, sizeof(siginfo_t));
        (void)memcpy_s(&(g_request.context), sizeof(ucontext_t), context, sizeof(ucontext_t));
    }
#if defined(__aarch64__) || defined(__loongarch_lp64)
    DoCrashHandler(&sig);
#else
    int pseudothreadTid = -1;
    pid_t childTid = clone(DoCrashHandler, g_reservedChildStack, \
        CLONE_THREAD | CLONE_SIGHAND | CLONE_VM | CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID, \
        &sig, &pseudothreadTid, NULL, NULL, &pseudothreadTid);
    if (childTid == -1) {
        DFXLOGE("Failed to create thread for crash local handler");
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }

    FutexWait(&pseudothreadTid, -1);
    FutexWait(&pseudothreadTid, childTid);

    DFXLOGI("child thread(%{public}d) exit.", childTid);
    _exit(0);
#endif
}

void DFX_GetCrashFdFunc(CrashFdFunc fn)
{
    g_crashFdFn = fn;
}

void DFX_InstallLocalSignalHandler(void)
{
#if !defined(__aarch64__) && !defined(__loongarch_lp64)
    ReserveChildThreadSignalStack();
#endif

    sigset_t set;
    sigemptyset(&set);
    struct sigaction action;
    (void)memset_s(&action, sizeof(action), 0, sizeof(action));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DFX_SignalLocalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    for (auto sig : CATCHER_STACK_SIGNALS) {
        remove_all_special_handler(sig);
        sigaddset(&set, sig);
        if (sigaction(sig, &action, nullptr) != 0) {
            DFXLOGE("Failed to register signal(%{public}d)", sig);
        }
    }
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
}

void DFX_SetSigAlarmCallBack(SigAlarmFunc func)
{
    g_sigAlarmCallbackFn = func;
}