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

#if defined (__LF64__)
#define RESERVED_CHILD_STACK_SIZE (32 * 1024)  // 32K
#else
#define RESERVED_CHILD_STACK_SIZE (16 * 1024)  // 16K
#endif

#define BOOL int
#define TRUE 1
#define FALSE 0

#define SECONDS_TO_MILLSECONDS 1000000
#define NANOSECONDS_TO_MILLSECONDS 1000
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
static void *g_reservedMainSignalStack;
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

static uint64_t GetTimeMillseconds(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return ((uint64_t)time.tv_sec * 1000) + // 1000 : second to millsecond convert ratio
        (((uint64_t)time.tv_usec) / 1000); // 1000 : microsecond to millsecond convert ratio
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

#ifndef DFX_LOCAL_UNWIND
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

static void DFX_SetUpSigAlarmAction(void)
{
    if (signal(SIGALRM, SIG_DFL) == SIG_ERR) {
        DfxLogError("signal error!");
    }
    sigset_t set;
    sigemptyset (&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

static int DFX_ExecDump(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_dumpMutex);
    DFX_SetUpEnvironment();
    DFX_SetUpSigAlarmAction();
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

    DfxLogInfo("Start processdump.");
#ifdef DFX_LOG_USE_HILOG_BASE
    execle("/system/bin/processdump", "-signalhandler", NULL, NULL);
#else
    execle("/bin/processdump", "-signalhandler", NULL, NULL);
#endif
    pthread_mutex_unlock(&g_dumpMutex);
    return errno;
}

static pid_t DFX_ForkAndDump()
{
    return clone(DFX_ExecDump, g_reservedChildStack, CLONE_VFORK | CLONE_FS | CLONE_UNTRACED, NULL);
}
#endif

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

#ifdef DFX_LOCAL_UNWIND
static void DFX_UnwindLocal(int sig, siginfo_t *si, void *context)
{
    int32_t fromSignalHandler = 1;
    DfxProcess *process = NULL;
    DfxThread *keyThread = NULL;
    if (!InitThreadByContext(&keyThread, g_request.pid, g_request.tid, &(g_request.context))) {
        DfxLogWarn("Fail to init key thread.");
        DestroyThread(keyThread);
        keyThread = NULL;
        return;
    }

    if (!InitProcessWithKeyThread(&process, g_request.pid, keyThread)) {
        DfxLogWarn("Fail to init process with key thread.");
        DestroyThread(keyThread);
        keyThread = NULL;
        return;
    }

    unw_cursor_t cursor;
    unw_context_t unwContext = {};
    unw_getcontext(&unwContext);
    if (unw_init_local(&cursor, &unwContext) != 0) {
        DfxLogWarn("Fail to init local unwind context.");
        DestroyProcess(process);
        return;
    }

    size_t index = 0;
    do {
        DfxFrame *frame = GetAvaliableFrame(keyThread);
        if (frame == NULL) {
            DfxLogWarn("Fail to create Frame.");
            break;
        }

        frame->index = index;
        char sym[1024] = {0}; // 1024 : symbol buffer size
        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(frame->pc)))) {
            DfxLogWarn("Fail to get program counter.");
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(frame->sp)))) {
            DfxLogWarn("Fail to get stack pointer.");
            break;
        }

        if (FindMapByAddr(process->maps, frame->pc, &(frame->map))) {
            frame->relativePc = GetRelativePc(frame, process->maps);
        }

        if (unw_get_proc_name(&cursor, sym, sizeof(sym), (unw_word_t*)(&(frame->funcOffset))) == 0) {
            std::string funcName;
            std::string strSym(sym, sym + strlen(sym));
            TrimAndDupStr(strSym, funcName);
            frame->funcName = funcName;
        }
        index++;
    } while (unw_step(&cursor) > 0);
    WriteProcessDump(process, &g_request, fromSignalHandler);
    DestroyProcess(process);
}
#endif

void ReadStringFromFile(char* path, char* pDestStore)
{
    char name[NAME_LEN];
    char nameFilter[NAME_LEN];

    memset_s(name, sizeof(name), '\0', sizeof(name));
    memset_s(nameFilter, sizeof(nameFilter), '\0', sizeof(nameFilter));

    int fd = -1;
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        return;
    }
    if (read(fd, name, NAME_LEN -1) == -1) {
        close(fd);
        return;
    }
    char* p = name;
    int i = 0;
    while (*p != '\0') {
        if ((*p == '\n') || (i == NAME_LEN)) {
            break;
        }
        nameFilter[i] = *p;
        p++, i++;
    }
    if (memcpy_s(pDestStore, NAME_LEN, nameFilter, strlen(nameFilter) + 1) != 0) {
        DfxLogError("Failed to copy name.");
    }
    int ret = close(fd);
    if (ret == -1) {
        DfxLogError("close failed!");
    }
}

void GetThreadName(void)
{
    char path[NAME_LEN];
    memset_s(path, sizeof(path), '\0', sizeof(path));
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/comm", getpid()) <= 0) {
        return ;
    }
    ReadStringFromFile(path, g_request.threadName);
}

void GetProcessName(void)
{
    char path[NAME_LEN];
    int ret = memset_s(path, sizeof(path), '\0', sizeof(path));
    if (ret != EOK) {
        printf("memset error!");
    }
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/cmdline", getpid()) <= 0) {
        return;
    }
    ReadStringFromFile(path, g_request.processName);
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
    pthread_mutex_lock(&g_signalHandlerMutex);

    (void)memset_s(&g_request, sizeof(g_request), 0, sizeof(g_request));
    g_curSig = sig;
    g_request.type = sig;
    g_request.tid = gettid();
    g_request.pid = getpid();
    g_request.uid = (int32_t)getuid();
    g_request.reserved = 0;
    g_request.timeStamp = GetTimeMillseconds();
    DfxLogInfo("DFX_SignalHandler :: sig(%d), pid(%d), tid(%d).", sig, g_request.pid, g_request.tid);

    GetThreadName();
    GetProcessName();

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
#ifdef DFX_LOCAL_UNWIND
    DFX_UnwindLocal(sig, si, context);
#else
    pid_t childPid;
    int status;
    int ret = -1;
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
        ret = waitpid(childPid, &status, WNOHANG);
        if (ret < 0) {
            DfxLogError("Failed to wait child process terminated, errno(%d)", errno);
            goto out;
        }

        if (ret == childPid) {
            break;
        }

        if ((int)time(NULL) - startTime > PROCESSDUMP_TIMEOUT) {
            DfxLogError("Exceed max wait time, errno(%d)", errno);
            goto out;
        }
        usleep(SIGNALHANDLER_TIMEOUT); // sleep 10ms
    } while (1);
    DfxLogInfo("child process(%d) terminated with status(%d)", childPid, status);
out:
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
#endif
    DfxLogInfo("Finish handle signal(%d) in %d:%d", sig, g_request.pid, g_request.tid);
    g_curSig = -1;
    pthread_mutex_unlock(&g_signalHandlerMutex);
}

void ReserveMainThreadSignalStack(void)
{
    if (getpid() != gettid()) {
        return;
    }

    g_reservedMainSignalStack = mmap(NULL, RESERVED_CHILD_STACK_SIZE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_reservedMainSignalStack == MAP_FAILED) {
        return;
    }

    stack_t signal_stack;
    signal_stack.ss_sp = g_reservedMainSignalStack;
    signal_stack.ss_size = RESERVED_CHILD_STACK_SIZE;
    signal_stack.ss_flags = 0;
    sigaltstack(&signal_stack, NULL);
}

void DFX_InstallSignalHandler(void)
{
    pthread_mutex_lock(&g_signalHandlerMutex);
    if (g_hasInit) {
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }

    if (SetPlatformSignalHandler != NULL) {
        SetPlatformSignalHandler((uintptr_t)DFX_SignalHandler);
    }

    // reserve stack for fork
    g_reservedChildStack = mmap(NULL, RESERVED_CHILD_STACK_SIZE, \
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 1, 0);
    if (g_reservedChildStack == NULL) {
        DfxLogError("Failed to alloc memory for child stack.");
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }
    g_reservedChildStack = (void *)(((uint8_t *)g_reservedChildStack) + RESERVED_CHILD_STACK_SIZE - 1);

    ReserveMainThreadSignalStack();
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
    pthread_mutex_unlock(&g_signalHandlerMutex);
}
