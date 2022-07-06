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

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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

#include <securec.h>

#include "dfx_log.h"
#include "dfx_cutil.h"
#if defined(CRASH_LOCAL_HANDLER)
#include "dfx_crash_local_handler.h"
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

#ifndef CLONE_VFORK
#define CLONE_VFORK 0x00004000
#endif
#ifndef CLONE_FS
#define CLONE_FS 0x00000200
#endif
#ifndef CLONE_UNTRACED
#define CLONE_UNTRACED 0x00800000
#endif

#define NUMBER_SIXTYFOUR 64
#define INHERITABLE_OFFSET 32

#define OHOS_TEMP_FAILURE_RETRY(exp)            \
    ({                                     \
    long int _rc;                          \
    do {                                   \
        _rc = (long int)(exp);             \
    } while ((_rc == -1) && (errno == EINTR)); \
    _rc;                                   \
    })

void __attribute__((constructor)) InitHandler(void)
{
    DFX_InstallSignalHandler();
}

static struct ProcessDumpRequest g_request;
static void *g_reservedChildStack;
static pthread_mutex_t g_signalHandlerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_dumpMutex = PTHREAD_MUTEX_INITIALIZER;
static int g_pipefd[2] = {-1, -1};
static BOOL g_hasInit = FALSE;
static const int SIGNALHANDLER_TIMEOUT = 10000; // 10000 us
static int g_lastHandledTid[MAX_HANDLED_TID_NUMBER] = {0};
static int g_lastHandledTidIndex = 0;
static const int ALARM_TIME_S = 10;
static int g_curSig = -1;

enum DumpPreparationStage {
    CREATE_PIPE_FAIL = 1,
    SET_PIPE_LEN_FAIL,
    WRITE_PIPE_FAIL,
    INHERIT_CAP_FAIL,
    EXEC_FAIL,
};

const char* GetLastFatalMessage(void) __attribute__((weak));

void SetPlatformSignalHandler(uintptr_t handler)  __attribute__((weak));

static void FillLastFatalMessageLocked(int32_t sig)
{
    if (sig != SIGABRT) {
        return;
    }

    if (GetLastFatalMessage == NULL) {
        DfxLogError("Could not find GetLastFatalMessage func");
        return;
    }

    const char* lastFatalMessage = GetLastFatalMessage();
    if (lastFatalMessage == NULL) {
        DfxLogError("Could not find last message");
        return;
    }

    size_t len = strlen(lastFatalMessage);
    if (len > MAX_FATAL_MSG_SIZE) {
        DfxLogError("Last message is longer than MAX_FATAL_MSG_SIZE");
        return;
    }

    (void)strncpy_s(g_request.lastFatalMessage, sizeof(g_request.lastFatalMessage),
        lastFatalMessage, sizeof(g_request.lastFatalMessage) - 1);
}

static int32_t InheritCapabilities(void)
{
    struct __user_cap_header_struct capHeader;
    if (memset_s(&capHeader, sizeof(capHeader), 0, sizeof(capHeader)) != EOK) {
        DfxLogError("Failed to memset cap header.");
        return -1;
    }

    capHeader.version = _LINUX_CAPABILITY_VERSION_3;
    capHeader.pid = 0;
    struct __user_cap_data_struct capData[2];
    if (capget(&capHeader, &capData[0]) == -1) {
        DfxLogError("Failed to get origin cap data");
        return -1;
    }

    capData[0].inheritable = capData[0].permitted;
    capData[1].inheritable = capData[1].permitted;
    if (capset(&capHeader, &capData[0]) == -1) {
        DfxLogError("Failed to set cap data");
        return -1;
    }

    uint64_t ambCap = capData[0].inheritable;
    ambCap = ambCap | (((uint64_t)capData[1].inheritable) << INHERITABLE_OFFSET);
    for (size_t i = 0; i < NUMBER_SIXTYFOUR; i++) {
        if (ambCap & ((uint64_t)1)) {
            (void)prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, i, 0, 0);
        }
        ambCap = ambCap >> 1;
    }
    return 0;
}

static int g_interestedSignalList[] = {
    SIGABRT,
    SIGBUS,
    SIGDUMP,
    SIGFPE,
    SIGILL,
    SIGSEGV,
    SIGSTKFLT,
    SIGSYS,
    SIGTRAP,
};

static struct sigaction g_oldSigactionList[NSIG] = {};

static void SetInterestedSignalMasks(int how)
{
    sigset_t set;
    sigemptyset (&set);
    for (size_t i = 0; i < sizeof(g_interestedSignalList) / sizeof(g_interestedSignalList[0]); i++) {
        sigaddset(&set, g_interestedSignalList[i]);
    }
    sigprocmask(how, &set, NULL);
}

static void DFX_SetUpEnvironment()
{
    // avoiding fd exhaust
    const int closeFdCount = 1024;
    for (int i = 0; i < closeFdCount; i++) {
        syscall(SYS_close, i);
    }
    // clear stdout and stderr
    int devNull = OHOS_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if (devNull < 0) {
        DfxLogError("Failed to open dev/null.");
        return;
    }

    OHOS_TEMP_FAILURE_RETRY(dup2(devNull, STDOUT_FILENO));
    OHOS_TEMP_FAILURE_RETRY(dup2(devNull, STDERR_FILENO));
    syscall(SYS_close, devNull);
    SetInterestedSignalMasks(SIG_BLOCK);
}

static void DFX_SetUpSignalAction(void)
{
    if (signal(SIGALRM, SIG_DFL) == SIG_ERR ||
        signal(SIGSTOP, SIG_DFL) == SIG_ERR) {
        DfxLogError("signal error!");
    }
    sigset_t set;
    sigemptyset (&set);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGSTOP);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

static int DFX_ExecDump(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_dumpMutex);
    DFX_SetUpEnvironment();
    DFX_SetUpSignalAction();
    alarm(ALARM_TIME_S);
    // create pipe for passing request to processdump
    if (pipe(g_pipefd) != 0) {
        DfxLogError("Failed to create pipe for transfering context.");
        pthread_mutex_unlock(&g_dumpMutex);
        return CREATE_PIPE_FAIL;
    }
    ssize_t writeLen = (long)(sizeof(struct ProcessDumpRequest));
    if (fcntl(g_pipefd[1], F_SETPIPE_SZ, writeLen) < writeLen) {
        DfxLogError("Failed to set pipe buffer size.");
        pthread_mutex_unlock(&g_dumpMutex);
        return SET_PIPE_LEN_FAIL;
    }

    struct iovec iovs[1] = {
        {
            .iov_base = &g_request,
            .iov_len = sizeof(struct ProcessDumpRequest)
        },
    };
    ssize_t realWriteSize = OHOS_TEMP_FAILURE_RETRY(writev(g_pipefd[1], iovs, 1));
    if ((ssize_t)writeLen != realWriteSize) {
        DfxLogError("Failed to write pipe.");
        pthread_mutex_unlock(&g_dumpMutex);
        return WRITE_PIPE_FAIL;
    }
    OHOS_TEMP_FAILURE_RETRY(dup2(g_pipefd[0], STDIN_FILENO));
    if (g_pipefd[0] != STDIN_FILENO) {
        syscall(SYS_close, g_pipefd[0]);
    }
    syscall(SYS_close, g_pipefd[1]);

    if (InheritCapabilities() != 0) {
        DfxLogError("Failed to inherit Capabilities from parent.");
        pthread_mutex_unlock(&g_dumpMutex);
        return INHERIT_CAP_FAIL;
    }

    DfxLogInfo("execle processdump.");
#ifdef DFX_LOG_USE_HILOG_BASE
    execle("/system/bin/processdump", "processdump", "-signalhandler", NULL, NULL);
#else
    execle("/bin/processdump", "processdump", "-signalhandler", NULL, NULL);
#endif
    pthread_mutex_unlock(&g_dumpMutex);
    return errno;
}

static pid_t DFX_ForkAndDump()
{
    if (NULL == g_reservedChildStack) {
        DfxLogError("g_reservedChildStack is null.");
        return -1;
    }
    return clone(DFX_ExecDump, g_reservedChildStack, CLONE_VFORK | CLONE_FS | CLONE_UNTRACED, NULL);
}

static void ResetSignalHandlerIfNeed(int sig)
{
    if (sig == SIGDUMP) {
        return;
    }

    if (g_oldSigactionList[sig].sa_sigaction == NULL) {
        signal(sig, SIG_DFL);
        return;
    }

    if (sigaction(sig, &(g_oldSigactionList[sig]), NULL) != 0) {
        DfxLogError("Failed to reset signal.");
        signal(sig, SIG_DFL);
    }
}

static int CheckLastHandledTid(int sig, siginfo_t *si)
{
    for (int i = 0; i < g_lastHandledTidIndex && i < MAX_HANDLED_TID_NUMBER; i++) {
        if (g_lastHandledTid[i] == gettid()) {
            ResetSignalHandlerIfNeed(sig);
            DfxLogInfo("Just resend sig(%d), pid(%d), tid(%d) to sys.", sig, getpid(), gettid());
            if (syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), si->si_signo, si) != 0) {
                DfxLogError("Failed to resend signal.");
            }
            return TRUE;
        }
    }
    return FALSE;
}

static void DFX_SignalHandler(int sig, siginfo_t *si, void *context)
{
    if (sig != SIGDUMP) {
        if (CheckLastHandledTid(sig, si) == TRUE) {
            return;
        }
    } else if (g_curSig == sig) {
        DfxLogInfo("We are handling sigdump now, skip same request.");
        return;
    }
    g_curSig = sig;
    
    pthread_mutex_lock(&g_signalHandlerMutex);
    (void)memset_s(&g_request, sizeof(g_request), 0, sizeof(g_request));
    g_request.type = sig;
    g_request.tid = gettid();
    g_request.pid = getpid();
    g_request.uid = getuid();
    g_request.reserved = 0;
    g_request.timeStamp = GetTimeMilliseconds();
    DfxLogInfo("DFX_SignalHandler :: sig(%d), pid(%d), tid(%d).", sig, g_request.pid, g_request.tid);

    GetThreadName(g_request.threadName, sizeof(g_request.threadName));
    GetProcessName(g_request.processName, sizeof(g_request.processName));

    if (sig != SIGDUMP && g_lastHandledTidIndex < MAX_HANDLED_TID_NUMBER) {
        g_lastHandledTid[g_lastHandledTidIndex] = g_request.tid;
        g_lastHandledTidIndex = g_lastHandledTidIndex + 1;
    }

    if (memcpy_s(&(g_request.siginfo), sizeof(g_request.siginfo),
        si, sizeof(siginfo_t)) != 0) {
        DfxLogError("Failed to copy siginfo.");
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }
    if (memcpy_s(&(g_request.context), sizeof(g_request.context),
        context, sizeof(ucontext_t)) != 0) {
        DfxLogError("Failed to copy ucontext.");
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }
    FillLastFatalMessageLocked(sig);
    pid_t childPid;
    int status;
    int ret = -1;
    int timeout = 0;
    int startTime = (int)time(NULL);
    // set privilege for dump ourself
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    BOOL isTracerStatusModified = FALSE;
    if (prctl(PR_SET_DUMPABLE, 1) != 0) {
        DfxLogError("Failed to set dumpable.");
        goto out;
    }
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
        if (errno != EINVAL) {
            DfxLogError("Failed to set ptracer.");
            goto out;
        }
    } else {
        isTracerStatusModified = TRUE;
    }
    // fork a child process that could ptrace us
    childPid = DFX_ForkAndDump();
    if (childPid < 0) {
        DfxLogError("Failed to fork child process, errno(%d).", errno);
        goto out;
    }

    do {
        errno = 0;
        ret = waitpid(childPid, &status, WNOHANG);
        if (ret < 0) {
            DfxLogError("Failed to wait child process terminated, errno(%d)", errno);
            goto out;
        }

        if (ret == childPid) {
            break;
        }

        if ((int)time(NULL) - startTime > PROCESSDUMP_TIMEOUT) {
            timeout = 1;
            DfxLogError("Exceed max wait time, errno(%d)", errno);
            goto out;
        }
        usleep(SIGNALHANDLER_TIMEOUT); // sleep 10ms
    } while (1);
    DfxLogInfo("waitpid for process(%d) return with ret(%d) status(%d)", childPid, ret, status);
out:
#if defined(CRASH_LOCAL_HANDLER)
    if ((sig != SIGDUMP) && ((timeout == 1) || ((ret >= 0) && (status != 0)))) {
        CrashLocalHandler(&g_request, si, context);
    }
#endif
    ResetSignalHandlerIfNeed(sig);
    prctl(PR_SET_DUMPABLE, prevDumpableStatus);
    if (isTracerStatusModified == TRUE) {
        prctl(PR_SET_PTRACER, 0);
    }

    if (sig != SIGDUMP) {
        if (syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), si->si_signo, si) != 0) {
            DfxLogError("Failed to resend signal.");
        }
    }

    DfxLogInfo("Finish handle signal(%d) in %d:%d", sig, g_request.pid, g_request.tid);
    g_curSig = -1;
    pthread_mutex_unlock(&g_signalHandlerMutex);
}

void DFX_InstallSignalHandler(void)
{
    if (g_hasInit) {
        return;
    }

    // reserve stack for fork
    g_reservedChildStack = mmap(NULL, RESERVED_CHILD_STACK_SIZE, \
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 1, 0);
    if (g_reservedChildStack == NULL) {
        DfxLogError("Failed to alloc memory for child stack.");
        return;
    }
    g_reservedChildStack = (void *)(((uint8_t *)g_reservedChildStack) + RESERVED_CHILD_STACK_SIZE - 1);

    if (SetPlatformSignalHandler != NULL) {
        SetPlatformSignalHandler((uintptr_t)DFX_SignalHandler);
    }

    struct sigaction action;
    memset_s(&action, sizeof(action), 0, sizeof(action));
    memset_s(&g_oldSigactionList, sizeof(g_oldSigactionList), 0, sizeof(g_oldSigactionList));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DFX_SignalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    for (size_t i = 0; i < sizeof(g_interestedSignalList) / sizeof(g_interestedSignalList[0]); i++) {
        int32_t sig = g_interestedSignalList[i];
        if (sigaction(sig, &action, &(g_oldSigactionList[sig])) != 0) {
            DfxLogError("Failed to register signal.");
        }
    }

    g_hasInit = TRUE;
}
