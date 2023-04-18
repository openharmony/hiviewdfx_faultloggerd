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
#include <pthread.h>
#include <sched.h>
#include <signal.h>
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
static struct ProcInfo g_procInfo;
static void *g_reservedChildStack = NULL;
static pthread_mutex_t g_signalHandlerMutex = PTHREAD_MUTEX_INITIALIZER;
static int g_pipefd[2] = {-1, -1};
static BOOL g_hasInit = FALSE;
static const int SIGNALHANDLER_TIMEOUT = 10000; // 10000 us
static const int ALARM_TIME_S = 10;
static int g_prevHandledSignal = SIGDUMP;
static BOOL g_isDumping = FALSE;
static struct sigaction g_oldSigactionList[NSIG] = {};

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
void SetThreadInfoCallback(ThreadInfoCallBack func)
{
    threadInfoCallBack = func;
}
static void FillLastFatalMessageLocked(int32_t sig)
{
    if (sig != SIGABRT && threadInfoCallBack != NULL) {
        threadInfoCallBack(g_request.lastFatalMessage, sizeof(g_request.lastFatalMessage));
        return;
    }

    if (GetLastFatalMessage == NULL) {
        DFXLOG_ERROR("Could not find GetLastFatalMessage func");
        return;
    }

    const char* lastFatalMessage = GetLastFatalMessage();
    if (lastFatalMessage == NULL) {
        DFXLOG_ERROR("Could not find last message");
        return;
    }

    size_t len = strlen(lastFatalMessage);
    if (len > MAX_FATAL_MSG_SIZE) {
        DFXLOG_ERROR("Last message is longer than MAX_FATAL_MSG_SIZE");
        return;
    }

    (void)strncpy(g_request.lastFatalMessage, lastFatalMessage, sizeof(g_request.lastFatalMessage) - 1);
}

static int32_t InheritCapabilities(void)
{
    struct __user_cap_header_struct capHeader;
    memset(&capHeader, 0, sizeof(capHeader));

    capHeader.version = _LINUX_CAPABILITY_VERSION_3;
    capHeader.pid = 0;
    struct __user_cap_data_struct capData[2];
    if (capget(&capHeader, &capData[0]) == -1) {
        DFXLOG_ERROR("Failed to get origin cap data");
        return -1;
    }

    capData[0].inheritable = capData[0].permitted;
    capData[1].inheritable = capData[1].permitted;
    if (capset(&capHeader, &capData[0]) == -1) {
        DFXLOG_ERROR("Failed to set cap data");
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

static const int g_interestedSignalList[] = {
    SIGABRT, SIGBUS, SIGDUMP, SIGFPE, SIGILL,
    SIGSEGV, SIGSTKFLT, SIGSYS, SIGTRAP,
};

static void SetInterestedSignalMasks(int how)
{
    sigset_t set;
    sigemptyset(&set);
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
        DFXLOG_ERROR("Failed to open dev/null.");
        return;
    }

    OHOS_TEMP_FAILURE_RETRY(dup2(devNull, STDOUT_FILENO));
    OHOS_TEMP_FAILURE_RETRY(dup2(devNull, STDERR_FILENO));
    syscall(SYS_close, devNull);
    SetInterestedSignalMasks(SIG_BLOCK);
}

static void DFX_SetUpSigAlarmAction(void)
{
    if (signal(SIGALRM, SIG_DFL) == SIG_ERR) {
        DFXLOG_ERROR("signal error!");
    }
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

static int DFX_ExecDump(void *arg)
{
    (void)arg;
    DFX_SetUpEnvironment();
    DFX_SetUpSigAlarmAction();
    alarm(ALARM_TIME_S);
    // create pipe for passing request to processdump
    if (pipe(g_pipefd) != 0) {
        DFXLOG_ERROR("Failed to create pipe for transfering context.");
        return CREATE_PIPE_FAIL;
    }
    ssize_t writeLen = (long)(sizeof(struct ProcessDumpRequest));
    if (fcntl(g_pipefd[1], F_SETPIPE_SZ, writeLen) < writeLen) {
        DFXLOG_ERROR("Failed to set pipe buffer size.");
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
        DFXLOG_ERROR("Failed to write pipe.");
        return WRITE_PIPE_FAIL;
    }
    OHOS_TEMP_FAILURE_RETRY(dup2(g_pipefd[0], STDIN_FILENO));
    if (g_pipefd[0] != STDIN_FILENO) {
        syscall(SYS_close, g_pipefd[0]);
    }
    syscall(SYS_close, g_pipefd[1]);

    if (InheritCapabilities() != 0) {
        DFXLOG_ERROR("Failed to inherit Capabilities from parent.");
        return INHERIT_CAP_FAIL;
    }
    DFXLOG_INFO("execl processdump.");
#ifdef DFX_LOG_USE_HILOG_BASE
    execl("/system/bin/processdump", "processdump", "-signalhandler", NULL);
#else
    execl("/bin/processdump", "processdump", "-signalhandler", NULL);
#endif
    DFXLOG_ERROR("Failed to execl processdump, errno: %d(%s)", errno, strerror(errno));
    return errno;
}

static pid_t GetPid(void)
{
    return g_procInfo.pid;
}

static pid_t GetTid(void)
{
    return g_procInfo.tid;
}

static bool IsMainThread(void)
{
    if (g_procInfo.ns) {
        if (GetTid() == 1) {
            return true;
        }
    } else {
        if (GetPid() == GetTid()) {
            return true;
        }
    }
    return false;
}

static void ResetSignalHandlerIfNeed(int sig)
{
    if (sig == SIGDUMP) {
        return;
    }

    if (g_oldSigactionList[sig].sa_sigaction == NULL) {
        signal(sig, SIG_DFL);
        // real init will not handle crash signal
        // crash in pid namespace may not exit even if rethrow the signal
        // use exit instead
        if (GetPid() == 1) {
            DFXLOG_ERROR("Sandbox process is about to exit with signal %d.", sig);
            _exit(sig);
        }
        return;
    }

    if (sigaction(sig, &(g_oldSigactionList[sig]), NULL) != 0) {
        DFXLOG_ERROR("Failed to reset signal.");
        signal(sig, SIG_DFL);
    }
}

static void PauseMainThreadHandler(int sig)
{
    DFXLOG_INFO("Crash(%d) in child thread(%d), lock main thread.", sig, GetTid());
    // only work when subthread crash and send SIGDUMP to mainthread.
    pthread_mutex_lock(&g_signalHandlerMutex);
    pthread_mutex_unlock(&g_signalHandlerMutex);
    DFXLOG_INFO("Crash in child thread(%d), exit main thread.", GetTid());
}

static void BlockMainThreadIfNeed(int sig)
{
    if (IsMainThread() || (sig == SIGDUMP)) {
        return;
    }

    DFXLOG_INFO("Crash(%d) in child thread(%d), try stop main thread.", sig, GetTid());
    (void)signal(SIGDUMP, PauseMainThreadHandler);
    if (syscall(SYS_tgkill, GetPid(), GetPid(), SIGDUMP) != 0) {
        DFXLOG_ERROR("Failed to send SIGDUMP to main thread, errno(%d).", errno);
    }
}

static pid_t ForkBySyscall(void)
{
#ifdef SYS_fork
    return syscall(SYS_fork);
#else
    return syscall(SYS_clone, SIGCHLD, 0);
#endif
}

static bool SetDumpState(void)
{
    if (prctl(PR_SET_DUMPABLE, 1) != 0) {
        DFXLOG_ERROR("Failed to set dumpable.");
        return false;
    }

    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
        if (errno != EINVAL) {
            DFXLOG_ERROR("Failed to set ptracer.");
            return false;
        }
    }
    return true;
}

static void RestoreDumpState(int prevState, bool isTracerStatusModified)
{
    prctl(PR_SET_DUMPABLE, prevState);
    if (isTracerStatusModified == true) {
        prctl(PR_SET_PTRACER, 0);
    }
}

static int DoProcessDump(void* arg)
{
    (void)arg;
    int status;
    int ret = -1;
    int childPid = -1;
    int startTime = (int)time(NULL);
    bool isTimeout = false;
    g_request.recycleTid = syscall(SYS_gettid);
    pthread_setname_np(pthread_self(), "dump_tmp_thread");
    struct sched_param schedParam;
    schedParam.sched_priority = 0;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &schedParam);
    // set privilege for dump ourself
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    bool isTracerStatusModified = SetDumpState();
    if (!isTracerStatusModified) {
        goto out;
    }

    // fork a child process that could ptrace us
    childPid = ForkBySyscall();
    if (childPid == 0) {
        _exit(DFX_ExecDump(NULL));
    } else if (childPid < 0) {
        DFXLOG_ERROR("Failed to fork child process, errno(%d).", errno);
        goto out;
    }

    do {
        errno = 0;
        ret = waitpid(childPid, &status, WNOHANG);
        if (ret < 0) {
            DFXLOG_ERROR("Failed to wait child process terminated, errno(%d)", errno);
            goto out;
        }

        if (ret == childPid) {
            break;
        }

        if ((int)time(NULL) - startTime > PROCESSDUMP_TIMEOUT) {
            isTimeout = true;
            break;
        }
        usleep(SIGNALHANDLER_TIMEOUT); // sleep 10ms
    } while (1);
    DFXLOG_INFO("(%d) wait for process(%d) return with ret(%d) status(%d) timeout(%d)",
        g_request.recycleTid, childPid, ret, status, isTimeout);
out:
    g_isDumping = FALSE;
    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
    pthread_mutex_unlock(&g_signalHandlerMutex);
    return 0;
}

static void UnlockSignalHandlerMutex(void)
{
    if (pthread_mutex_trylock(&g_signalHandlerMutex) == 0) {
        pthread_mutex_unlock(&g_signalHandlerMutex);
    } else {
        pthread_mutex_unlock(&g_signalHandlerMutex);
    }
}

static void ForkAndDoProcessDump(void)
{
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    bool isTracerStatusModified = SetDumpState();
    int childPid = ForkBySyscall();
    if (childPid == 0) {
        g_request.vmPid = syscall(SYS_getpid);
        DFXLOG_INFO("Start DoProcessDump in VmProcess(%d).", g_request.vmPid);
        alarm(ALARM_TIME_S);
        _exit(DoProcessDump(NULL));
    } else if (childPid < 0) {
        DFXLOG_ERROR("Failed to vfork child process, errno(%d).", errno);
        RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
        DoProcessDump(NULL);
        return;
    }

    DFXLOG_INFO("Start wait for VmProcess(%d) exit.", childPid);
    errno = 0;
    int status;
    int ret = waitpid(childPid, &status, 0);
    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
    DFXLOG_INFO("(%d) wait for VmProcess(%d) return with ret(%d) status(%d)",
        getpid(), childPid, ret, status);
}

static void DFX_SignalHandler(int sig, siginfo_t *si, void *context)
{
    if (sig == SIGDUMP && g_isDumping) {
        DFXLOG_WARN("Current Process is dumping stacktrace now.");
        return;
    }

    // crash signal should never be skipped
    pthread_mutex_lock(&g_signalHandlerMutex);
    memset(&g_procInfo, 0, sizeof(g_procInfo));
    GetProcStatus(&g_procInfo);
    BlockMainThreadIfNeed(sig);
    if (g_prevHandledSignal != SIGDUMP) {
        ResetSignalHandlerIfNeed(sig);
        if (syscall(SYS_rt_tgsigqueueinfo, GetPid(), GetTid(), si->si_signo, si) != 0) {
            DFXLOG_ERROR("Failed to resend signal(%d), errno(%d).", si->si_signo, errno);
        } else {
            DFXLOG_INFO("Current process has encount a crash, rethrow sig(%d).", si->si_signo);
        }
        return;
    }
    g_prevHandledSignal = sig;
    g_isDumping = TRUE;
    memset(&g_request, 0, sizeof(g_request));
    g_request.type = sig;
    g_request.pid = GetPid();
    g_request.tid = syscall(SYS_gettid);
    g_request.uid = getuid();
    g_request.reserved = 0;
    g_request.timeStamp = GetTimeMilliseconds();
    DFXLOG_INFO("DFX_SignalHandler :: sig(%d), pid(%d), tid(%d).", sig, g_request.pid, g_request.tid);

    GetThreadName(g_request.threadName, sizeof(g_request.threadName));
    GetProcessName(g_request.processName, sizeof(g_request.processName));

    memcpy(&(g_request.siginfo), si, sizeof(siginfo_t));
    memcpy(&(g_request.context), context, sizeof(ucontext_t));

    FillTraceIdLocked(&g_request);
    FillLastFatalMessageLocked(sig);

    // for protecting g_reservedChildStack
    // g_signalHandlerMutex will be unlocked in DoProcessDump function
    if (sig != SIGDUMP) {
        ForkAndDoProcessDump();
        ResetSignalHandlerIfNeed(sig);
        if (syscall(SYS_rt_tgsigqueueinfo, syscall(SYS_getpid), syscall(SYS_gettid), si->si_signo, si) != 0) {
            DFXLOG_ERROR("Failed to resend signal(%d), errno(%d).", si->si_signo, errno);
        }
        UnlockSignalHandlerMutex();
    } else {
        int recycleTid = clone(DoProcessDump, g_reservedChildStack, CLONE_THREAD | CLONE_SIGHAND | CLONE_VM, NULL);
        if (recycleTid == -1) {
            DFXLOG_ERROR("Failed to create thread for recycle dump process");
            pthread_mutex_unlock(&g_signalHandlerMutex);
        }
    }

    DFXLOG_INFO("Finish handle signal(%d) in %d:%d", sig, g_request.pid, g_request.tid);
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
        DFXLOG_ERROR("Failed to alloc memory for child stack.");
        return;
    }
    g_reservedChildStack = (void *)(((uint8_t *)g_reservedChildStack) + RESERVED_CHILD_STACK_SIZE - 1);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    memset(&g_oldSigactionList, 0, sizeof(g_oldSigactionList));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DFX_SignalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    for (size_t i = 0; i < sizeof(g_interestedSignalList) / sizeof(g_interestedSignalList[0]); i++) {
        int32_t sig = g_interestedSignalList[i];
        if (sigaction(sig, &action, &(g_oldSigactionList[sig])) != 0) {
            DFXLOG_ERROR("Failed to register signal(%d)", sig);
        }
    }

    g_hasInit = TRUE;
}
