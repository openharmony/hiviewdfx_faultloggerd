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

static struct ProcessDumpRequest *g_request = NULL;
static void *g_reservedChildStack = NULL;
static pthread_mutex_t *g_signalHandlerMutex = NULL;

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

static const int SIGNALHANDLER_TIMEOUT = 10000; // 10000 us
static const int ALARM_TIME_S = 10;
enum DumpPreparationStage {
    CREATE_PIPE_FAIL = 1,
    SET_PIPE_LEN_FAIL,
    WRITE_PIPE_FAIL,
    INHERIT_CAP_FAIL,
    EXEC_FAIL,
};

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
    exception.pid = g_request->pid;
    exception.uid = (int32_t)(g_request->uid);
    exception.error = err;
    exception.time = (int64_t)(GetTimeMilliseconds());
    (void)strncpy(exception.message, GetCrashDescription(err), sizeof(exception.message) - 1);
    ReportException(exception);
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
    if (g_request->dumpMode == SPLIT_MODE) {
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
            .iov_base = g_request,
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

static void Exit(int flag)
{
    _exit(flag); // Avoid crashes that occur when directly using the _exit()
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
        g_request->dumpMode = SPLIT_MODE;
        DFXLOG_INFO("The exec processdump pid(%ld).", syscall(SYS_getpid));
        Exit(DFX_ExecDump());
    } else if (childPid < 0) {
        DFXLOG_ERROR("Failed to fork child process, errno(%d).", errno);
        FillCrashExceptionAndReport(CRASH_SIGNAL_EFORK);
        goto out;
    }
    WaitProcessExit(childPid, "processdump");
out:
    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
    pthread_mutex_unlock(g_signalHandlerMutex);
    return 0;
}

static int CloneAndDoProcessDump(void* arg)
{
    (void)arg;
    DFXLOG_INFO("The clone thread(%ld).", syscall(SYS_gettid));
    g_request->recycleTid = syscall(SYS_gettid);
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
            ParseSiValue(&g_request->siginfo, &endTime, &tid);
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

    g_request->pmPipeFd[0] = g_pipeFds[READ_FROM_DUMP_TO_MAIN][0];
    g_request->pmPipeFd[1] = g_pipeFds[READ_FROM_DUMP_TO_MAIN][1];
    g_request->vmPipeFd[0] = g_pipeFds[READ_FORM_DUMP_TO_VIRTUAL][0];
    g_request->vmPipeFd[1] = g_pipeFds[READ_FORM_DUMP_TO_VIRTUAL][1];
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
    g_request->dumpMode = FUSION_MODE;

    do {
        uint64_t endTime;
        int tid;
        ParseSiValue(&g_request->siginfo, &endTime, &tid);
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
        g_request->vmNsPid = syscall(SYS_getpid);
        g_request->vmPid = GetRealPid();
        DFXLOG_INFO("The vm pid(%d:%d).", g_request->vmPid, g_request->vmNsPid);
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
    pthread_mutex_unlock(g_signalHandlerMutex);
}

int DfxDumpRequest(int sig, struct ProcessDumpRequest *request, void *reservedChildStack,
    pthread_mutex_t *signalHandlerMutex)
{
    int ret = 0;
    if (request == NULL || reservedChildStack == NULL || signalHandlerMutex == NULL) {
        DFXLOG_ERROR("Failed to DumpRequest because of error parameters!");
        return ret;
    }
    g_request = request;
    g_reservedChildStack = reservedChildStack;
    g_signalHandlerMutex = signalHandlerMutex;
    if (ProcessDump(sig) == 0) {
        ret = sig == SIGDUMP || sig == SIGLEAK_STACK;
        pthread_mutex_unlock(g_signalHandlerMutex);
        return ret;
    }

    if (sig != SIGDUMP) {
        ret = sig == SIGLEAK_STACK ? true : false;
        ForkAndDoProcessDump(sig);
    } else {
        ret = true;
        int recycleTid = clone(CloneAndDoProcessDump, reservedChildStack,\
            CLONE_THREAD | CLONE_SIGHAND | CLONE_VM, NULL);
        if (recycleTid == -1) {
            DFXLOG_ERROR("Failed to clone thread for recycle dump process, errno(%d)", errno);
            pthread_mutex_unlock(g_signalHandlerMutex);
        }
    }
    return ret;
}