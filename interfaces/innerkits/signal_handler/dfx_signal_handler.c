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
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <hilog_base/log_base.h>
#include <securec.h>
#include "dfx_define.h"
#include "../../../tools/process_dump/dfx_log.h"

#ifdef DFX_LOCAL_UNWIND
#include <libunwind.h>

#include "dfx_dump_writer.h"
#include "dfx_elf.h"
#include "dfx_frames.h"
#include "dfx_maps.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_thread.h"
#include "dfx_util.h"
#endif


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

#define NUMBER_SIXTYFOUR 64
#define INHERITABLE_OFFSET 32

#ifndef SIGNAL_HANDLER_DEBUG
#define SIGNAL_HANDLER_DEBUG
#endif
#undef SIGNAL_HANDLER_DEBUG

void __attribute__((constructor)) Init()
{
    DFX_InstallSignalHandler();
}

static struct ProcessDumpRequest g_request;
static void *g_reservedChildStack;
static pthread_mutex_t g_signalHandlerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_dumpMutex = PTHREAD_MUTEX_INITIALIZER;
static int g_pipefd[2] = {-1, -1};
static BOOL g_hasInit = FALSE;

char* g_StackInfo_ = NULL;
long long g_CurrentPosition = 0;

enum DumpPreparationStage {
    CREATE_PIPE_FAIL = 1,
    SET_PIPE_LEN_FAIL,
    WRITE_PIPE_FAIL,
    INHERIT_CAP_FAIL,
    EXEC_FAIL,
};

#ifdef SIGNAL_HANDLER_DEBUG
static void PrintCapability(unsigned long long capability, char *tag)
{
    DfxLogToSocket(tag);
    for (size_t i = 0; i < NUMBER_SIXTYFOUR; i++) {
        if (capability & ((uint64_t)1)) {
            DfxLogToSocket("capability: 1");
        } else {
            DfxLogToSocket("capability: 0");
        }
        capability = capability >> 1;
    }
}
#endif

static int32_t InheritCapabilities()
{
    struct __user_cap_header_struct capHeader;
    if (memset_s(&capHeader, sizeof(capHeader), 0, sizeof(capHeader)) != EOK) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to memset cap header.");
        return -1;
    }

    capHeader.version = _LINUX_CAPABILITY_VERSION_3;
    capHeader.pid = 0;
    struct __user_cap_data_struct capData[2];
    if (capget(&capHeader, &capData[0]) == -1) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to get origin cap data");
        return -1;
    }

    capData[0].inheritable = capData[0].permitted;
    capData[1].inheritable = capData[1].permitted;
    if (capset(&capHeader, &capData[0]) == -1) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to set cap data");
        return -1;
    }

    uint64_t ambCap = capData[0].inheritable;
    ambCap = ambCap | (((uint64_t)capData[1].inheritable) << INHERITABLE_OFFSET);
#ifdef SIGNAL_HANDLER_DEBUG
    PrintCapability((unsigned long long)ambCap, "ambCap");
    PrintCapability((unsigned long long)capData[0].inheritable, "capData[0].inheritable");
    PrintCapability((unsigned long long)capData[1].inheritable, "capData[1].inheritable");
#endif
    for (size_t i = 0; i < NUMBER_SIXTYFOUR; i++) {
        if (ambCap & ((uint64_t)1)) {
            (void)prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, i, 0, 0);
        }
        ambCap = ambCap >> 1;
    }
    DfxLogToSocket("InheritCapabilities done");
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
    int devNull = TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if (devNull < 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to open dev/null.");
        return;
    }

    TEMP_FAILURE_RETRY(dup2(devNull, STDOUT_FILENO));
    TEMP_FAILURE_RETRY(dup2(devNull, STDERR_FILENO));
    syscall(SYS_close, devNull);
    SetInterestedSignalMasks(SIG_BLOCK);
}

static int DFX_ExecDump(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&g_dumpMutex);

#ifdef SIGNAL_HANDLER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_ExecDump.");
#endif

    DFX_SetUpEnvironment();
    // create pipe for passing request to processdump
    if (pipe(g_pipefd) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to create pipe for transfering context.");
        pthread_mutex_unlock(&g_dumpMutex);
        return CREATE_PIPE_FAIL;
    }
    ssize_t writeLen = sizeof(struct ProcessDumpRequest);
    if (fcntl(g_pipefd[1], F_SETPIPE_SZ, writeLen) < writeLen) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to set pipe buffer size.");
        pthread_mutex_unlock(&g_dumpMutex);
        return SET_PIPE_LEN_FAIL;
    }

    struct iovec iovs[1] = {
        {
            .iov_base = &g_request,
            .iov_len = sizeof(struct ProcessDumpRequest)
        },
    };
    ssize_t realWriteSize = TEMP_FAILURE_RETRY(writev(g_pipefd[1], iovs, 1));
    if ((ssize_t)writeLen != realWriteSize) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to write pipe.");
        pthread_mutex_unlock(&g_dumpMutex);
        return WRITE_PIPE_FAIL;
    }
    TEMP_FAILURE_RETRY(dup2(g_pipefd[0], STDIN_FILENO));
    if (g_pipefd[0] != STDIN_FILENO) {
        syscall(SYS_close, g_pipefd[0]);
    }
    syscall(SYS_close, g_pipefd[1]);

    if (InheritCapabilities() != 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to inherit Capabilities from parent.");
        return INHERIT_CAP_FAIL;
    }

    DfxLogToSocket("Start processdump.");
    execle("/system/bin/processdump", "-signalhandler", NULL, NULL);
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
        HILOG_BASE_ERROR(LOG_CORE, "Failed to reset signal.");
        signal(sig, SIG_DFL);
    }
}

#ifdef DFX_LOCAL_UNWIND
long WriteDumpInfo(long current_position, size_t index, DfxFrame *frame)
{
    int writeNumber = 0;
    char* current_pos = g_StackInfo_ + current_position;
    if (frame->funcName == NULL) {
        writeNumber = sprintf_s(current_pos, BACK_STACK_INFO_SIZE-current_position,
        "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s\n",
        index, frame->relativePc, frame->pc, (frame->map== NULL) ? "Unknown" : frame->map->path);
    } else {
        writeNumber = sprintf_s(current_pos, BACK_STACK_INFO_SIZE-current_position,
        "#%02zu pc %016" PRIx64 "(%016" PRIx64 ") %s(%s+%" PRIu64 ")\n", index,
        frame->relativePc, frame->pc, (frame->map == NULL) ?
        "Unknown" : frame->map->path, frame->funcName, frame->funcOffset);
    }
    return writeNumber;
}

static void DFX_UnwindLocal(int sig, siginfo_t *si, void *context)
{
#ifdef SIGNAL_HANDLER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_UnwindLocal :: sig(%{public}d), callerPid(%{public}d), callerTid(%{public}d).",
        sig, si->si_pid, si->si_uid);
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_UnwindLocal :: sig(%{public}d), pid(%{public}d), tid(%{public}d).",
        sig, g_request.pid, g_request.tid);
#endif

    int32_t fromSignalHandler = 1;
    DfxProcess *process = NULL;
    DfxThread *keyThread = NULL;
    if (!InitThreadByContext(&keyThread, g_request.pid, g_request.tid, &(g_request.context))) {
        DestroyThread(keyThread);
        keyThread = NULL;
        return;
    }

    if (!InitProcessWithKeyThread(&process, g_request.pid, keyThread)) {
        DestroyThread(keyThread);
        keyThread = NULL;
        return;
    }

    unw_cursor_t cursor;
    unw_context_t unwContext = {};
    unw_getcontext(&unwContext);
    if (unw_init_local(&cursor, &unwContext) != 0) {
        DestroyProcess(process);
        return;
    }

    size_t index = 0;
    int hasSkipDumpCatch = false;
    do {
        DfxFrame *frame = GetAvaliableFrame(keyThread);
        if (frame == NULL) {
            break;
        }

        frame->index = index;
        char sym[1024] = {0}; // 1024 : symbol buffer size
        if (unw_get_reg(&cursor, UNW_REG_IP, (unw_word_t*)(&(frame->pc)))) {
            break;
        }

        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)(&(frame->sp)))) {
            break;
        }

        if (FindMapByAddr(process->maps, frame->pc, &(frame->map))) {
            frame->relativePc = GetRelativePc(frame, process->maps);
        }

        if (unw_get_proc_name(&cursor, sym, sizeof(sym), (unw_word_t*)(&(frame->funcOffset))) == 0) {
            frame->funcName = TrimAndDupStr(sym);
        }

        // skip DumpCatch stack, as this is dump catcher. Caller don't need it.
        if (hasSkipDumpCatch == true) {
            int writeNumber = WriteDumpInfo(g_CurrentPosition, index, frame);
            g_CurrentPosition = g_CurrentPosition + writeNumber;
#ifdef SIGNAL_HANDLER_DEBUG
            HILOG_BASE_DEBUG(LOG_CORE, "DFX_UnwindLocal :: index(%{public}d), current_position(%{public}lld).",
                index, g_CurrentPosition);
#endif
            index++;
        }
#ifdef SIGNAL_HANDLER_DEBUG
        HILOG_BASE_DEBUG(LOG_CORE, "DFX_UnwindLocal :: path(%{public}s).", frame->map->path);
#endif
        if ((strcmp("/system/lib/libdfx_signalhandler.z.so", frame->map->path) == 0) && hasSkipDumpCatch == false) {
            hasSkipDumpCatch = true;
        }
    } while ((unw_step(&cursor) > 0) && (index < BACK_STACK_MAX_STEPS));
    DestroyProcess(process);
}
#endif

static void DFX_SignalHandler(int sig, siginfo_t *si, void *context)
{
    pthread_mutex_lock(&g_signalHandlerMutex);
    DfxLogByTrace(true, "faultlog_native_crash");
    DfxLogByTrace(true, "faultlog_crash_hold_process");

    (void)memset_s(&g_request, sizeof(g_request), 0, sizeof(g_request));
    g_request.type = sig;
    g_request.tid = gettid();
    g_request.pid = getpid();
    g_request.uid = getuid();
    g_request.reserved = 0;
    g_request.timeStamp = time(NULL);
    if (memcpy_s(&(g_request.siginfo), sizeof(g_request.siginfo),
        si, sizeof(siginfo_t)) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to copy siginfo.");
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }
    if (memcpy_s(&(g_request.context), sizeof(g_request.context),
        context, sizeof(ucontext_t)) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to copy ucontext.");
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }

#ifdef SIGNAL_HANDLER_DEBUG
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_SignalHandler :: sig(%{public}d).", sig);
    HILOG_BASE_DEBUG(LOG_CORE, "DFX_SignalHandler :: pid(%{public}d), tid(%{public}d).", g_request.pid, g_request.tid);
    if (sig == SIGDUMP) {
        HILOG_BASE_DEBUG(LOG_CORE, "DFX_SignalHandler :: callerPid(%{public}d), callerTid(%{public}d).",
            si->si_pid, si->si_uid);
    }
#endif

#ifdef DFX_LOCAL_UNWIND
if (sig == SIGDUMP) {
    if ((g_request.pid == si->si_pid) && (g_StackInfo_ != NULL)) {
#ifdef SIGNAL_HANDLER_DEBUG
        HILOG_BASE_DEBUG(LOG_CORE, "DFX_SignalHandler :: call DFX_UnwindLocal.");
#endif
        DFX_UnwindLocal(sig, si, context);
        pthread_mutex_unlock(&g_signalHandlerMutex);
        return;
    }
}
#endif

    pid_t childPid;
    int status;
    // set privilege for dump ourself
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    BOOL isTracerStatusModified = FALSE;
    if (prctl(PR_SET_DUMPABLE, 1) != 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to set dumpable.");
        goto out;
    }
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
        if (errno != EINVAL) {
            HILOG_BASE_ERROR(LOG_CORE, "Failed to set ptracer.");
            goto out;
        }
    } else {
        isTracerStatusModified = TRUE;
    }
    // fork a child process that could ptrace us
    childPid = DFX_ForkAndDump();
    if (childPid < 0) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to fork child process.");
        goto out;
    }
    // wait the child process terminated
    if (TEMP_FAILURE_RETRY(waitpid(childPid, &status, __WALL)) == -1) {
        HILOG_BASE_ERROR(LOG_CORE, "Failed to wait child process terminated.");
        goto out;
    }
    DfxLogByTrace(false, "faultlog_crash_hold_process");
out:
    ResetSignalHandlerIfNeed(sig);
    prctl(PR_SET_DUMPABLE, prevDumpableStatus);
    if (isTracerStatusModified == TRUE) {
        prctl(PR_SET_PTRACER, 0);
    }

    if (sig != SIGDUMP) {
        if (syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), si->si_signo, si) != 0) {
            HILOG_BASE_ERROR(LOG_CORE, "Failed to resend signal.");
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
        HILOG_BASE_ERROR(LOG_CORE, "Failed to alloc memory for child stack.");
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
            HILOG_BASE_ERROR(LOG_CORE, "Failed to register signal.");
        }
    }
    g_hasInit = TRUE;
    pthread_mutex_unlock(&g_signalHandlerMutex);
}
