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

#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <hilog/log_c.h>
#include <securec.h>

#include "dfx_signal_handler.h"

#define RESERVED_CHILD_STACK_SIZE (16 * 1024)  // 16K
#define SIGDUMP 35

#define BOOL int
#define TRUE 1
#define FALSE 0

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSignalHandler"
#endif

#define SECONDS_TO_MILLSECONDS 1000000
#define NANOSECONDS_TO_MILLSECONDS 1000
#ifndef NSIG
#define NSIG 64
#endif

void __attribute__((constructor)) Init()
{
    DFX_InstallSignalHandler();
}

static struct ProcessDumpRequest g_processDumpRequest;
static void *g_reservedChildStack;
static pthread_mutex_t g_signalHandlerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_dumpMutex = PTHREAD_MUTEX_INITIALIZER;
static int g_pipefd[2] = {-1, -1};
static BOOL g_hasInit = FALSE;

enum DumpPreparationStage {
    CREATE_PIPE_FAIL = 1,
    SET_PIPE_LEN_FAIL,
    WRITE_PIPE_FAIL,
    EXEC_FAIL,
};

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
    int devNull = TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if (devNull < 0) {
        HILOG_ERROR(LOG_CORE, "Failed to open dev/null. error:%{public}s", strerror(errno));
        return;
    } else if (devNull != 0) {
        HILOG_ERROR(LOG_CORE, "Failed to open dev/null.");
        close(devNull);
        return;
    }

    TEMP_FAILURE_RETRY(dup2(devNull, STDOUT_FILENO));
    TEMP_FAILURE_RETRY(dup2(devNull, STDERR_FILENO));
    SetInterestedSignalMasks(SIG_BLOCK);
}

static int DFX_ExecDump(void *arg)
{
    (void)arg;

    pthread_mutex_lock(&g_dumpMutex);
    DFX_SetUpEnvironment();

    // create pipe for passing request to processdump
    if (pipe2(g_pipefd, O_CLOEXEC) != 0) {
        HILOG_ERROR(LOG_CORE, "Failed to create pipe for transfering context.");
        pthread_mutex_unlock(&g_dumpMutex);
        return CREATE_PIPE_FAIL;
    }

    ssize_t writeLen = sizeof(struct ProcessDumpRequest);
    if (fcntl(g_pipefd[1], F_SETPIPE_SZ, writeLen) < writeLen) {
        HILOG_ERROR(LOG_CORE, "Failed to set pipe buffer size.");
        pthread_mutex_unlock(&g_dumpMutex);
        return SET_PIPE_LEN_FAIL;
    }

    struct iovec iovs[1] = {
        {
            .iov_base = &g_processDumpRequest,
            .iov_len = sizeof(struct ProcessDumpRequest)
        },
    };

    ssize_t realWriteSize = TEMP_FAILURE_RETRY(writev(g_pipefd[1], iovs, 1));
    if ((ssize_t)writeLen != realWriteSize) {
        HILOG_INFO(LOG_CORE, "Failed to write pipe.");
        pthread_mutex_unlock(&g_dumpMutex);
        return WRITE_PIPE_FAIL;
    }

    TEMP_FAILURE_RETRY(dup2(g_pipefd[0], STDIN_FILENO));
    syscall(SYS_close, g_pipefd[0]);
    syscall(SYS_close, g_pipefd[1]);

    execl("/system/bin/processdump", "-signalhandler", NULL);
    pthread_mutex_unlock(&g_dumpMutex);
    return errno;
}

static pid_t DFX_ForkAndDump()
{
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
        HILOG_WARN(LOG_CORE, "Failed to reset signal(%{public}d).", sig);
        signal(sig, SIG_DFL);
    }
}

static void DFX_SignalHandler(int sig, siginfo_t *si, void *context)
{
    pthread_mutex_lock(&g_signalHandlerMutex);
    ResetSignalHandlerIfNeed(sig);

    pid_t tid = gettid();
    HILOG_INFO(LOG_CORE, "Start dump process for tid:%{public}d signal:%{public}d(%{public}d)",
        tid, sig, si->si_code);

    pid_t childPid;
    int status;
    struct timespec crashTimeSpec;
    memset_s(&crashTimeSpec, sizeof(crashTimeSpec), 0, sizeof(crashTimeSpec));
    clock_gettime(CLOCK_REALTIME, &crashTimeSpec);
    uint64_t crashTime = (uint64_t)(crashTimeSpec.tv_sec) * SECONDS_TO_MILLSECONDS +
        (uint64_t)crashTimeSpec.tv_nsec / NANOSECONDS_TO_MILLSECONDS;

    // set privilege for dump ourself
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    BOOL isTracerStatusModified = FALSE;
    if (prctl(PR_SET_DUMPABLE, 1) != 0) {
        goto out;
    }

    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
        if (errno != EINVAL) {
            goto out;
        }
    } else {
        isTracerStatusModified = TRUE;
    }

    // fill dump request
    memset_s(&g_processDumpRequest, sizeof(g_processDumpRequest), 0, sizeof(g_processDumpRequest));
    g_processDumpRequest.type = sig;
    g_processDumpRequest.tid = tid;
    g_processDumpRequest.pid = getpid();
    g_processDumpRequest.uid = getuid();
    g_processDumpRequest.reserved = 0;
    g_processDumpRequest.timeStamp = crashTime;
    if (memcpy_s(&(g_processDumpRequest.siginfo), sizeof(g_processDumpRequest.siginfo),
        si, sizeof(siginfo_t)) != 0) {
        HILOG_ERROR(LOG_CORE, "Failed to copy siginfo.");
        goto out;
    }

    if (memcpy_s(&(g_processDumpRequest.context), sizeof(g_processDumpRequest.context),
        context, sizeof(ucontext_t)) != 0) {
        HILOG_ERROR(LOG_CORE, "Failed to copy ucontext.");
        goto out;
    }

    // fork a child process that could ptrace us
    childPid = DFX_ForkAndDump();
    if (childPid < 0) {
        HILOG_ERROR(LOG_CORE, "Failed to fork child process.");
        goto out;
    }

    sleep(1);

    // wait the child process terminated
    if (TEMP_FAILURE_RETRY(waitpid(childPid, &status, __WALL)) == -1) {
        HILOG_ERROR(LOG_CORE, "Failed to wait child process terminated. error:%{public}s", strerror(errno));
        goto out;
    }

    if (!(WIFEXITED(status)) || WEXITSTATUS(status) != 0) {
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            HILOG_INFO(LOG_CORE, "processdump exit normally.");
        } else if (WIFSIGNALED(status)) {
            HILOG_INFO(LOG_CORE, "processdump terminated by a signal");
        } else {
            HILOG_INFO(LOG_CORE, "processdump exit unknown");
        }
    }

out:
    prctl(PR_SET_DUMPABLE, prevDumpableStatus);
    if (isTracerStatusModified == TRUE) {
        prctl(PR_SET_PTRACER, 0);
    }

    if (sig != SIGDUMP) {
        if (syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), si->si_signo, si) != 0) {
            HILOG_WARN(LOG_CORE, "Failed to resend signal(%{public}d).", si->si_signo);
        }
    }

    pthread_mutex_unlock(&g_signalHandlerMutex);
}

void DFX_InstallSignalHandler()
{
    pthread_mutex_lock(&g_signalHandlerMutex);
    if (g_hasInit) {
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }

    // reserve stack for fork
    g_reservedChildStack = calloc(RESERVED_CHILD_STACK_SIZE, 1);
    if (g_reservedChildStack == NULL) {
        HILOG_ERROR(LOG_CORE, "Failed to alloc memory for child stack.");
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }
    g_reservedChildStack = (void *)(((uint8_t *)g_reservedChildStack) + RESERVED_CHILD_STACK_SIZE);

    struct sigaction action;
    memset_s(&action, sizeof(action), 0, sizeof(action));
    memset_s(&g_oldSigactionList, sizeof(g_oldSigactionList), 0, sizeof(g_oldSigactionList));
    sigfillset(&action.sa_mask);
    action.sa_sigaction = DFX_SignalHandler;
    action.sa_flags = SA_RESTART | SA_SIGINFO;

    for (size_t i = 0; i < sizeof(g_interestedSignalList) / sizeof(g_interestedSignalList[0]); i++) {
        int32_t sig = g_interestedSignalList[i];
        if (sigaction(sig, &action, &(g_oldSigactionList[sig])) != 0) {
            HILOG_WARN(LOG_CORE, "Failed to register signal(%{public}d).", g_interestedSignalList[i]);
        }
    }
    g_hasInit = TRUE;
    pthread_mutex_unlock(&g_signalHandlerMutex);
}
