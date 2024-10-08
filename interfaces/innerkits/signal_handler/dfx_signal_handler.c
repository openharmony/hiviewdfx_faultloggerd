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

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sigchain.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_signalhandler_exception.h"
#include "errno.h"
#include "linux/capability.h"
#include "stdbool.h"
#include "string.h"
#ifndef DFX_SIGNAL_LIBC
#include <securec.h>
#include "dfx_cutil.h"
#include "dfx_log.h"
#else
#include "musl_cutil.h"
#include "musl_log.h"
#endif

#ifdef is_ohos_lite
#include "dfx_dumprequest.h"
#endif

#include "info/fatal_message.h"

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

#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif

#define NUMBER_SIXTYFOUR 64
#define INHERITABLE_OFFSET 32

#ifndef __MUSL__
void __attribute__((constructor)) InitHandler(void)
{
    DFX_InstallSignalHandler();
}
#endif

static struct ProcessDumpRequest g_request;
static void *g_reservedChildStack = NULL;
static pthread_mutex_t g_signalHandlerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t g_crashObjKey;
static bool g_crashObjInit = false;
static BOOL g_hasInit = FALSE;
static const int SIGNALHANDLER_TIMEOUT = 10000; // 10000 us
static const int ALARM_TIME_S = 10;
static int g_prevHandledSignal = SIGDUMP;
static struct sigaction g_oldSigactionList[NSIG] = {};
static char g_appRunningId[MAX_APP_RUNNING_UNIQUE_ID_LEN];
enum DumpPreparationStage {
    CREATE_PIPE_FAIL = 1,
    SET_PIPE_LEN_FAIL,
    WRITE_PIPE_FAIL,
    INHERIT_CAP_FAIL,
    EXEC_FAIL,
};

TraceInfo HiTraceGetId(void) __attribute__((weak));
static void FillTraceIdLocked(struct ProcessDumpRequest* request)
{
    if (HiTraceGetId == NULL || request == NULL) {
        return;
    }

    TraceInfo id = HiTraceGetId();
    memcpy(&(request->traceInfo), &id, sizeof(TraceInfo));
}

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

static GetStackIdFunc g_GetStackIdFunc = NULL;
void SetAsyncStackCallbackFunc(void* func)
{
    g_GetStackIdFunc = (GetStackIdFunc)func;
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

static void FillLastFatalMessageLocked(int32_t sig)
{
    if (sig != SIGABRT) {
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

    if (lastFatalMessage == NULL) {
        return;
    }

    size_t len = strlen(lastFatalMessage);
    if (len > MAX_FATAL_MSG_SIZE) {
        LOGERROR("Last message is longer than MAX_FATAL_MSG_SIZE");
        return;
    }

    g_request.msg.type = MESSAGE_FATAL;
    (void)strncpy(g_request.msg.body, lastFatalMessage, sizeof(g_request.msg.body) - 1);
}

static bool FillDebugMessageLocked(int32_t sig, siginfo_t *si)
{
    if (sig != SIGLEAK_STACK || si == NULL || si->si_signo != SIGLEAK_STACK || si->si_code != SIGLEAK_STACK_FDSAN) {
        return true;
    }
    debug_msg_t* dMsg = (debug_msg_t*)si->si_value.sival_ptr;
    if (dMsg == NULL || dMsg->msg == NULL) {
        return true;
    }

    if (g_request.timeStamp > dMsg->timestamp + PROCESSDUMP_TIMEOUT * NUMBER_ONE_THOUSAND) {
        LOGERROR("The event has timed out since it was triggered");
        return false;
    }

    g_request.msg.type = MESSAGE_FDSAN_DEBUG;
    (void)strncpy(g_request.msg.body, dMsg->msg, sizeof(g_request.msg.body) - 1);
    return true;
}

static const char* GetCrashDescription(const int32_t errCode)
{
    size_t i;

    for (i = 0; i < sizeof(g_crashExceptionMap) / sizeof(g_crashExceptionMap[0]); i++) {
        if (errCode == g_crashExceptionMap[i].errCode) {
            return g_crashExceptionMap[i].str;
        }
    }
    return g_crashExceptionMap[i - 1].str;    /* the end of map is "unknown reason" */
}

static void FillCrashExceptionAndReport(const int err)
{
    struct CrashDumpException exception;
    memset(&exception, 0, sizeof(struct CrashDumpException));
    exception.pid = g_request.pid;
    exception.uid = (int32_t)(g_request.uid);
    exception.error = err;
    exception.time = (int64_t)(GetTimeMilliseconds());
    (void)strncpy(exception.message, GetCrashDescription(err), sizeof(exception.message) - 1);
    ReportException(exception);
}

static bool IsDumpSignal(int sig)
{
    return sig == SIGDUMP || sig == SIGLEAK_STACK;
}

static bool FillDumpRequest(int sig, siginfo_t *si, void *context)
{
    memset(&g_request, 0, sizeof(g_request));
    g_request.type = sig;
    g_request.pid = GetRealPid();
    g_request.nsPid = syscall(SYS_getpid);
    g_request.tid = syscall(SYS_gettid);
    g_request.uid = getuid();
    g_request.reserved = 0;
    g_request.timeStamp = GetTimeMilliseconds();
    g_request.fdTableAddr = (uint64_t)fdsan_get_fd_table();
    memcpy(g_request.appRunningId, g_appRunningId, sizeof(g_request.appRunningId));
    if (!IsDumpSignal(sig) && g_GetStackIdFunc!= NULL) {
        g_request.stackId = g_GetStackIdFunc();
        LOGINFO("g_GetStackIdFunc %{public}p.", (void*)g_request.stackId);
    }

    GetThreadNameByTid(g_request.tid, g_request.threadName, sizeof(g_request.threadName));
    GetProcessName(g_request.processName, sizeof(g_request.processName));

    memcpy(&(g_request.siginfo), si, sizeof(siginfo_t));
    memcpy(&(g_request.context), context, sizeof(ucontext_t));

    FillTraceIdLocked(&g_request);
    bool ret = true;
    switch (sig) {
        case SIGABRT:
            FillLastFatalMessageLocked(sig);
            break;
        case SIGLEAK_STACK:
            ret = FillDebugMessageLocked(sig, si);
            /* fall-through */
        default: {
            ThreadInfoCallBack callback = GetCallbackLocked();
            if (callback != NULL) {
                LOGINFO("Start collect crash thread info.");
                g_request.msg.type = MESSAGE_FATAL;
                callback(g_request.msg.body, sizeof(g_request.msg.body), context);
                LOGINFO("Finish collect crash thread info.");
            }
            break;
        }
    }
    g_request.crashObj = (uintptr_t)pthread_getspecific(g_crashObjKey);

    return ret;
}

static const int SIGCHAIN_DUMP_SIGNAL_LIST[] = {
    SIGDUMP, SIGLEAK_STACK
};

static const int SIGCHAIN_CRASH_SIGNAL_LIST[] = {
    SIGILL, SIGABRT, SIGBUS, SIGFPE,
    SIGSEGV, SIGSTKFLT, SIGSYS, SIGTRAP
};

static bool IsMainThread(void)
{
    if (syscall(SYS_getpid) == 1) {
        if (syscall(SYS_gettid) == 1) {
            return true;
        }
    } else {
        if (syscall(SYS_getpid) == syscall(SYS_gettid)) {
            return true;
        }
    }
    return false;
}

static void ResetAndRethrowSignalIfNeed(int sig, siginfo_t *si)
{
    if (IsDumpSignal(sig)) {
        return;
    }

    if (g_oldSigactionList[sig].sa_sigaction == NULL) {
        signal(sig, SIG_DFL);
    } else if (sigaction(sig, &(g_oldSigactionList[sig]), NULL) != 0) {
        LOGERROR("Failed to reset sig(%{public}d).", sig);
        signal(sig, SIG_DFL);
    }

    if (syscall(SYS_rt_tgsigqueueinfo, syscall(SYS_getpid), syscall(SYS_gettid), sig, si) != 0) {
        LOGERROR("Failed to rethrow sig(%{public}d), errno(%{public}d).", sig, errno);
    } else {
        LOGINFO("Current process(%{public}ld) rethrow sig(%{public}d).", syscall(SYS_getpid), sig);
    }
}

static void PauseMainThreadHandler(int sig)
{
    LOGINFO("Crash(%{public}d) in child thread(%{public}ld), lock main thread.", sig, syscall(SYS_gettid));
    // only work when subthread crash and send SIGDUMP to mainthread.
    pthread_mutex_lock(&g_signalHandlerMutex);
    pthread_mutex_unlock(&g_signalHandlerMutex);
    LOGINFO("Crash in child thread(%{public}ld), exit main thread.", syscall(SYS_gettid));
}

static void BlockMainThreadIfNeed(int sig)
{
    if (IsMainThread() || IsDumpSignal(sig)) {
        return;
    }

    LOGINFO("Try block main thread.");
    (void)signal(SIGQUIT, PauseMainThreadHandler);
    if (syscall(SYS_tgkill, syscall(SYS_getpid), syscall(SYS_getpid), SIGQUIT) != 0) {
        LOGERROR("Failed to send SIGQUIT to main thread, errno(%{public}d).", errno);
    }
}

#ifndef is_ohos_lite
int DfxDumpRequest(int sig, struct ProcessDumpRequest *request, void *reservedChildStack) __attribute__((weak));
#endif
static int DumpRequest(int sig)
{
    int ret = false;
    if (DfxDumpRequest == NULL) {
        LOGERROR("DumpRequest fail, the DfxDumpRequest is NULL");
        FillCrashExceptionAndReport(CRASH_SIGNAL_EDUMPREQUEST);
        return ret;
    }
    ret = DfxDumpRequest(sig, &g_request, g_reservedChildStack);
    return ret;
}


static bool DFX_SigchainHandler(int sig, siginfo_t *si, void *context)
{
    int pid = syscall(SYS_getpid);
    int tid = syscall(SYS_gettid);

    LOGINFO("DFX_SigchainHandler :: sig(%{public}d), pid(%{public}d), tid(%{public}d).", sig, pid, tid);
    bool ret = false;
    if (sig == SIGDUMP) {
        if (si->si_code != DUMP_TYPE_REMOTE) {
            LOGWARN("DFX_SigchainHandler :: sig(%{public}d:%{public}d) is not remote dump type, return directly",
                sig, si->si_code);
            return true;
        }
    }

    // crash signal should never be skipped
    pthread_mutex_lock(&g_signalHandlerMutex);
    if (!IsDumpSignal(g_prevHandledSignal)) {
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return ret;
    }
    BlockMainThreadIfNeed(sig);
    g_prevHandledSignal = sig;

    if (!FillDumpRequest(sig, si, context)) {
        pthread_mutex_unlock(&g_signalHandlerMutex);
        LOGERROR("DFX_SigchainHandler :: signal(%{public}d) in %{public}d:%{public}d fill dump request faild.",
            sig, g_request.pid, g_request.tid);
        return ret;
    }
    LOGINFO("DFX_SigchainHandler :: sig(%{public}d), pid(%{public}d), processName(%{public}s), threadName(%{public}s).",
        sig, g_request.pid, g_request.processName, g_request.threadName);
    ret = DumpRequest(sig);
    pthread_mutex_unlock(&g_signalHandlerMutex);
    LOGINFO("Finish handle signal(%{public}d) in %{public}d:%{public}d.", sig, g_request.pid, g_request.tid);
    return ret;
}

static void DFX_SignalHandler(int sig, siginfo_t *si, void *context)
{
    DFX_SigchainHandler(sig, si, context);
    ResetAndRethrowSignalIfNeed(sig, si);
}

static void InstallSigActionHandler(int sig)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    memset(&g_oldSigactionList, 0, sizeof(g_oldSigactionList));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DFX_SignalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
    if (sigaction(sig, &action, &(g_oldSigactionList[sig])) != 0) {
        LOGERROR("Failed to register signal(%{public}d)", sig);
    }
}

void DFX_InstallSignalHandler(void)
{
    if (g_hasInit) {
        return;
    }

    InitCallbackItems();
    // reserve stack for fork
    g_reservedChildStack = mmap(NULL, RESERVED_CHILD_STACK_SIZE, \
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 1, 0);
    if (g_reservedChildStack == NULL) {
        LOGERROR("Failed to alloc memory for child stack.");
        return;
    }
    g_reservedChildStack = (void *)(((uint8_t *)g_reservedChildStack) + RESERVED_CHILD_STACK_SIZE - 1);

    struct signal_chain_action sigchain = {
        .sca_sigaction = DFX_SigchainHandler,
        .sca_mask = {},
        .sca_flags = 0,
    };

    for (size_t i = 0; i < sizeof(SIGCHAIN_DUMP_SIGNAL_LIST) / sizeof(SIGCHAIN_DUMP_SIGNAL_LIST[0]); i++) {
        int32_t sig = SIGCHAIN_DUMP_SIGNAL_LIST[i];
        sigfillset(&sigchain.sca_mask);
        // dump signal not mask crash signal
        for (size_t j = 0; j < sizeof(SIGCHAIN_CRASH_SIGNAL_LIST) / sizeof(SIGCHAIN_CRASH_SIGNAL_LIST[0]); j++) {
            sigdelset(&sigchain.sca_mask, SIGCHAIN_CRASH_SIGNAL_LIST[j]);
        }
        add_special_handler_at_last(sig, &sigchain);
    }
    for (size_t i = 0; i < sizeof(SIGCHAIN_CRASH_SIGNAL_LIST) / sizeof(SIGCHAIN_CRASH_SIGNAL_LIST[0]); i++) {
        int32_t sig = SIGCHAIN_CRASH_SIGNAL_LIST[i];
        if (sig == SIGILL || sig == SIGSYS) {
            InstallSigActionHandler(sig);
        } else {
            sigfillset(&sigchain.sca_mask);
            add_special_handler_at_last(sig, &sigchain);
        }
    }

    g_hasInit = TRUE;
    if (pthread_key_create(&g_crashObjKey, NULL) == 0) {
        g_crashObjInit = true;
    }
}

int DFX_SetAppRunningUniqueId(const char* appRunningId, size_t len)
{
    size_t appRunningIdMaxLen = sizeof(g_appRunningId);
    if (appRunningId == NULL || appRunningIdMaxLen <= len) {
        LOGERROR("param error. appRunningId is NULL or length overflow");
        return -1;
    }
    memset(g_appRunningId, 0, appRunningIdMaxLen);
    memcpy(g_appRunningId, appRunningId, len);
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
    if ((addr >> moveBit) != 0) {
        DFXLOGE("crash object address can not greater than 2^56, set crash object equal 0!");
    } else {
        crashObj = ((uintptr_t)type << moveBit) | addr;
    }
    pthread_setspecific(g_crashObjKey, (void*)(crashObj));
    return origin;
#endif
    return 0;
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