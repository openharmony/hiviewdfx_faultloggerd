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

#ifndef F_SETPIPE_SZ
#define F_SETPIPE_SZ 1031
#endif

#define NUMBER_SIXTYFOUR 64
#define INHERITABLE_OFFSET 32

static struct ProcessDumpRequest *g_request = NULL;
static void *g_reservedChildStack = NULL;

static long g_blockFlag = 0;
static long g_vmRealPid = 0;

enum PIPE_FD_TYPE {
    WRITE_TO_DUMP,
    READ_FROM_DUMP_TO_CHILD,
    PIPE_MAX,
};

static int g_pipeFds[PIPE_MAX][2] = {
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

static void CleanFd(int *pipeFd);
static void CleanPipe(void);
static bool InitPipe(void);
static bool ReadPipeTimeout(int fd, uint64_t timeout, uint32_t* value);
static bool ReadProcessDumpGetRegsMsg(void);

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

static int32_t InheritCapabilities(void)
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
    if (g_request->dumpMode == SPLIT_MODE) {
        if (syscall(SYS_pipe2, pipefd, 0) != 0) {
            DFXLOGE("Failed to create pipe for transfering context, errno(%{public}d)", errno);
            return CREATE_PIPE_FAIL;
        }
    } else {
        pipefd[0] = g_pipeFds[WRITE_TO_DUMP][0];
        pipefd[1] = g_pipeFds[WRITE_TO_DUMP][1];
    }

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

    if (InheritCapabilities() != 0) {
        DFXLOGE("Failed to inherit Capabilities from parent.");
        FillCrashExceptionAndReport(CRASH_SIGNAL_EINHERITCAP);
        return INHERIT_CAP_FAIL;
    }
    DFXLOGI("execl processdump.");
#ifdef DFX_LOG_HILOG_BASE
    execl("/system/bin/processdump", "processdump", "-signalhandler", NULL);
#else
    execl("/bin/processdump", "processdump", "-signalhandler", NULL);
#endif
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

static bool SetDumpState(void)
{
    if (prctl(PR_SET_DUMPABLE, 1) != 0) {
        DFXLOGE("Failed to set dumpable, errno(%{public}d).", errno);
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
    DFXLOGI("(%{public}ld) wait %{public}s(%{public}d) exit.", syscall(SYS_gettid), name, childPid);
    do {
        errno = 0;
        ret = waitpid(childPid, &status, WNOHANG);
        if (ret < 0) {
            DFXLOGE("Failed to wait child process terminated, errno(%{public}d)", errno);
            return isSuccess;
        }

        if (ret == childPid) {
            isSuccess = true;
            break;
        }

        if ((int)time(NULL) - startTime > PROCESSDUMP_TIMEOUT) {
            DFXLOGI("(%{public}ld) wait for (%{public}d) timeout", syscall(SYS_gettid), childPid);
            isSuccess = false;
            break;
        }
        usleep(SIGNALHANDLER_TIMEOUT); // sleep 10ms
    } while (1);

    DFXLOGI("(%{public}ld) wait for %{public}s(%{public}d) return with ret(%{public}d), status(%{public}d)",
        syscall(SYS_gettid), name, childPid, ret, status);
    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        DFXLOGI("wait %{public}s(%{public}d) exit code: %{public}d", name, childPid, exitCode);
    } else if (WIFSIGNALED(status)) {
        int sigNum = WTERMSIG(status);
        DFXLOGI("wait %{public}s(%{public}d) exit with sig: %{public}d", name, childPid, sigNum);
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
        DFXLOGI("The exec processdump pid(%{public}ld).", syscall(SYS_getpid));
        Exit(DFX_ExecDump());
    } else if (childPid < 0) {
        DFXLOGE("[%{public}d]: Failed to fork child process, errno(%{public}d).", __LINE__, errno);
        FillCrashExceptionAndReport(CRASH_SIGNAL_EFORK);
        goto out;
    }
    WaitProcessExit(childPid, "processdump");
out:
    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
    return 0;
}

static int CloneAndDoProcessDump(void* arg)
{
    (void)arg;
    DFXLOGI("The clone thread(%{public}ld).", syscall(SYS_gettid));
    g_request->recycleTid = syscall(SYS_gettid);
    return ForkAndExecProcessDump();
}

static bool StartProcessdump(void)
{
    uint64_t startTime = GetAbsTimeMilliSeconds();
    pid_t pid = ForkBySyscall();
    if (pid < 0) {
        DFXLOGE("Failed to fork dummy processdump(%{public}d)", errno);
        return false;
    } else if (pid == 0) {
        if (!InitPipe()) {
            DFXLOGE("init pipe fail");
            _exit(0);
        }
        pid_t processDumpPid = ForkBySyscall();
        if (processDumpPid < 0) {
            DFXLOGE("Failed to fork processdump(%{public}d)", errno);
            _exit(0);
        } else if (processDumpPid > 0) {
            ReadProcessDumpGetRegsMsg();
            _exit(0);
        } else {
            uint64_t endTime;
            int tid;
            ParseSiValue(&g_request->siginfo, &endTime, &tid);
            uint64_t curTime = GetAbsTimeMilliSeconds();
            DFXLOGI("start processdump, fork spend time %{public}" PRIu64 "ms", curTime - startTime);
            if (endTime != 0) {
                DFXLOGI("dump remain %{public}" PRId64 "ms", endTime - curTime);
            }
            if (endTime == 0 || endTime > curTime) {
                g_request->isBlockCrash = (intptr_t)&g_blockFlag;
                g_request->vmProcRealPid = (intptr_t)&g_vmRealPid;
                DFX_ExecDump();
            } else {
                DFXLOGI("current has spend all time, not execl processdump");
            }
            _exit(0);
        }
    }

    if (waitpid(pid, NULL, 0) <= 0) {
        DFXLOGE("failed to wait dummy processdump(%{public}d)", errno);
    }
    return true;
}

static bool StartVMProcessUnwind(void)
{
    uint32_t startTime = GetAbsTimeMilliSeconds();
    pid_t pid = ForkBySyscall();
    if (pid < 0) {
        DFXLOGE("Failed to fork vm process(%{public}d)", errno);
        return false;
    } else if (pid == 0) {
        pid_t vmPid = ForkBySyscall();
        if (vmPid == 0) {
            DFXLOGI("start vm process, fork spend time %{public}" PRIu64 "ms", GetAbsTimeMilliSeconds() - startTime);
            g_vmRealPid = GetRealPid();
            DFXLOGI("vm prorcecc read pid = %{public}ld", g_vmRealPid);
            _exit(0);
        } else {
            DFXLOGI("exit dummy vm process");
            _exit(0);
        }
    }

    if (waitpid(pid, NULL, 0) <= 0) {
        DFXLOGE("failed to wait dummy vm process(%{public}d)", errno);
    }
    return true;
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

static void ReadUnwindFinishMsg(int sig)
{
    if (sig == SIGDUMP) {
        return;
    }

    DFXLOGI("crash processdump unwind finish, blockFlag %{public}ld", g_blockFlag);
    if (g_blockFlag == CRASH_BLOCK_EXIT_FLAG) {
        syscall(SYS_tgkill, g_request->nsPid, g_request->tid, SIGSTOP);
    }
}

static int ProcessDump(int sig)
{
    int prevDumpableStatus = prctl(PR_GET_DUMPABLE);
    bool isTracerStatusModified = SetDumpState();

    g_request->dumpMode = FUSION_MODE;

    do {
        uint64_t endTime;
        int tid;
        ParseSiValue(&g_request->siginfo, &endTime, &tid);
        if (endTime != 0 && endTime <= GetAbsTimeMilliSeconds()) {
            DFXLOGI("enter processdump has coat all time, just exit");
            break;
        }
        if (!StartProcessdump()) {
            DFXLOGE("start processdump fail");
            break;
        }

        if (!StartVMProcessUnwind()) {
            DFXLOGE("start vm process unwind fail");
            break;
        }
        ReadUnwindFinishMsg(sig);
    } while (false);

    DFXLOGI("process dump end");
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
        DFXLOGI("The vm pid(%{public}d:%{public}d).", g_request->vmPid, g_request->vmNsPid);
        DFX_SetUpSigAlarmAction();
        alarm(ALARM_TIME_S);
        _exit(ForkAndExecProcessDump());
    } else if (childPid < 0) {
        DFXLOGE("[%{public}d]: Failed to fork child process, errno(%{public}d).", __LINE__, errno);
        RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
        ForkAndExecProcessDump();
        return;
    }

    DFXLOGI("Start wait for VmProcess(%{public}d) exit.", childPid);
    errno = 0;
    if (!WaitProcessExit(childPid, "VmProcess") &&
        sig != SIGDUMP &&
        sig != SIGLEAK_STACK) {
        DFXLOGI("Wait VmProcess(%{public}d) exit timeout in handling critical signal.", childPid);
        FillCrashExceptionAndReport(CRASH_SIGNAL_EWAITEXIT);
        // do not left vm process
        kill(childPid, SIGKILL);
    }

    RestoreDumpState(prevDumpableStatus, isTracerStatusModified);
}

int DfxDumpRequest(int sig, struct ProcessDumpRequest *request, void *reservedChildStack)
{
    int ret = 0;
    if (request == NULL || reservedChildStack == NULL) {
        DFXLOGE("Failed to DumpRequest because of error parameters!");
        return ret;
    }
    g_request = request;
    g_reservedChildStack = reservedChildStack;
    if (ProcessDump(sig) == 0) {
        ret = sig == SIGDUMP || sig == SIGLEAK_STACK;
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
            DFXLOGE("Failed to clone thread for recycle dump process, errno(%{public}d)", errno);
        }
    }
    return ret;
}
