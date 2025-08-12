/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include "dfx_signal_handler.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sigchain.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <info/fatal_message.h>
#include <linux/capability.h>

#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_signalhandler_exception.h"
#include <securec.h>
#include "dfx_log.h"
#include "dfx_dumprequest.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSignalHandler"
#endif

#if defined (__LP64__)
#define RESERVED_CHILD_STACK_SIZE (32 * 1024)  // 32K
#else
#define RESERVED_CHILD_STACK_SIZE (16 * 1024)  // 16K
#endif

#define BOOL int
#define TRUE 1
#define FALSE 0

#ifndef NSIG
#define NSIG 64
#endif

static void DFX_InstallSignalHandler(void);
// preload by libc
void __attribute__((constructor)) InitHandler(void)
{
    DFX_InstallSignalHandler();
}

static struct ProcessDumpRequest g_request;
static pthread_mutex_t g_signalHandlerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t g_crashObjKey;
static uint64_t g_crashLogConfig = 0;
static bool g_crashObjInit = false;
static BOOL g_hasInit = FALSE;
static int g_prevHandledSignal = SIGDUMP;
static struct sigaction g_oldSigactionList[NSIG] = {};
static char g_appRunningId[MAX_APP_RUNNING_UNIQUE_ID_LEN];

const char* GetLastFatalMessage(void) __attribute__((weak));
fatal_msg_t *get_fatal_message(void) __attribute__((weak));

typedef struct ThreadCallbackItem {
    int32_t tid;
    ThreadInfoCallBack callback;
} ThreadCallbackItem;

#define CALLBACK_ITEM_COUNT 32
static ThreadCallbackItem g_callbackItems[CALLBACK_ITEM_COUNT];
static void InitCallbackItems(void)
{
    for (int i = 0; i < CALLBACK_ITEM_COUNT; i++) {
        g_callbackItems[i].tid = -1;
        g_callbackItems[i].callback = NULL;
    }
}

static GetStackIdFunc g_getStackIdCallback = NULL;
void DFX_SetAsyncStackCallback(GetStackIdFunc func)
{
    g_getStackIdCallback = func;
}

// caller should set to NULL before exit thread
void SetThreadInfoCallback(ThreadInfoCallBack func)
{
    int32_t currentTid = syscall(SYS_gettid);
    int32_t firstEmptySlot = -1;
    int32_t currentThreadSlot = -1;
    pthread_mutex_lock(&g_signalHandlerMutex);
    for (int i = 0; i < CALLBACK_ITEM_COUNT; i++) {
        if (firstEmptySlot == -1 && g_callbackItems[i].tid == -1) {
            firstEmptySlot = i;
        }

        if (g_callbackItems[i].tid == currentTid) {
            currentThreadSlot = i;
            break;
        }
    }

    int32_t targetSlot = currentThreadSlot == -1 ? firstEmptySlot : currentThreadSlot;
    if (targetSlot != -1) {
        g_callbackItems[targetSlot].tid = func == NULL ? -1 : currentTid;
        g_callbackItems[targetSlot].callback = func;
    }
    pthread_mutex_unlock(&g_signalHandlerMutex);
}

static ThreadInfoCallBack GetCallbackLocked()
{
    int32_t currentTid = syscall(SYS_gettid);
    for (int i = 0; i < CALLBACK_ITEM_COUNT; i++) {
        if (g_callbackItems[i].tid != currentTid) {
            continue;
        }

        return g_callbackItems[i].callback;
    }
    return NULL;
}

static void FillLastFatalMessageLocked(int32_t signo)
{
    if (signo != SIGABRT) {
        return;
    }

    const char* lastFatalMessage = NULL;
    if (get_fatal_message != NULL) {
        fatal_msg_t* fatalMsg = get_fatal_message();
        lastFatalMessage = fatalMsg == NULL ? NULL : fatalMsg->msg;
    }

    if (lastFatalMessage == NULL && GetLastFatalMessage != NULL) {
        lastFatalMessage = GetLastFatalMessage();
    }

    g_request.msg.type = MESSAGE_FATAL;
    if (strcpy_s(g_request.msg.body, sizeof(g_request.msg.body), lastFatalMessage) != EOK) {
        DFXLOGE("Last message strcpy fail");
    }
}

static bool FillDebugMessageLocked(int32_t signo, siginfo_t *si)
{
    if (signo != SIGLEAK_STACK || si == NULL ||
        (si->si_code != SIGLEAK_STACK_FDSAN && si->si_code != SIGLEAK_STACK_JEMALLOC)) {
        return true;
    }

    // The pointer received by the Linux kernel must be NULL
    debug_msg_t *dMsg = (debug_msg_t*)si->si_value.sival_ptr;
    if (dMsg == NULL || g_request.timeStamp > dMsg->timestamp + PROCESSDUMP_TIMEOUT * NUMBER_ONE_THOUSAND) {
        DFXLOGE("The event has timed out since it was triggered");
        return false;
    }
    return true;
}

static bool IsDumpSignal(int signo)
{
    return signo == SIGDUMP || signo == SIGLEAK_STACK;
}
static enum ProcessDumpType GetDumpType(int signo, siginfo_t *si)
{
    if (signo == SIGDUMP) {
        return DUMP_TYPE_DUMP_CATCH;
    } else if (signo == SIGLEAK_STACK) {
        switch (si->si_code) {
            case SIGLEAK_STACK_FDSAN:
                return DUMP_TYPE_FDSAN;
            case SIGLEAK_STACK_JEMALLOC:
                return DUMP_TYPE_JEMALLOC;
            case SIGLEAK_STACK_COREDUMP:
                return DUMP_TYPE_COREDUMP;
            case SIGLEAK_STACK_BADFD:
                return DUMP_TYPE_BADFD;
            default:
                return DUMP_TYPE_MEM_LEAK;
        }
    } else {
        return DUMP_TYPE_CPP_CRASH;
    }
}

static bool FillDumpRequest(int signo, siginfo_t *si, void *context)
{
    (void)memset_s(&g_request, sizeof(g_request), 0, sizeof(g_request));
    g_request.type = GetDumpType(signo, si);
    g_request.pid = GetRealPid();
    g_request.nsPid = syscall(SYS_getpid);
    g_request.tid = syscall(SYS_gettid);
    g_request.uid = getuid();
    g_request.reserved = 0;
    g_request.timeStamp = GetTimeMilliseconds();
    g_request.fdTableAddr = (uint64_t)fdsan_get_fd_table();
    if (memcpy_s(g_request.appRunningId, sizeof(g_request.appRunningId),
                 g_appRunningId, sizeof(g_appRunningId)) != EOK) {
        DFXLOGE("FillDumpRequest appRunningId memcpy fail!");
    }
    if (!IsDumpSignal(signo) && g_getStackIdCallback != NULL) {
        g_request.stackId = g_getStackIdCallback();
        DFXLOGI("g_GetStackIdFunc %{private}p.", (void*)g_request.stackId);
    }
    GetThreadNameByTid(g_request.tid, g_request.threadName, sizeof(g_request.threadName));
    GetProcessName(g_request.processName, sizeof(g_request.processName));
    (void)memcpy_s(&(g_request.siginfo), sizeof(siginfo_t), si, sizeof(siginfo_t));
    (void)memcpy_s(&(g_request.context), sizeof(ucontext_t), context, sizeof(ucontext_t));

    bool ret = true;
    switch (signo) {
        case SIGABRT:
            FillLastFatalMessageLocked(signo);
            break;
        case SIGLEAK_STACK:
            ret = FillDebugMessageLocked(signo, si);
            AT_FALLTHROUGH;
        default: {
            ThreadInfoCallBack callback = GetCallbackLocked();
            if (callback != NULL) {
                DFXLOGI("Start collect crash thread info.");
                g_request.msg.type = MESSAGE_CALLBACK;
                callback(g_request.msg.body, sizeof(g_request.msg.body), context);
                DFXLOGI("Finish collect crash thread info.");
            }
            break;
        }
    }
    g_request.crashObj = (uintptr_t)pthread_getspecific(g_crashObjKey);
    g_request.crashLogConfig = g_crashLogConfig;
    return ret;
}

static const int SIGCHAIN_DUMP_SIGNAL_LIST[] = {
    SIGDUMP, SIGLEAK_STACK
};

static const int SIGCHAIN_CRASH_SIGNAL_LIST[] = {
    SIGILL, SIGABRT, SIGBUS, SIGFPE,
    SIGSEGV, SIGSTKFLT, SIGSYS, SIGTRAP
};

static void ResetAndRethrowSignalIfNeed(int signo, siginfo_t *si)
{
    if (IsDumpSignal(signo)) {
        return;
    }

    if (g_oldSigactionList[signo].sa_sigaction == NULL) {
        signal(signo, SIG_DFL);
    } else if (sigaction(signo, &(g_oldSigactionList[signo]), NULL) != 0) {
        DFXLOGE("Failed to reset signo(%{public}d).", signo);
        signal(signo, SIG_DFL);
    }

    if (syscall(SYS_rt_tgsigqueueinfo, syscall(SYS_getpid), syscall(SYS_gettid), signo, si) != 0) {
        DFXLOGE("Failed to rethrow signo(%{public}d), errno(%{public}d).", signo, errno);
    } else {
        DFXLOGI("Current process(%{public}ld) rethrow signo(%{public}d).", syscall(SYS_getpid), signo);
    }
}

static void DumpRequest(int signo)
{
    DfxDumpRequest(signo, &g_request);
}

static bool DFX_SigchainHandler(int signo, siginfo_t *si, void *context)
{
    int pid = syscall(SYS_getpid);
    int tid = syscall(SYS_gettid);
    if (si == NULL) {
        return IsDumpSignal(signo);
    }

    DFXLOGI("DFX_SigchainHandler :: signo(%{public}d), si_code(%{public}d), pid(%{public}d), tid(%{public}d).",
            signo, si->si_code, pid, tid);
    if (signo == SIGDUMP) {
        if (si->si_code != DUMP_TYPE_REMOTE && si->si_code != DUMP_TYPE_REMOTE_JSON) {
            DFXLOGW("DFX_SigchainHandler :: signo(%{public}d:%{public}d) is not remote dump type, return directly",
                signo, si->si_code);
            return IsDumpSignal(signo);
        }
    }

    // crash signal should never be skipped
    pthread_mutex_lock(&g_signalHandlerMutex);
    if (!IsDumpSignal(g_prevHandledSignal)) {
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return IsDumpSignal(signo);
    }
    g_prevHandledSignal = signo;

    if (!FillDumpRequest(signo, si, context)) {
        pthread_mutex_unlock(&g_signalHandlerMutex);
        DFXLOGE("DFX_SigchainHandler :: signal(%{public}d) in %{public}d:%{public}d fill dump request faild.",
            signo, g_request.pid, g_request.tid);
        return IsDumpSignal(signo);
    }
    DFXLOGI("DFX_SigchainHandler :: signo(%{public}d), pid(%{public}d), processName(%{public}s), " \
        "threadName(%{public}s).", signo, g_request.pid, g_request.processName, g_request.threadName);
    DumpRequest(signo);
    pthread_mutex_unlock(&g_signalHandlerMutex);
    DFXLOGI("Finish handle signal(%{public}d) in %{public}d:%{public}d.", signo, g_request.pid, g_request.tid);
    return IsDumpSignal(signo);
}

static void DFX_SignalHandler(int signo, siginfo_t *si, void *context)
{
    DFX_SigchainHandler(signo, si, context);
    ResetAndRethrowSignalIfNeed(signo, si);
}

static void InstallSigActionHandler(int signo)
{
    struct sigaction action;
    (void)memset_s(&action, sizeof(action), 0, sizeof(action));
    (void)memset_s(&g_oldSigactionList, sizeof(g_oldSigactionList), 0, sizeof(g_oldSigactionList));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DFX_SignalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
    if (sigaction(signo, &action, &(g_oldSigactionList[signo])) != 0) {
        DFXLOGE("Failed to register signal(%{public}d)", signo);
    }
}

static void DFX_InstallSignalHandler(void)
{
    if (g_hasInit) {
        return;
    }

    InitCallbackItems();
    struct signal_chain_action sigchain = {
        .sca_sigaction = DFX_SigchainHandler,
        .sca_mask = {},
        .sca_flags = 0,
    };

    for (size_t i = 0; i < sizeof(SIGCHAIN_DUMP_SIGNAL_LIST) / sizeof(SIGCHAIN_DUMP_SIGNAL_LIST[0]); i++) {
        int32_t signo = SIGCHAIN_DUMP_SIGNAL_LIST[i];
        if (signo == SIGLEAK_STACK) {
            InstallSigActionHandler(signo);
            continue;
        }
        sigfillset(&sigchain.sca_mask);
        // dump signal not mask crash signal
        for (size_t j = 0; j < sizeof(SIGCHAIN_CRASH_SIGNAL_LIST) / sizeof(SIGCHAIN_CRASH_SIGNAL_LIST[0]); j++) {
            sigdelset(&sigchain.sca_mask, SIGCHAIN_CRASH_SIGNAL_LIST[j]);
        }
        add_special_handler_at_last(signo, &sigchain);
    }
    for (size_t i = 0; i < sizeof(SIGCHAIN_CRASH_SIGNAL_LIST) / sizeof(SIGCHAIN_CRASH_SIGNAL_LIST[0]); i++) {
        int32_t signo = SIGCHAIN_CRASH_SIGNAL_LIST[i];
        if (signo == SIGILL || signo == SIGSYS) {
            InstallSigActionHandler(signo);
        } else {
            sigfillset(&sigchain.sca_mask);
            add_special_handler_at_last(signo, &sigchain);
        }
    }

    g_hasInit = TRUE;
    if (pthread_key_create(&g_crashObjKey, NULL) == 0) {
        g_crashObjInit = true;
    }
}

const char* DFX_GetAppRunningUniqueId(void)
{
    return g_appRunningId;
}

int DFX_SetAppRunningUniqueId(const char* appRunningId, size_t len)
{
    (void)memset_s(g_appRunningId, sizeof(g_appRunningId), 0, sizeof(g_appRunningId));
    if (memcpy_s(g_appRunningId, sizeof(g_appRunningId) - 1, appRunningId, len) != EOK) {
        DFXLOGE("param error. appRunningId is NULL or length overflow");
        return -1;
    }
    return 0;
}

uintptr_t DFX_SetCrashObj(uint8_t type, uintptr_t addr)
{
    if (!g_crashObjInit) {
        return 0;
    }
#if defined __LP64__
    uintptr_t origin = (uintptr_t)pthread_getspecific(g_crashObjKey);
    uintptr_t crashObj = 0;
    const int moveBit = 56;
    crashObj = ((uintptr_t)type << moveBit) | (addr & 0x00ffffffffffffff);
    pthread_setspecific(g_crashObjKey, (void*)(crashObj));
    return origin;
#else
    return 0;
#endif
}

void DFX_ResetCrashObj(uintptr_t crashObj)
{
    if (!g_crashObjInit) {
        return;
    }
#if defined __LP64__
    pthread_setspecific(g_crashObjKey, (void*)(crashObj));
#endif
}

int DFX_SetCrashLogConfig(uint8_t type, uint32_t value)
{
    if (!g_hasInit) {
        errno = EPERM;
        return -1;
    }
    if ((type == EXTEND_PRINT_PC_LR || type == SIMPLIFY_PRINT_MAPS) && value > 1) {
        DFXLOGE("invalid value %{public}u", value);
        errno = EINVAL;
        return -1;
    }
    const uint64_t extendPrintPcLrMask = 0xfffffffffffffffe;
    const int moveBit = 32;
    const uint64_t cutOffLogMask = 0xffffffff;
    const uint64_t simplifyVmaMask = 0xfffffffffffffffd;
    switch (type) {
        case EXTEND_PRINT_PC_LR:
            g_crashLogConfig = (g_crashLogConfig & extendPrintPcLrMask) + value;
            break;
        case CUT_OFF_LOG_FILE:
            g_crashLogConfig = ((uint64_t)value << moveBit) + (g_crashLogConfig & cutOffLogMask);
            break;
        case SIMPLIFY_PRINT_MAPS:
            g_crashLogConfig = (g_crashLogConfig & simplifyVmaMask) + (value << 1);
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    return 0;
}
