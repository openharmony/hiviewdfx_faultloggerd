/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "dfx_dumprequest.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <securec.h>
#include <signal.h>
#include <sigchain.h>
#include <stdatomic.h>
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
#include "dfx_log.h"
#include "dfx_signalhandler_exception.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxSignalHandler"
#endif

#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif

#define NUMBER_SIXTYFOUR 64
#define INHERITABLE_OFFSET 32
#define HILOG_SNAPSHOT_LINES 1000

static struct ProcessDumpRequest *g_request = NULL;

static long g_blockExit = 0;
static long g_unwindResult = 0;
static atomic_int g_dumpCount = 0;
static int g_dumpState = 0;
static pthread_mutex_t g_dumpMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutexattr_t g_dumpAttr;

enum PIPE_FD_TYPE {
    WRITE_TO_DUMP,
    READ_FROM_DUMP_TO_CHILD,
    PIPE_MAX,
};

enum START_PROCESS_DUMP_RESULT {
    START_PROCESS_DUMP_SUCCESS = 0,
    START_PROCESS_DUMP_FAIL,
    START_PROCESS_DUMP_RETRY,
};

enum PROCESS_EXIT_STATUS {
    PROCESS_NORMAL_EXIT = 0,
    PROCESS_ABNORMAL_EXIT,
    PROCESS_ALARM_EXIT,
};

static int g_pipeFds[PIPE_MAX][2] = {
    {-1, -1},
    {-1, -1}
};

static const int ALARM_TIME_S = 10;
static const int TRY_WAIT_SECONDS = 1;
static const uint32_t CRASH_SNAPSHOT_FLAG = 0x8;
static const int WAITPID_TIMEOUT = 3000; // 3000 : 3 sec timeout
enum DumpPreparationStage {
    CREATE_PIPE_FAIL = 1,
    SET_PIPE_LEN_FAIL,
    WRITE_PIPE_FAIL,
    INHERIT_CAP_FAIL,
    EXEC_FAIL,
};

static void CleanFd(int *pipeFd);
static void CleanPipe(void);
static bool InitPipe(void);
static bool ReadPipeTimeout(int fd, uint64_t timeout, uint32_t* value);
static bool ReadProcessDumpGetRegsMsg(void);

#ifndef is_ohos_lite
DumpHiTraceIdStruct HiTraceChainGetId() __attribute__((weak));
#endif

void __attribute__((constructor)) InitMutex(void)
{
    pthread_mutexattr_init(&g_dumpAttr);
    pthread_mutexattr_settype(&g_dumpAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_dumpMutex, &g_dumpAttr);
}

void __attribute__((destructor)) DeinitMutex(void)
{
    pthread_mutexattr_destroy(&g_dumpAttr);
    pthread_mutex_destroy(&g_dumpMutex);
}

static void ResetFlags(void)
{
    g_unwindResult = 0;
    g_blockExit = 0;
}

static bool IsDumpSignal(int signo)
{
    return signo == SIGDUMP || signo == SIGLEAK_STACK;
}

static void FillCrashExceptionAndReport(const int err)
{
    struct CrashDumpException exception;
    (void)memset_s(&exception, sizeof(struct CrashDumpException), 0, sizeof(struct CrashDumpException));
    exception.pid = g_request->pid;
    exception.uid = (int32_t)(g_request->uid);
    exception.error = err;
    exception.time = (int64_t)(GetTimeMilliseconds());
    if (strncpy_s(exception.message, sizeof(exception.message), GetCrashDescription(err),
        sizeof(exception.message) - 1) != 0) {
        DFXLOGE("strcpy exception message fail");
        return;
    }
    ReportException(&exception);
}

int32_t DFX_InheritCapabilities(void)
{
    struct __user_cap_header_struct capHeader;
    (void)memset_s(&capHeader, sizeof(capHeader), 0, sizeof(capHeader));

    capHeader.version = _LINUX_CAPABILITY_VERSION_3;
    capHeader.pid = 0;
    struct __user_cap_data_struct capData[2];
    if (capget(&capHeader, &capData[0]) == -1) {
        DFXLOGE("Failed to get origin cap data");
        return -1;
    }

    capData[0].inheritable = capData[0].permitted;
    capData[1].inheritable = capData[1].permitted;
    if (capset(&capHeader, &capData[0]) == -1) {
        DFXLOGE("Failed to set cap data, errno(%{public}d)", errno);
        return -1;
    }

    uint64_t ambCap = capData[0].inheritable;
    ambCap = ambCap | (((uint64_t)capData[1].inheritable) << INHERITABLE_OFFSET);
    for (size_t i = 0; i < NUMBER_SIXTYFOUR; i++) {
        if (ambCap & ((uint64_t)1)) {
            if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, i, 0, 0) < 0) {
                DFXLOGE("Failed to change the ambient capability set, errno(%{public}d)", errno);
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
    const int startIndex = 128;  // 128 : avoid set pipe fail
    const int closeFdCount = 1024;
    for (int i = startIndex; i < closeFdCount; i++) {
        syscall(SYS_close, i);
    }
}

static void DFX_SetUpEnvironment(void)
{
    // clear stdout and stderr
    int devNull = OHOS_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
    if (devNull < 0) {
        DFXLOGE("Failed to open dev/null.");
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
        DFXLOGW("Default signal alarm error!");
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
    pipefd[0] = g_pipeFds[WRITE_TO_DUMP][0];
    pipefd[1] = g_pipeFds[WRITE_TO_DUMP][1];

    ssize_t writeLen = (long)(sizeof(struct ProcessDumpRequest));
    if (fcntl(pipefd[1], F_SETPIPE_SZ, writeLen) < writeLen) {
        DFXLOGE("Failed to set pipe buffer size, errno(%{public}d).", errno);
        return SET_PIPE_LEN_FAIL;
    }

    struct iovec iovs[1] = {
        {
            .iov_base = g_request,
            .iov_len = sizeof(struct ProcessDumpRequest)
        },
    };
    if (OHOS_TEMP_FAILURE_RETRY(writev(pipefd[1], iovs, 1)) != writeLen) {
        DFXLOGE("Failed to write pipe, errno(%{public}d)", errno);
        return WRITE_PIPE_FAIL;
    }
    OHOS_TEMP_FAILURE_RETRY(dup2(pipefd[0], STDIN_FILENO));
    if (pipefd[0] != STDIN_FILENO) {
        syscall(SYS_close, pipefd[0]);
    }
    syscall(SYS_close, pipefd[1]);

    if (DFX_InheritCapabilities() != 0) {
        DFXLOGE("Failed to inherit Capabilities from parent.");
        FillCrashExceptionAndReport(CRASH_SIGNAL_EINHERITCAP);
        return INHERIT_CAP_FAIL;
    }
    DFXLOGI("execl processdump.");
    execl(PROCESSDUMP_PATH, "processdump", "-signalhandler", NULL);
    DFXLOGE("Failed to execl processdump, errno(%{public}d)", errno);
    FillCrashExceptionAndReport(CRASH_SIGNAL_EEXECL);
    return errno;
}

static pid_t ForkBySyscall(void)
{
#ifdef SYS_fork
    return syscall(SYS_fork);
#else
    return syscall(SYS_clone, SIGCHLD, 0);
#endif
}

bool DFX_SetDumpableState(void)
{
    pthread_mutex_lock(&g_dumpMutex);
    int expected = 0;
    if (atomic_compare_exchange_strong(&g_dumpCount, &expected, 1)) {
        g_dumpState = prctl(PR_GET_DUMPABLE);
        if (prctl(PR_SET_DUMPABLE, 1) != 0) {
            DFXLOGE("Failed to set dumpable, errno(%{public}d).", errno);
            atomic_fetch_sub(&g_dumpCount, 1);
            pthread_mutex_unlock(&g_dumpMutex);
            return false;
        }
    } else {
        atomic_fetch_add(&g_dumpCount, 1);
    }
    pthread_mutex_unlock(&g_dumpMutex);
    return true;
}

void DFX_RestoreDumpableState(void)
{
    pthread_mutex_lock(&g_dumpMutex);
    if (atomic_load(&g_dumpCount) > 0) {
        atomic_fetch_sub(&g_dumpCount, 1);
        if (atomic_load(&g_dumpCount) == 0) {
            prctl(PR_SET_DUMPABLE, g_dumpState);
        }
    }
    pthread_mutex_unlock(&g_dumpMutex);
}

static bool SetDumpState(void)
{
    if (DFX_SetDumpableState() == false) {
        return false;
    }
    if (prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0) {
        if (errno != EINVAL) {
            DFXLOGE("Failed to set ptracer, errno(%{public}d).", errno);
            return false;
        }
    }
    return true;
}

static void RestoreDumpState(bool isTracerStatusModified)
{
    DFX_RestoreDumpableState();
    if (isTracerStatusModified == true) {
        prctl(PR_SET_PTRACER, 0);
    }
}

static void SafeDelayOneMillSec(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000; // 1000000 : 1ms
    OHOS_TEMP_FAILURE_RETRY(nanosleep(&ts, &ts));
}

static int WaitProcessExitTimeout(pid_t pid, int timeoutMs)
{
    int status = 1; // abnomal exit code
    while (timeoutMs > 0) {
        int res = waitpid(pid, &status, WNOHANG);
        if (res > 0) {
            break;
        } else if (res < 0) {
            DFXLOGE("failed to wait dummy, error(%{public}d)", errno);
            break;
        }
        SafeDelayOneMillSec();
        timeoutMs--;
        if (timeoutMs == 0) {
            DFXLOGI("waitpid %{public}d timeout", pid);
            kill(pid, SIGKILL);
            FillCrashExceptionAndReport(CRASH_SIGNAL_EWAITPIDTIMEOUT);
            return PROCESS_ABNORMAL_EXIT;
        }
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
        DFXLOGE("dummy exit with signal(%{public}d)", WTERMSIG(status));
        return PROCESS_ALARM_EXIT;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        DFXLOGE("dummy exit (%{public}d)", WEXITSTATUS(status));
        return PROCESS_NORMAL_EXIT;
    }
    DFXLOGE("dummy exit with error(%{public}d)", WEXITSTATUS(status));
    return PROCESS_ABNORMAL_EXIT;
}

static void TryNonSafeOperate(bool isCrash)
{
    DFX_SetUpSigAlarmAction();
    alarm(TRY_WAIT_SECONDS);
#ifndef is_ohos_lite
    if (HiTraceChainGetId != NULL && isCrash) {
        DumpHiTraceIdStruct hitraceChainId = HiTraceChainGetId();
        if (memcpy_s(&g_request->hitraceId, sizeof(g_request->hitraceId),
            &hitraceChainId, sizeof(hitraceChainId)) != 0) {
            DFXLOGE("memcpy hitrace fail");
        }
    }
#endif
    alarm(0); // cancel alarm
}

static void ForkProcessdump(uint64_t startTime)
{
    pid_t processDumpPid = ForkBySyscall();
    if (processDumpPid < 0) {
        DFXLOGE("Failed to fork processdump(%{public}d)", errno);
        syscall(SYS_exit, errno);
    } else if (processDumpPid > 0) {
        int ret = ReadProcessDumpGetRegsMsg() == true ? 0 : errno;
        DFXLOGI("exit the processdump parent process.");
        syscall(SYS_exit, ret);
    } else {
        uint64_t endTime;
        int tid;
        ParseSiValue(&g_request->siginfo, &endTime, &tid);
        uint64_t curTime = GetAbsTimeMilliSecondsCInterce();
        DFXLOGI("start processdump, fork spend time %{public}" PRIu64 "ms", curTime - startTime);
        if (endTime != 0) {
            DFXLOGI("dump remain %{public}" PRId64 "ms", endTime - curTime);
        }
        if (endTime == 0 || endTime > curTime) {
            g_request->blockCrashExitAddr = (intptr_t)&g_blockExit;
            g_request->unwindResultAddr = (intptr_t)&g_unwindResult;
            DFX_ExecDump();
        } else {
            DFXLOGI("current has spend all time, not execl processdump");
        }
        syscall(SYS_exit, 0);
    }
}

static int StartProcessdump(bool allowNonSafeOperate, bool isCrash)
{
    uint64_t startTime = GetAbsTimeMilliSecondsCInterce();
    pid_t pid = ForkBySyscall();
    if (pid < 0) {
        DFXLOGE("Failed to fork dummy processdump(%{public}d)", errno);
        return START_PROCESS_DUMP_FAIL;
    } else if (pid == 0) {
        if (allowNonSafeOperate) {
            TryNonSafeOperate(isCrash);
        }
        if (!InitPipe()) {
            DFXLOGE("init pipe fail");
            syscall(SYS_exit, errno);
        }
        ForkProcessdump(startTime);
    }
    switch (WaitProcessExitTimeout(pid, WAITPID_TIMEOUT)) {
        case PROCESS_NORMAL_EXIT:
            return START_PROCESS_DUMP_SUCCESS;
        case PROCESS_ABNORMAL_EXIT:
            return START_PROCESS_DUMP_FAIL;
        case PROCESS_ALARM_EXIT:
            return START_PROCESS_DUMP_RETRY;
        default:
            break;
    }
    return START_PROCESS_DUMP_FAIL;
}

static bool StartVMProcessUnwind(void)
{
    pid_t pid = ForkBySyscall();
    if (pid < 0) {
        DFXLOGE("Failed to fork vm process(%{public}d)", errno);
        return false;
    }
    if (pid == 0) {
        pid_t vmPid = ForkBySyscall();
        if (vmPid == 0) {
            DFXLOGI("exit vm process");
            syscall(SYS_exit, 0);
        } else {
            DFXLOGI("exit dummy vm process");
            syscall(SYS_exit, 0);
        }
    }

    return WaitProcessExitTimeout(pid, WAITPID_TIMEOUT) == PROCESS_NORMAL_EXIT;
}

static void CleanFd(int *pipeFd)
{
    if (*pipeFd != -1) {
        syscall(SYS_close, *pipeFd);
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
    bool ret = true;
    for (int i = 0; i < PIPE_MAX; i++) {
        if (syscall(SYS_pipe2, g_pipeFds[i], 0) == -1) {
            DFXLOGE("create pipe fail, errno(%{public}d)", errno);
            ret = false;
            CleanPipe();
            break;
        }
    }
    if (!ret) {
        CloseFds();
        for (int i = 0; i < PIPE_MAX; i++) {
            if (syscall(SYS_pipe2, g_pipeFds[i], 0) == -1) {
                DFXLOGE("create pipe fail again, errno(%{public}d)", errno);
                FillCrashExceptionAndReport(CRASH_SIGNAL_ECREATEPIPE);
                CleanPipe();
                return false;
            }
        }
    }

    g_request->childPipeFd[0] = g_pipeFds[READ_FROM_DUMP_TO_CHILD][0];
    g_request->childPipeFd[1] = g_pipeFds[READ_FROM_DUMP_TO_CHILD][1];
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
    DFXLOGE("read pipe failed , errno(%{public}d)", errno);
    return false;
}

static bool ReadProcessDumpGetRegsMsg(void)
{
    CleanFd(&g_pipeFds[READ_FROM_DUMP_TO_CHILD][1]);

    DFXLOGI("start wait processdump read registers");
    const uint64_t readRegsTimeout = 5000; // 5s
    uint32_t isFinishGetRegs = OPE_FAIL;
    if (ReadPipeTimeout(g_pipeFds[READ_FROM_DUMP_TO_CHILD][0], readRegsTimeout, &isFinishGetRegs)) {
        if (isFinishGetRegs == OPE_SUCCESS) {
            DFXLOGI("processdump have get all registers .");
            return true;
        }
    }

    return false;
}

void SetKernelSnapshot(bool enable)
{
    const char *filePath = "/proc/self/unexpected_die_catch";
    if (access(filePath, F_OK) < 0) {
        return;
    }
    int dieCatchFd = open(filePath, O_RDWR);
    if (dieCatchFd < 0) {
        return;
    }
    do {
        char val[10] = {0}; // 10 : to save diecatch val
        if (read(dieCatchFd, val, sizeof(val)) < 0) {
            DFXLOGE("Failed to read unexpecterd_die_catch %{public}d", errno);
            break;
        }
        if (lseek(dieCatchFd, 0, SEEK_SET) < 0) {
            DFXLOGE("Failed to lseek unexpecterd_die_catch %{public}d", errno);
            break;
        }

        uint32_t num = (uint32_t)strtoul(val, NULL, 16); // 16 : val is hex
        if (errno == ERANGE) {
            DFXLOGE("Failed to cast unexpecterd_die_catch val to int %{public}d", errno);
            break;
        }
        if (enable) {
            num |= CRASH_SNAPSHOT_FLAG;
        } else {
            num &= (~CRASH_SNAPSHOT_FLAG);
        }

        (void)memset_s(val, sizeof(val), 0, sizeof(val));
        if (snprintf_s(val, sizeof(val), sizeof(val) - 1, "0x%x", num) < 0) {
            DFXLOGE("Failed to format unexpecterd_die_catch val %{public}d", errno);
            break;
        }
        if (write(dieCatchFd, val, sizeof(val)) < 0) {
            DFXLOGE("Failed to write unexpecterd_die_catch %{public}d", errno);
        }
    } while (false);
    syscall(SYS_close, dieCatchFd);
}

static void ReadUnwindFinishMsg(int signo)
{
    if (IsDumpSignal(signo)) {
        return;
    }

    DFXLOGI("crash processdump unwind finish, unwind success Flag %{public}ld, blockFlag %{public}ld",
        g_unwindResult, g_blockExit);
    if (g_unwindResult == CRASH_UNWIND_SUCCESS_FLAG) {
        SetKernelSnapshot(false);
    }
    if (g_blockExit == CRASH_BLOCK_EXIT_FLAG) {
        syscall(SYS_tgkill, g_request->nsPid, g_request->tid, SIGSTOP);
    }
}

static int ProcessDump(int signo)
{
    bool isTracerStatusModified = SetDumpState();
    bool isCrash = !IsDumpSignal(signo);
    if (isCrash) {
        ResetFlags();
        SetKernelSnapshot(true);
#ifndef is_ohos_lite
        HiLogRecordSnapshot(HILOG_SNAPSHOT_LINES, g_request->timeStamp);
#endif
    }

    do {
        uint64_t endTime;
        int tid;
        ParseSiValue(&g_request->siginfo, &endTime, &tid);
        if (endTime != 0 && endTime <= GetAbsTimeMilliSecondsCInterce()) {
            DFXLOGI("enter processdump has coat all time, just exit");
            break;
        }
        int result = StartProcessdump(true, isCrash);
        // not allow non-safe operate and try again
        if (result == START_PROCESS_DUMP_FAIL || (result == START_PROCESS_DUMP_RETRY &&
                StartProcessdump(false, isCrash) != START_PROCESS_DUMP_SUCCESS)) {
            DFXLOGE("start processdump fail");
            break;
        }

        if (!StartVMProcessUnwind()) {
            DFXLOGE("start vm process unwind fail");
            break;
        }
        ReadUnwindFinishMsg(signo);
    } while (false);

    RestoreDumpState(isTracerStatusModified);
    return 0;
}

void DfxDumpRequest(int signo, struct ProcessDumpRequest *request)
{
    if (request == NULL) {
        DFXLOGE("Failed to DumpRequest because of error parameters!");
        return;
    }
    g_request = request;
    ProcessDump(signo);
}

bool DFX_EnableNativeCrashKernelSnapshot()
{
    if (!DFX_SetDumpableState()) {
        return false;
    }
    SetKernelSnapshot(true);
    DFX_RestoreDumpableState();
    return true;
}
