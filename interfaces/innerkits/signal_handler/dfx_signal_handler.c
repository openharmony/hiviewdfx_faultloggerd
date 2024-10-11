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

enum PIPE_FD_TYPE {
    WRITE_TO_DUMP,
    READ_FROM_DUMP_TO_MAIN,
    READ_FORM_DUMP_TO_VIRTUAL,
    PIPE_MAX,
};

static int g_pipeFds[PIPE_MAX][2] = {
    {-1, -1},
    {-1, -1},
    {-1, -1}
};

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

static void FillLastFatalMessageLocked(int32_t sig, void *context)
{
    if (sig != SIGABRT) {
        ThreadInfoCallBack callback = GetCallbackLocked();
        if (callback != NULL) {
            DFXLOG_INFO("Start collect crash thread info.");
            callback(g_request.lastFatalMessage, sizeof(g_request.lastFatalMessage), context);
            DFXLOG_INFO("Finish collect crash thread info.");
        }
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
        DFXLOG_ERROR("Last message is longer than MAX_FATAL_MSG_SIZE");
        return;
    }

    (void)strncpy(g_request.lastFatalMessage, lastFatalMessage, sizeof(g_request.lastFatalMessage) - 1);
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

static void FillDumpRequest(int sig, siginfo_t *si, void *context)
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
        DFXLOG_INFO("g_GetStackIdFunc %p.", (void*)g_request.stackId);
    }

    GetThreadNameByTid(g_request.tid, g_request.threadName, sizeof(g_request.threadName));
    GetProcessName(g_request.processName, sizeof(g_request.processName));

    memcpy(&(g_request.siginfo), si, sizeof(siginfo_t));
    memcpy(&(g_request.context), context, sizeof(ucontext_t));

    FillTraceIdLocked(&g_request);

    FillLastFatalMessageLocked(sig, context);
    g_request.crashObj = (uintptr_t)pthread_getspecific(g_crashObjKey);
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
        DFXLOG_ERROR("Failed to set cap data, errno(%d)", errno);
        return -1;
    }

    uint64_t ambCap = capData[0].inheritable;
    ambCap = ambCap | (((uint64_t)capData[1].inheritable) << INHERITABLE_OFFSET);
    for (size_t i = 0; i < NUMBER_SIXTYFOUR; i++) {
        if (ambCap & ((uint64_t)1)) {
            if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, i, 0, 0) < 0) {
                DFXLOG_ERROR("Failed to change the ambient capability set, errno(%d)", errno);
            }
        }
        ambCap = ambCap >> 1;
    }
    return 0;
}

static const int SIGCHAIN_DUMP_SIGNAL_LIST[] = {
    SIGDUMP, SIGLEAK_STACK
};

static const int SIGCHAIN_CRASH_SIGNAL_LIST[] = {
    SIGILL, SIGABRT, SIGBUS, SIGFPE,
    SIGSEGV, SIGSTKFLT, SIGSYS, SIGTRAP
};

static void SetInterestedSignalMasks(int how)
{
    sigset_t set;
    sigemptyset(&set);
    for (size_t i = 0; i < sizeof(SIGCHAIN_DUMP_SIGNAL_LIST) / sizeof(SIGCHAIN_DUMP_SIGNAL_LIST[0]); i++) {
        sigaddset(&set, SIGCHAIN_DUMP_SIGNAL_LIST[i]);
    }
    for (size_t i = 0; i < sizeof(SIGCHAIN_CRASH_SIGNAL_LIST) / sizeof(SIGCHAIN_CRASH_SIGNAL_LIST[0]); i++) {
        sigaddset(&set, SIGCHAIN_CRASH_SIGNAL_LIST[i]);
    }
    sigprocmask(how, &set, NULL);
}

static void CloseFds(void)
{
    const int closeFdCount = 1024;
    for (int i = 0; i < closeFdCount; i++) {
        syscall(SYS_close, i);
    }
}

static void DFX_SetUpEnvironment(void)
{
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
        DFXLOG_WARN("Default signal alarm error!");
    }
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

static int DFX_ExecDump(void)
{
    DFX_SetUpEnvironment();
    DFX_SetUpSigAlarmAction();
    alarm(ALARM_TIME_S);
    int pipefd[2] = {-1, -1};
    // create pipe for passing request to processdump
    if (g_request.dumpMode == SPLIT_MODE) {
        if (pipe(pipefd) != 0) {
            DFXLOG_ERROR("Failed to create pipe for transfering context, errno(%d)", errno);
            return CREATE_PIPE_FAIL;
        }
    } else {
        pipefd[0] = g_pipeFds[WRITE_TO_DUMP][0];
        pipefd[1] = g_pipeFds[WRITE_TO_DUMP][1];
    }

    ssize_t writeLen = (long)(sizeof(struct ProcessDumpRequest));
    if (fcntl(pipefd[1], F_SETPIPE_SZ, writeLen) < writeLen) {
        DFXLOG_ERROR("Failed to set pipe buffer size, errno(%d).", errno);
        return SET_PIPE_LEN_FAIL;
    }

    struct iovec iovs[1] = {
        {
            .iov_base = &g_request,
            .iov_len = sizeof(struct ProcessDumpRequest)
        },
    };
    if (OHOS_TEMP_FAILURE_RETRY(writev(pipefd[1], iovs, 1)) != writeLen) {
        DFXLOG_ERROR("Failed to write pipe, errno(%d)", errno);
        return WRITE_PIPE_FAIL;
    }
    OHOS_TEMP_FAILURE_RETRY(dup2(pipefd[0], STDIN_FILENO));
    if (pipefd[0] != STDIN_FILENO) {
        syscall(SYS_close, pipefd[0]);
    }
    syscall(SYS_close, pipefd[1]);

    if (InheritCapabilities() != 0) {
        DFXLOG_ERROR("Failed to inherit Capabilities from parent.");
        FillCrashExceptionAndReport(CRASH_SIGNAL_EINHERITCAP);
        return INHERIT_CAP_FAIL;
    }
    DFXLOG_INFO("execl processdump.");
#ifdef DFX_LOG_HILOG_BASE
    execl("/system/bin/processdump", "processdump", "-signalhandler", NULL);
#else
    execl("/bin/processdump", "processdump", "-signalhandler", NULL);
#endif
    DFXLOG_ERROR("Failed to execl processdump, errno(%d)", errno);
    FillCrashExceptionAndReport(CRASH_SIGNAL_EEXECL);
    return errno;
}

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
        DFXLOG_ERROR("Failed to reset sig(%d).", sig);
        signal(sig, SIG_DFL);
    }

    if (syscall(SYS_rt_tgsigqueueinfo, syscall(SYS_getpid), syscall(SYS_gettid), sig, si) != 0) {
        DFXLOG_ERROR("Failed to rethrow sig(%d), errno(%d).", sig, errno);
    } else {
        DFXLOG_INFO("Current process(%ld) rethrow sig(%d).", syscall(SYS_getpid), sig);
    }
}

static void PauseMainThreadHandler(int sig)
{
    DFXLOG_INFO("Crash(%d) in child thread(%ld), lock main thread.", sig, syscall(SYS_gettid));
    // only work when subthread crash and send SIGDUMP to mainthread.
    pthread_mutex_lock(&g_signalHandlerMutex);
    pthread_mutex_unlock(&g_signalHandlerMutex);
    DFXLOG_INFO("Crash in child thread(%ld), exit main thread.", syscall(SYS_gettid));
}

static void BlockMainThreadIfNeed(int sig)
{
    if (IsMainThread() || IsDumpSignal(sig)) {
        return;
    }

    DFXLOG_INFO("Try block main thread.");
    (void)signal(SIGQUIT, PauseMainThreadHandler);
    if (syscall(SYS_tgkill, syscall(SYS_getpid), syscall(SYS_getpid), SIGQUIT) != 0) {
        DFXLOG_ERROR("Failed to send SIGQUIT to main thread, errno(%d).", errno);
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
        DFXLOG_ERROR("Failed to set dumpable, errno(%d).", errno);
        return false;
    }

    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
        if (errno != EINVAL) {
            DFXLOG_ERROR("Failed to set ptracer, errno(%d).", errno);
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

static void SetSelfThreadParam(const char* name, int priority)
{
    pthread_setname_np(pthread_self(), name);
    struct sched_param schedParam;
    schedParam.sched_priority = priority;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &schedParam);
}

static bool WaitProcessExit(int childPid, const char* name)
{
    int ret = -1;
    int status = 0;
    int startTime = (int)time(NULL);
    bool isSuccess = false;
    DFXLOG_INFO("(%ld) wait %s(%d) exit.", syscall(SYS_gettid), name, childPid);
    do {
        errno = 0;
        ret = waitpid(childPid, &status, WNOHANG);
        if (ret < 0) {
            DFXLOG_ERROR("Failed to wait child process terminated, errno(%d)", errno);
            return isSuccess;
        }

        if (ret == childPid) {
            isSuccess = true;
            break;
        }

        if ((int)time(NULL) - startTime > PROCESSDUMP_TIMEOUT) {
            DFXLOG_INFO("(%ld) wait for (%d) timeout", syscall(SYS_gettid), childPid);
            isSuccess = false;
            break;
        }
        usleep(SIGNALHANDLER_TIMEOUT); // sleep 10ms
    } while (1);

    DFXLOG_INFO("(%ld) wait for %s(%d) return with ret(%d), status(%d)",
        syscall(SYS_gettid), name, childPid, ret, status);
    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        DFXLOG_INFO("wait %s(%d) exit code: %d", name, childPid, exitCode);
    } else if (WIFSIGNALED(status)) {
        int sigNum = WTERMSIG(status);
        DFXLOG_INFO("wait %s(%d) exit with sig: %d", name, childPid, sigNum);
    }
    return isSuccess;
}

static int ForkAndExecProcessDump(void)
{
    int childPid = -1;
    SetSelfThreadParam("dump_tmp_thread", 0);

    // set privilege for dump ourself
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    bool isTracerStatusModified = SetDumpState();
    if (!isTracerStatusModified) {
        FillCrashExceptionAndReport(CRASH_SIGNAL_ESETSTATE);
        goto out;
    }

    // fork a child process that could ptrace us
    childPid = ForkBySyscall();
    if (childPid == 0) {
        g_request.dumpMode = SPLIT_MODE;
        DFXLOG_INFO("The exec processdump pid(%ld).", syscall(SYS_getpid));
        _exit(DFX_ExecDump());
    } else if (childPid < 0) {
        DFXLOG_ERROR("Failed to fork child process, errno(%d).", errno);
        FillCrashExceptionAndReport(CRASH_SIGNAL_EFORK);
        goto out;
    }
    WaitProcessExit(childPid, "processdump");
out:
    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
    pthread_mutex_unlock(&g_signalHandlerMutex);
    return 0;
}

static int CloneAndDoProcessDump(void* arg)
{
    (void)arg;
    DFXLOG_INFO("The clone thread(%ld).", syscall(SYS_gettid));
    g_request.recycleTid = syscall(SYS_gettid);
    return ForkAndExecProcessDump();
}

static bool StartProcessdump(void)
{
    uint64_t startTime = GetAbsTimeMilliSeconds();
    pid_t pid = ForkBySyscall();
    if (pid < 0) {
        DFXLOG_ERROR("Failed to fork dummy processdump(%d)", errno);
        return false;
    } else if (pid == 0) {
        pid_t processDumpPid = ForkBySyscall();
        if (processDumpPid < 0) {
            DFXLOG_ERROR("Failed to fork processdump(%d)", errno);
            _exit(0);
        } else if (processDumpPid > 0) {
            _exit(0);
        } else {
            uint64_t endTime;
            int tid;
            ParseSiValue(&g_request.siginfo, &endTime, &tid);
            uint64_t curTime = GetAbsTimeMilliSeconds();
            DFXLOG_INFO("start processdump, fork spend time %" PRIu64 "ms", curTime - startTime);
            if (endTime != 0) {
                DFXLOG_INFO("dump remain %" PRId64 "ms", endTime - curTime);
            }
            if (endTime == 0 || endTime > curTime) {
                DFX_ExecDump();
            } else {
                DFXLOG_INFO("%s", "current has spend all time, not execl processdump");
            }
            _exit(0);
        }
    }

    if (waitpid(pid, NULL, 0) <= 0) {
        DFXLOG_ERROR("failed to wait dummy processdump(%d)", errno);
        return false;
    }
    return true;
}

static bool StartVMProcessUnwind(void)
{
    uint32_t startTime = GetAbsTimeMilliSeconds();
    pid_t pid = ForkBySyscall();
    if (pid < 0) {
        DFXLOG_ERROR("Failed to fork vm process(%d)", errno);
        return false;
    } else if (pid == 0) {
        pid_t vmPid = ForkBySyscall();
        if (vmPid == 0) {
            DFXLOG_INFO("start vm process, fork spend time %" PRIu64 "ms", GetAbsTimeMilliSeconds() - startTime);
            close(g_pipeFds[WRITE_TO_DUMP][0]);
            pid_t pids[PID_MAX] = {0};
            pids[REAL_PROCESS_PID] = GetRealPid();
            pids[VIRTUAL_PROCESS_PID] = syscall(SYS_getpid);

            OHOS_TEMP_FAILURE_RETRY(write(g_pipeFds[WRITE_TO_DUMP][1], pids, sizeof(pids)));
            close(g_pipeFds[WRITE_TO_DUMP][1]);

            uint32_t finishUnwind = OPE_FAIL;
            close(g_pipeFds[READ_FORM_DUMP_TO_VIRTUAL][1]);
            OHOS_TEMP_FAILURE_RETRY(read(g_pipeFds[READ_FORM_DUMP_TO_VIRTUAL][0], &finishUnwind, sizeof(finishUnwind)));
            close(g_pipeFds[READ_FORM_DUMP_TO_VIRTUAL][0]);
            DFXLOG_INFO("processdump unwind finish, exit vm pid = %d", pids[VIRTUAL_PROCESS_PID]);
            _exit(0);
        } else {
            DFXLOG_INFO("exit dummy vm process");
            _exit(0);
        }
    }

    if (waitpid(pid, NULL, 0) <= 0) {
        DFXLOG_ERROR("failed to wait dummy vm process(%d)", errno);
        return false;
    }
    return true;
}

static void CleanFd(int *pipeFd)
{
    if (*pipeFd != -1) {
        close(*pipeFd);
        *pipeFd = -1;
    }
}

static void CleanPipe(void)
{
    for (size_t i = 0; i < PIPE_MAX; i++) {
        CleanFd(&g_pipeFds[i][0]);
        CleanFd(&g_pipeFds[i][1]);
    }
}

static bool InitPipe(void)
{
    for (int i = 0; i < PIPE_MAX; i++) {
        if (pipe(g_pipeFds[i]) == -1) {
            DFXLOG_ERROR("create pipe fail, errno(%d)", errno);
            FillCrashExceptionAndReport(CRASH_SIGNAL_ECREATEPIPE);
            CleanPipe();
            return false;
        }
    }

    g_request.pmPipeFd[0] = g_pipeFds[READ_FROM_DUMP_TO_MAIN][0];
    g_request.pmPipeFd[1] = g_pipeFds[READ_FROM_DUMP_TO_MAIN][1];
    g_request.vmPipeFd[0] = g_pipeFds[READ_FORM_DUMP_TO_VIRTUAL][0];
    g_request.vmPipeFd[1] = g_pipeFds[READ_FORM_DUMP_TO_VIRTUAL][1];
    return true;
}

static bool ReadPipeTimeout(int fd, uint64_t timeout, uint32_t* value)
{
    if (fd < 0 || value == NULL) {
        return false;
    }
    struct pollfd pfds[1];
    pfds[0].fd = fd;
    pfds[0].events = POLLIN;

    uint64_t startTime = GetTimeMilliseconds();
    uint64_t endTime = startTime + timeout;
    int pollRet = -1;
    do {
        pollRet = poll(pfds, 1, timeout);
        if ((pollRet > 0) && (pfds[0].revents && POLLIN)) {
            if (OHOS_TEMP_FAILURE_RETRY(read(fd, value, sizeof(uint32_t))) ==
                (long int)(sizeof(uint32_t))) {
                return true;
            }
        }

        uint64_t now = GetTimeMilliseconds();
        if (now >= endTime || now < startTime) {
            break;
        } else {
            timeout = endTime - now;
        }
    } while (pollRet < 0 && errno == EINTR);
    FillCrashExceptionAndReport(CRASH_SIGNAL_EREADPIPE);
    DFXLOG_ERROR("read pipe failed , errno(%d)", errno);
    return false;
}

static bool ReadProcessDumpGetRegsMsg(void)
{
    CleanFd(&g_pipeFds[READ_FROM_DUMP_TO_MAIN][1]);

    DFXLOG_INFO("start wait processdump read registers");
    const uint64_t readRegsTimeout = 5000; // 5s
    uint32_t isFinishGetRegs = OPE_FAIL;
    if (ReadPipeTimeout(g_pipeFds[READ_FROM_DUMP_TO_MAIN][0], readRegsTimeout, &isFinishGetRegs)) {
        if (isFinishGetRegs == OPE_SUCCESS) {
            DFXLOG_INFO("processdump have get all registers .");
            return true;
        }
    }

    return false;
}

static void ReadUnwindFinishMsg(int sig)
{
    if (sig == SIGDUMP) {
        return;
    }

    DFXLOG_INFO("start wait processdump unwind");
    const uint64_t unwindTimeout = 10000; // 10s
    uint32_t isExitAfterUnwind = OPE_CONTINUE;
    if (ReadPipeTimeout(g_pipeFds[READ_FROM_DUMP_TO_MAIN][0], unwindTimeout, &isExitAfterUnwind)) {
        DFXLOG_INFO("processdump unwind finish");
    } else {
        DFXLOG_ERROR("wait processdump unwind finish timeout");
    }
}

static int ProcessDump(int sig)
{
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    bool isTracerStatusModified = SetDumpState();

    if (!InitPipe()) {
        return -1;
    }
    g_request.dumpMode = FUSION_MODE;

    do {
        uint64_t endTime;
        int tid;
        ParseSiValue(&g_request.siginfo, &endTime, &tid);
        if (endTime != 0 && endTime <= GetAbsTimeMilliSeconds()) {
            DFXLOG_INFO("%s", "enter processdump has coat all time, just exit");
            break;
        }
        if (!StartProcessdump()) {
            DFXLOG_ERROR("start processdump fail");
            break;
        }

        if (!ReadProcessDumpGetRegsMsg()) {
            break;
        }

        if (!StartVMProcessUnwind()) {
            DFXLOG_ERROR("start vm process unwind fail");
            break;
        }
        ReadUnwindFinishMsg(sig);
    } while (false);

    CleanPipe();
    DFXLOG_INFO("process dump end");
    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
    return 0;
}

static void ForkAndDoProcessDump(int sig)
{
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    bool isTracerStatusModified = SetDumpState();
    int childPid = ForkBySyscall();
    if (childPid == 0) {
        CloseFds();
        g_request.vmNsPid = syscall(SYS_getpid);
        g_request.vmPid = GetRealPid();
        DFXLOG_INFO("The vm pid(%d:%d).", g_request.vmPid, g_request.vmNsPid);
        DFX_SetUpSigAlarmAction();
        alarm(ALARM_TIME_S);
        _exit(ForkAndExecProcessDump());
    } else if (childPid < 0) {
        DFXLOG_ERROR("Failed to fork child process, errno(%d).", errno);
        RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
        ForkAndExecProcessDump();
        return;
    }

    DFXLOG_INFO("Start wait for VmProcess(%d) exit.", childPid);
    errno = 0;
    if (!WaitProcessExit(childPid, "VmProcess") &&
        sig != SIGDUMP &&
        sig != SIGLEAK_STACK) {
        DFXLOG_INFO("Wait VmProcess(%d) exit timeout in handling critical signal.", childPid);
        FillCrashExceptionAndReport(CRASH_SIGNAL_EWAITEXIT);
        // do not left vm process
        kill(childPid, SIGKILL);
    }

    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
    pthread_mutex_unlock(&g_signalHandlerMutex);
}

static bool DFX_SigchainHandler(int sig, siginfo_t *si, void *context)
{
    int pid = syscall(SYS_getpid);
    int tid = syscall(SYS_gettid);

    DFXLOG_INFO("DFX_SigchainHandler :: sig(%d), pid(%d), tid(%d).", sig, pid, tid);
    bool ret = false;
    if (sig == SIGDUMP) {
        if (si->si_code != DUMP_TYPE_REMOTE) {
            DFXLOG_WARN("DFX_SigchainHandler :: sig(%d:%d) is not remote dump type, return directly",
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

    FillDumpRequest(sig, si, context);
    DFXLOG_INFO("DFX_SigchainHandler :: sig(%d), pid(%d), processName(%s), threadName(%s).",
        sig, g_request.pid, g_request.processName, g_request.threadName);
    if (ProcessDump(sig) == 0) {
        ret = sig == SIGDUMP || sig == SIGLEAK_STACK;
        DFXLOG_INFO("Finish handle signal(%d) in %d:%d", sig, g_request.pid, g_request.tid);
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return ret;
    }
    // for protecting g_reservedChildStack
    // g_signalHandlerMutex will be unlocked in ForkAndExecProcessDump function
    if (sig != SIGDUMP) {
        ret = sig == SIGLEAK_STACK ? true : false;
        ForkAndDoProcessDump(sig);
    } else {
        ret = true;
        int recycleTid = clone(CloneAndDoProcessDump, g_reservedChildStack,\
            CLONE_THREAD | CLONE_SIGHAND | CLONE_VM, NULL);
        if (recycleTid == -1) {
            DFXLOG_ERROR("Failed to clone thread for recycle dump process, errno(%d)", errno);
            pthread_mutex_unlock(&g_signalHandlerMutex);
        }
    }

    DFXLOG_INFO("Finish handle signal(%d) in %d:%d", sig, g_request.pid, g_request.tid);
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
        DFXLOG_ERROR("Failed to register signal(%d)", sig);
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
        DFXLOG_ERROR("Failed to alloc memory for child stack.");
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
        DFXLOG_ERROR("param error. appRunningId is NULL or length overflow");
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
        DFXLOG_ERROR("crash object address can not greater than 2^56, set crash object equal 0!");
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