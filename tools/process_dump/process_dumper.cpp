/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "process_dumper.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <pthread.h>
#include <securec.h>
#include <string>
#include <syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

#include "cppcrash_reporter.h"
#include "crash_exception.h"
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_dump_res.h"
#include "dfx_fdsan.h"
#include "dfx_logger.h"
#include "dfx_process.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_stack_info_formatter.h"
#include "dfx_thread.h"
#include "dfx_unwind_remote.h"
#include "dfx_util.h"
#include "dfx_trace.h"
#include "elapsed_time.h"
#include "faultloggerd_client.h"
#ifndef HISYSEVENT_DISABLE
#include "hisysevent.h"
#endif
#include "printer.h"
#include "procinfo.h"
#include "unwinder_config.h"
#ifndef is_ohos_lite
#include "parameter.h"
#include "parameters.h"
#endif // !is_ohos_lite

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxProcessDump"
const char *const BLOCK_CRASH_PROCESS = "faultloggerd.priv.block_crash_process.enabled";

static bool IsBlockCrashProcess()
{
    bool isBlockCrash = false;
#ifndef is_ohos_lite
    isBlockCrash = OHOS::system::GetParameter(BLOCK_CRASH_PROCESS, "false") == "true";
#endif
    return isBlockCrash;
}

void WriteData(int fd, const std::string& data, size_t blockSize)
{
    size_t dataSize = data.length();
    size_t index = 0;
    while (index < dataSize) {
        size_t writeLength = (index + blockSize) <= dataSize ? blockSize : (dataSize - index);
        ssize_t nwrite = OHOS_TEMP_FAILURE_RETRY(write(fd, data.substr(index, writeLength).c_str(), writeLength));
        if (nwrite != static_cast<ssize_t>(writeLength)) {
            DFXLOG_INFO("%s :: nwrite: %zd, writeLength: %zu", __func__, nwrite, writeLength);
        }
        index += writeLength;
    }
    DFXLOG_INFO("%s :: needWriteDataSize: %zu, writeDataSize: %zu", __func__, dataSize, index);
}

#if !defined(__x86_64__)
const int ARG_MAX_NUM = 131072;
#endif
using OpenFilesList = std::map<int, FDInfo>;

bool ReadLink(std::string &src, std::string &dst)
{
    char buf[PATH_MAX];
    ssize_t count = readlink(src.c_str(), buf, sizeof(buf) - 1);
    if (count < 0) {
        return false;
    }
    buf[count] = '\0';
    dst = buf;
    return true;
}

void CollectOpenFiles(OpenFilesList &list, pid_t pid)
{
    std::string fdDirName = "/proc/" + std::to_string(pid) + "/fd";
    std::unique_ptr<DIR, int (*)(DIR *)> dir(opendir(fdDirName.c_str()), closedir);
    if (dir == nullptr) {
        DFXLOG_ERROR("failed to open directory %s: %s", fdDirName.c_str(), strerror(errno));
        return;
    }

    struct dirent *de;
    while ((de = readdir(dir.get())) != nullptr) {
        if (*de->d_name == '.') {
            continue;
        }

        int fd = atoi(de->d_name);
        std::string path = fdDirName + "/" + std::string(de->d_name);
        std::string target;
        if (ReadLink(path, target)) {
            list[fd].path = target;
        } else {
            list[fd].path = "???";
            DFXLOG_ERROR("failed to readlink %s: %s", path.c_str(), strerror(errno));
        }
    }
}

#if !defined(__x86_64__)
void FillFdsaninfo(OpenFilesList &list, pid_t nsPid, uint64_t fdTableAddr)
{
    constexpr size_t fds = sizeof(FdTable::entries) / sizeof(*FdTable::entries);
    size_t entryOffset = offsetof(FdTable, entries);
    uint64_t addr = fdTableAddr + entryOffset;
    FdEntry entrys[fds];
    if (DfxMemory::ReadProcMemByPid(nsPid, addr, entrys, sizeof(FdEntry) * fds) != sizeof(FdEntry) * fds) {
        DFXLOG_ERROR("read nsPid mem error %s", strerror(errno));
        return;
    }
    for (size_t i = 0; i < fds; i++) {
        if (entrys[i].close_tag) {
            list[i].fdsanOwner = entrys[i].close_tag;
        }
    }

    size_t overflowOffset = offsetof(FdTable, overflow);
    uintptr_t overflow = 0;
    uint64_t tmp = fdTableAddr + overflowOffset;
    if (DfxMemory::ReadProcMemByPid(nsPid, tmp, &overflow, sizeof(overflow)) != sizeof(overflow)) {
        return;
    }
    if (!overflow) {
        return;
    }

    size_t overflowLength;
    if (DfxMemory::ReadProcMemByPid(nsPid, overflow, &overflowLength, sizeof(overflowLength))
        != sizeof(overflowLength)) {
        return;
    }
    if (overflowLength > ARG_MAX_NUM) {
        return;
    }

    std::vector<FdEntry> overflowFdEntrys(overflowLength);
    uint64_t address = overflow + offsetof(FdTableOverflow, entries);
    if (DfxMemory::ReadProcMemByPid(nsPid, address, overflowFdEntrys.data(), sizeof(FdEntry) * overflowLength) !=
        sizeof(FdEntry) * overflowLength) {
        DFXLOG_ERROR("read nsPid mem error %s", strerror(errno));
        return;
    }
    size_t fdIndex = fds;
    for (size_t i = 0; i < overflowLength; i++) {
        if (overflowFdEntrys[i].close_tag) {
            list[fdIndex++].fdsanOwner = overflowFdEntrys[i].close_tag;
        }
    }
}
#endif

std::string DumpOpenFiles(OpenFilesList &files)
{
    std::string openFiles;
    for (auto &[fd, entry]: files) {
        const std::string path = entry.path;
        uint64_t tag = entry.fdsanOwner;
        const char* type = fdsan_get_tag_type(tag);
        uint64_t val = fdsan_get_tag_value(tag);
        if (!path.empty()) {
            openFiles += std::to_string(fd) + "->" + path + " " + type + " " + std::to_string(val) + "\n";
        } else {
            openFiles += "OpenFilesList contain an entry (fd " + std::to_string(fd) + ") with no path or owner\n";
        }
    }
    return openFiles;
}

void WaitForFork(unsigned long pid, unsigned long& childPid)
{
    DFXLOG_INFO("%s", "start wait fork event happen");
    int waitStatus = 0;
    waitpid(pid, &waitStatus, 0); // wait fork event
    DFXLOG_INFO("wait for fork status %d", waitStatus);
    if (waitStatus >> 8 == (SIGTRAP | (PTRACE_EVENT_FORK << 8))) { // 8 : get fork  event flag
        ptrace(PTRACE_GETEVENTMSG, pid, NULL, &childPid);
        DFXLOG_INFO("next child pid %lu", childPid);
        waitpid(childPid, &waitStatus, 0); // wait child stop event
    }
}

void ReadVmRealPid(std::shared_ptr<ProcessDumpRequest> request, unsigned long vmPid, int& realPid)
{
    int waitStatus = 0;
    ptrace(PTRACE_SETOPTIONS, vmPid, NULL, PTRACE_O_TRACEEXIT); // block son exit
    ptrace(PTRACE_CONT, vmPid, NULL, NULL);

    long data = 0;
    DFXLOG_INFO("%s", "start wait exit event happen");
    waitpid(vmPid, &waitStatus, 0); // wait exit stop
    data = ptrace(PTRACE_PEEKDATA, vmPid, reinterpret_cast<void *>(request->vmProcRealPid), NULL);
    if (data < 0) {
        DFXLOG_INFO("ptrace peek data error %lu %d", vmPid, errno);
    }
    realPid = static_cast<int>(data);
}

void ReadPids(std::shared_ptr<ProcessDumpRequest> request, int& realPid, int& vmPid, std::shared_ptr<DfxProcess> proc)
{
    unsigned long childPid = 0;

    WaitForFork(request->tid, childPid);
    ptrace(PTRACE_CONT, childPid, NULL, NULL);

    // frezze detach after fork child to fork vm process
    if (request->siginfo.si_signo == SIGDUMP) {
        if (proc->keyThread_ != nullptr) {
            proc->keyThread_->Detach();
        }
        proc->Detach();
        DFXLOG_INFO("%s", "ptrace detach all tids");
    }

    unsigned long sonPid = 0;
    WaitForFork(childPid, sonPid);
    vmPid = sonPid;

    ptrace(PTRACE_DETACH, childPid, 0, 0);
    ReadVmRealPid(request, sonPid, realPid);

    DFXLOG_INFO("procecdump get real pid is %d vm pid is %d", realPid, vmPid);
}

void InfoRemoteProcessResult(std::shared_ptr<ProcessDumpRequest> request, int result)
{
    if (request == nullptr) {
        return;
    }
    if (request->childPipeFd[0] != -1) {
        close(request->childPipeFd[0]);
        request->childPipeFd[0] = -1;
    }
    if (OHOS_TEMP_FAILURE_RETRY(write(request->childPipeFd[1], &result, sizeof(result))) < 0) {
        DFXLOG_ERROR("write to child process fail %d", errno);
    }
}

void SetProcessdumpTimeout(siginfo_t &si)
{
    if (si.si_signo != SIGDUMP) {
        return;
    }

    uint64_t endTime;
    int tid;
    ParseSiValue(si, endTime, tid);

    if (tid == 0) {
        DFXLOG_INFO("%s", "reset, prevent incorrect reading sival_int for tid");
        si.si_value.sival_int = 0;
    }

    if (endTime == 0) {
        DFXLOG_INFO("%s", "end time is zero, not set new alarm");
        return;
    }

    uint64_t curTime = GetAbsTimeMilliSeconds();
    if (curTime >= endTime) {
        DFXLOG_INFO("%s", "now has timeout, processdump exit");
#ifndef CLANG_COVERAGE
        _exit(0);
#endif
    }
    uint64_t diffTime = endTime - curTime;

    DFXLOG_INFO("processdump remain time%" PRIu64 "ms", diffTime);
    if (diffTime > PROCESSDUMP_TIMEOUT * NUMBER_ONE_THOUSAND) {
        DFXLOG_ERROR("%s", "dump remain time is invalid, not set timer");
        return;
    }

    struct itimerval timer;
    timer.it_value.tv_sec = static_cast<int64_t>(diffTime / NUMBER_ONE_THOUSAND);
    timer.it_value.tv_usec = static_cast<int64_t>(diffTime * NUMBER_ONE_THOUSAND % NUMBER_ONE_MILLION);
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, nullptr) != 0) {
        DFXLOG_ERROR("start processdump timer fail %d", errno);
    }
}
}

ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper ins;
    return ins;
}

void ProcessDumper::Dump()
{
    startTime_ = GetTimeMillisec();
    std::shared_ptr<ProcessDumpRequest> request = std::make_shared<ProcessDumpRequest>();
    resDump_ = DumpProcess(request);
    if (process_ == nullptr) {
        DFXLOG_ERROR("%s", "Dump process failed, please check permission and whether pid is valid.");
    } else {
        if (isCrash_ && process_->vmThread_ != nullptr) {
            process_->vmThread_->Detach();
        }
        if (isCrash_ && (request->dumpMode == FUSION_MODE) && IsBlockCrashProcess()) {
            DFXLOG_INFO("start block crash process pid %d nspid %d", request->pid, request->nsPid);
            if (ptrace(PTRACE_POKEDATA, request->nsPid, (void*)request->isBlockCrash, CRASH_BLOCK_EXIT_FLAG) < 0) {
                DFXLOG_ERROR("pok block falg to nsPid %d fail %s", request->nsPid, strerror(errno));
            }
        }
        if (process_->keyThread_ != nullptr) {
            process_->keyThread_->Detach();
        }
    }

    std::string jsonInfo;
    if (isJsonDump_ || isCrash_) {
        DfxStackInfoFormatter formatter(process_, request);
        formatter.GetStackInfo(isJsonDump_, jsonInfo);
        DFXLOG_INFO("Finish GetStackInfo len %" PRIuPTR "", jsonInfo.length());
        if (isJsonDump_) {
            WriteData(jsonFd_, jsonInfo, MAX_PIPE_SIZE);
        }
    }

    finishTime_ = GetTimeMillisec();
    ReportSigDumpStats(request);
    // After skipping InitPrintThread due to ptrace failure or other reasons,
    // request resFd_ to write back the result to dumpcatch
    if (request->siginfo.si_signo == SIGDUMP && resFd_ == -1) {
        resFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES);
        if (resFd_ < 0) {
            resFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE_RES);
        }
    }
    WriteDumpRes(resDump_);
    // print keythread base info to hilog when carsh
    DfxRingBufferWrapper::GetInstance().PrintBaseInfo();
    DfxRingBufferWrapper::GetInstance().StopThread();
    DFXLOG_INFO("Finish dump stacktrace for %s(%d:%d).", request->processName, request->pid, request->tid);
    CloseDebugLog();
    Report(request, jsonInfo);
}

void ProcessDumper::Report(std::shared_ptr<ProcessDumpRequest> request, std::string &jsonInfo)
{
    if (request == nullptr) {
        DFXLOG_ERROR("%s", "request is nullptr.");
        return;
    }
    if (request->msg.type == MESSAGE_FDSAN_DEBUG && strlen(request->msg.body) > 0) {
#ifndef HISYSEVENT_DISABLE
        std::string fingerPrint = request->processName;
        if (process_ != nullptr && process_->keyThread_ != nullptr) {
            auto frames = process_->keyThread_->GetFrames();
            frames.erase(std::remove_if(frames.begin(), frames.end(),
                [](const DfxFrame& frame) {
                    return frame.mapName.find("ld-musl-", 0) != std::string::npos;
                }),
                frames.end());
            constexpr size_t MAX_FRAME_CNT = 3;
            for (size_t index = 0; index < MAX_FRAME_CNT && index < frames.size(); index++) {
                fingerPrint = fingerPrint + frames[index].funcName;
            }
        }
        size_t hashVal = std::hash<std::string>()(fingerPrint);
        HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::RELIABILITY, "ADDR_SANITIZER",
                        OHOS::HiviewDFX::HiSysEvent::EventType::FAULT,
                        "MODULE", request->processName,
                        "PID", request->pid,
                        "HAPPEN_TIME", request->timeStamp,
                        "REASON", "DEBUG SIGNAL",
                        "FINGERPRINT", std::to_string(hashVal));
        DFXLOG_INFO("%s", "Report fdsan event done.");
#else
        DFXLOG_INFO("%s", "Not supported for fdsan reporting.");
#endif
        return;
    }
    if (resDump_ != DumpErrorCode::DUMP_ENOMAP && resDump_ != DumpErrorCode::DUMP_EREADPID) {
        ReportCrashInfo(jsonInfo);
    }
}

static int32_t ReadRequestAndCheck(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("ReadRequestAndCheck");
    ElapsedTime counter("ReadRequestAndCheck", 20); // 20 : limit cost time 20 ms
    ssize_t readCount = OHOS_TEMP_FAILURE_RETRY(read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest)));
    request->threadName[NAME_BUF_LEN - 1] = '\0';
    request->processName[NAME_BUF_LEN - 1] = '\0';
    request->msg.body[MAX_FATAL_MSG_SIZE - 1] = '\0';
    request->appRunningId[MAX_APP_RUNNING_UNIQUE_ID_LEN - 1] = '\0';
    if (readCount != static_cast<long>(sizeof(ProcessDumpRequest))) {
        DFXLOG_ERROR("Failed to read DumpRequest(%d), readCount(%zd).", errno, readCount);
        ReportCrashException(request->processName, request->pid, request->uid,
                             CrashExceptionCode::CRASH_DUMP_EREADREQ);
        return DumpErrorCode::DUMP_EREADREQUEST;
    }

    return DumpErrorCode::DUMP_ESUCCESS;
}

std::string GetOpenFiles(int32_t pid, int nsPid, uint64_t fdTableAddr)
{
    DFX_TRACE_SCOPED("GetOpenFiles");
    OpenFilesList openFies;
    CollectOpenFiles(openFies, pid);
#if !defined(__x86_64__)
    FillFdsaninfo(openFies, nsPid, fdTableAddr);
#endif
    std::string fds = DumpOpenFiles(openFies);
    DFXLOG_INFO("%s", "get open files info finish");
    return fds;
}

void ProcessDumper::InitRegs(std::shared_ptr<ProcessDumpRequest> request, int &dumpRes)
{
    DFX_TRACE_SCOPED("InitRegs");
    if (request->dumpMode == FUSION_MODE) {
        if (!DfxUnwindRemote::GetInstance().InitProcessAllThreadRegs(request, process_)) {
            DFXLOG_ERROR("%s", "Failed to init process regs.");
            dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
        }

        DFXLOG_INFO("%s", "get all tid regs finish");
    }
}

bool ProcessDumper::IsTargetProcessAlive(std::shared_ptr<ProcessDumpRequest> request)
{
    if ((request->dumpMode == SPLIT_MODE) && ((!isCrash_ && (syscall(SYS_getppid) != request->nsPid)) ||
        (isCrash_ && (syscall(SYS_getppid) != request->vmNsPid)))) {
        DfxRingBufferWrapper::GetInstance().AppendMsg(
            "Target process has been killed, the crash log may not be fully generated.");
        ReportCrashException(request->processName, request->pid, request->uid,
                             CrashExceptionCode::CRASH_DUMP_EKILLED);
        return false;
    }
    return true;
}

void ProcessDumper::UnwindWriteJit(const ProcessDumpRequest &request)
{
    if (!isCrash_) {
        return;
    }

    const auto& jitCache = unwinder_->GetJitCache();
    if (jitCache.empty()) {
        return;
    }
    struct FaultLoggerdRequest jitRequest;
    (void)memset_s(&jitRequest, sizeof(jitRequest), 0, sizeof(jitRequest));
    jitRequest.type = FaultLoggerType::JIT_CODE_LOG;
    jitRequest.pid = request.pid;
    jitRequest.tid = request.tid;
    jitRequest.uid = request.uid;
    jitRequest.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    int32_t fd = RequestFileDescriptorEx(&jitRequest);
    if (fd == -1) {
        DFXLOG_ERROR("%s", "request jitlog fd failed.");
        return;
    }
    if (unwinder_->ArkWriteJitCodeToFile(fd) < 0) {
        DFXLOG_ERROR("%s", "jit code write file failed.");
    }
    (void)close(fd);
}

bool ProcessDumper::Unwind(std::shared_ptr<ProcessDumpRequest> request, int &dumpRes, pid_t vmPid)
{
    // dump unwind should still keep main thread or aim thread is frist unwind
    if (!isCrash_) {
        int tid = request->siginfo.si_value.sival_int;
        tid = tid != 0 ? tid : request->nsPid;
        for (auto &thread : process_->GetOtherThreads()) {
            if (thread->threadInfo_.nsTid == tid) {
                swap(process_->keyThread_, thread);
                break;
            }
        }
    }

    if (!DfxUnwindRemote::GetInstance().UnwindProcess(request, process_, unwinder_, vmPid)) {
        DFXLOG_ERROR("%s", "Failed to unwind process.");
        dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
        return false;
    }

    UnwindWriteJit(*request);
    return true;
}

int ProcessDumper::DumpProcess(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("DumpProcess");
    int dumpRes = DumpErrorCode::DUMP_ESUCCESS;
    do {
        if ((dumpRes = ReadRequestAndCheck(request)) != DumpErrorCode::DUMP_ESUCCESS) {
            break;
        }
        SetProcessdumpTimeout(request->siginfo);
        isCrash_ = request->siginfo.si_signo != SIGDUMP;
        bool isLeakDump = request->siginfo.si_signo == SIGLEAK_STACK;
        // We need check pid is same with getppid().
        // As in signal handler, current process is a child process, and target pid is our parent process.
        // If pid namespace is enabled, both ppid and pid are equal one.
        // In this case, we have to parse /proc/self/status
        if ((request->dumpMode == SPLIT_MODE) && (((!isCrash_) && (syscall(SYS_getppid) != request->nsPid)) ||
            ((isCrash_ || isLeakDump) && (syscall(SYS_getppid) != request->vmNsPid)))) {
            DFXLOG_ERROR("Target process(%s:%d) is not parent pid(%ld), exit processdump for signal(%d).",
                request->processName, request->nsPid, syscall(SYS_getppid), request->siginfo.si_signo);
            dumpRes = DumpErrorCode::DUMP_EGETPPID;
            break;
        }
        DFXLOG_INFO("Processdump SigVal(%d), TargetPid(%d:%d), TargetTid(%d), threadname(%s).",
            request->siginfo.si_value.sival_int, request->pid, request->nsPid, request->tid, request->threadName);

        if (InitProcessInfo(request) < 0) {
            DFXLOG_ERROR("%s", "Failed to init crash process info.");
            dumpRes = DumpErrorCode::DUMP_EATTACH;
            break;
        }
        InitRegs(request, dumpRes);
        pid_t vmPid = 0;
        if (!InitUnwinder(request, vmPid, dumpRes) && (isCrash_ && !isLeakDump)) {
            break;
        }
        if (InitPrintThread(request) < 0) {
            DFXLOG_ERROR("%s", "Failed to init print thread.");
            dumpRes = DumpErrorCode::DUMP_EGETFD;
        }
        ReadFdTable(*request);
        if (!Unwind(request, dumpRes, vmPid)) {
            DFXLOG_ERROR("%s", "unwind fail.");
        }
    } while (false);
    if (dumpRes == DumpErrorCode::DUMP_ESUCCESS && !IsTargetProcessAlive(request)) {
        dumpRes = DumpErrorCode::DUMP_EGETPPID;
    }
    return dumpRes;
}

bool ProcessDumper::InitVmThread(std::shared_ptr<ProcessDumpRequest> request)
{
    if (request == nullptr || process_ == nullptr) {
        return false;
    }
    if (isCrash_ && request->vmPid != 0) {
        if (getppid() != request->vmNsPid) {
            DFXLOG_ERROR("VM process(%d) should be parent pid.", request->vmNsPid);
            ReportCrashException(request->processName, request->pid, request->uid,
                                 CrashExceptionCode::CRASH_DUMP_EPARENTPID);
            return false;
        }
        process_->vmThread_ = DfxThread::Create(request->vmPid, request->vmPid, request->vmNsPid);
        if ((process_->vmThread_ == nullptr) || (!process_->vmThread_->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT))) {
            DFXLOG_ERROR("Failed to attach vm thread(%d).", request->vmNsPid);
            return false;
        }

        process_->vmThread_->SetThreadRegs(DfxRegs::CreateFromUcontext(request->context));
        process_->vmThread_->threadInfo_.threadName = std::string(request->threadName);
    }
    return true;
}

bool ProcessDumper::InitKeyThread(std::shared_ptr<ProcessDumpRequest> request)
{
    if (request == nullptr || process_ == nullptr) {
        return false;
    }
    pid_t nsTid = request->tid;
    pid_t tid = process_->ChangeTid(nsTid, true);
    process_->keyThread_ = DfxThread::Create(process_->processInfo_.pid, tid, nsTid);
    if ((process_->keyThread_ == nullptr) || (!process_->keyThread_->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT))) {
        DFXLOG_ERROR("Failed to attach key thread(%d).", nsTid);
        ReportCrashException(request->processName, request->pid, request->uid,
                             CrashExceptionCode::CRASH_DUMP_EATTACH);
        if (!isCrash_) {
            return false;
        }
    }
    if ((process_->keyThread_ != nullptr) && request->dumpMode == FUSION_MODE) {
        InfoRemoteProcessResult(request, OPE_SUCCESS);
        ptrace(PTRACE_SETOPTIONS, process_->keyThread_->threadInfo_.nsTid, NULL, PTRACE_O_TRACEFORK);
        ptrace(PTRACE_CONT, process_->keyThread_->threadInfo_.nsTid, 0, 0);
    }

    if ((process_->keyThread_ != nullptr) && (request->dumpMode == SPLIT_MODE) && !isCrash_) {
        process_->keyThread_->SetThreadRegs(DfxRegs::CreateFromUcontext(request->context));
    }

    if ((process_->keyThread_ != nullptr) && process_->keyThread_->threadInfo_.threadName.empty()) {
        process_->keyThread_->threadInfo_.threadName = std::string(request->threadName);
    }
    return true;
}

bool ProcessDumper::InitUnwinder(std::shared_ptr<ProcessDumpRequest> request, pid_t &vmPid, int &dumpRes)
{
    DFX_TRACE_SCOPED("InitUnwinder");
    pid_t realPid = 0;
    if (request->dumpMode == FUSION_MODE) {
        ReadPids(request, realPid, vmPid, process_);
        if (realPid == 0 || vmPid == 0) {
            ReportCrashException(request->processName, request->pid, request->uid,
                CrashExceptionCode::CRASH_DUMP_EREADPID);
            DFXLOG_ERROR("%s", "Failed to read real pid!");
            dumpRes = DumpErrorCode::DUMP_EREADPID;
            return false;
        }
    }

    if (request->dumpMode == FUSION_MODE) {
        unwinder_ = std::make_shared<Unwinder>(realPid, vmPid, isCrash_);
    } else {
        if (isCrash_) {
            unwinder_ = std::make_shared<Unwinder>(process_->vmThread_->threadInfo_.pid);
        } else {
            unwinder_ = std::make_shared<Unwinder>(process_->processInfo_.pid, false);
        }
    }
    if (unwinder_ == nullptr) {
        DFXLOG_ERROR("%s", "unwinder_ is nullptr!");
        return false;
    }
    if (unwinder_->GetMaps() == nullptr) {
        ReportCrashException(request->processName, request->pid, request->uid,
            CrashExceptionCode::CRASH_LOG_EMAPLOS);
        DFXLOG_ERROR("%s", "Mapinfo of crashed process is not exist!");
        dumpRes = DumpErrorCode::DUMP_ENOMAP;
        return false;
    }
    return true;
}

int ProcessDumper::InitProcessInfo(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("InitProcessInfo");
    if (request->pid <= 0) {
        return -1;
    }
    process_ = DfxProcess::Create(request->pid, request->nsPid);
    if (process_ == nullptr) {
        return -1;
    }
    if (process_->processInfo_.processName.empty()) {
        process_->processInfo_.processName = std::string(request->processName);
    }
    process_->processInfo_.uid = request->uid;
    process_->recycleTid_ = request->recycleTid;
    process_->SetFatalMessage(request->msg.body);

    if (!InitVmThread(request)) {
        return -1;
    }

    if (!InitKeyThread(request)) {
        return -1;
    }

    DFXLOG_INFO("%s", "ptrace attach all tids");
    bool isLeakDump = request->siginfo.si_signo == SIGLEAK_STACK;
    if (isCrash_ && !isLeakDump) {
        process_->InitOtherThreads();
        process_->Attach();
    } else {
        if (!isLeakDump) {
            process_->InitOtherThreads();
            if (request->dumpMode == FUSION_MODE) {
                process_->Attach();
            }
        }
    }
#if defined(PROCESSDUMP_MINIDEBUGINFO)
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
    return 0;
}

int ProcessDumper::GetLogTypeByRequest(const ProcessDumpRequest &request)
{
    switch (request.siginfo.si_signo) {
        case SIGLEAK_STACK:
            if (request.msg.type == MESSAGE_FDSAN_DEBUG) {
                return FaultLoggerType::CPP_STACKTRACE;
            }
            return FaultLoggerType::LEAK_STACKTRACE;
        case SIGDUMP:
            return FaultLoggerType::CPP_STACKTRACE;
        default:
            return FaultLoggerType::CPP_CRASH;
    }
}

int32_t ProcessDumper::CreateFileForCrash(int32_t pid, uint64_t time) const
{
    const std::string logFilePath = "/log/crash";
    const std::string logFileType = "cppcrash";
    const int32_t logcrashFileProp = 0640; // 0640:-rw-r-----
    if (access(logFilePath.c_str(), F_OK) != 0) {
        DFXLOG_ERROR("%s is not exist.", logFilePath.c_str());
        return INVALID_FD;
    }
    std::string logPath = logFilePath + "/" + logFileType + "-" + std::to_string(pid) + "-" + std::to_string(time);
    int32_t fd = OHOS_TEMP_FAILURE_RETRY(open(logPath.c_str(), O_RDWR | O_CREAT, logcrashFileProp));
    if (fd == INVALID_FD) {
        DFXLOG_ERROR("create %s failed, errno=%d", logPath.c_str(), errno);
    } else {
        DFXLOG_INFO("create crash path %s succ.", logPath.c_str());
    }
    return fd;
}

int ProcessDumper::InitPrintThread(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("InitPrintThread");
    int fd = -1;
    struct FaultLoggerdRequest faultloggerdRequest;
    (void)memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest));
    faultloggerdRequest.type = ProcessDumper::GetLogTypeByRequest(*request);
    faultloggerdRequest.pid = request->pid;
    faultloggerdRequest.tid = request->tid;
    faultloggerdRequest.uid = request->uid;
    faultloggerdRequest.time = request->timeStamp;
    isJsonDump_ = false;
    jsonFd_ = -1;
    if (isCrash_ || faultloggerdRequest.type == FaultLoggerType::LEAK_STACKTRACE) {
        if (DfxConfig::GetConfig().logPersist) {
            InitDebugLog((int)faultloggerdRequest.type, request->pid, request->tid, request->uid);
        }
        fd = RequestFileDescriptorEx(&faultloggerdRequest);
        if (fd == -1) {
            fd = CreateFileForCrash(request->pid, request->timeStamp);
        }
        DfxRingBufferWrapper::GetInstance().SetWriteFunc(ProcessDumper::WriteDumpBuf);
    } else {
        jsonFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_BUF);
        if (jsonFd_ < 0) {
            // If fd returns -1, we try to obtain the fd that needs to return JSON style
            fd = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE_BUF);
            resFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE_RES);
            DFXLOG_DEBUG("write buf fd: %d, write res fd: %d", fd, resFd_);
        } else {
            resFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES);
            DFXLOG_DEBUG("write json fd: %d, res fd: %d", jsonFd_, resFd_);
        }
    }
    if (jsonFd_ > 0) {
        isJsonDump_ = true;
    }
    if ((fd < 0) && (jsonFd_ < 0)) {
        DFXLOG_WARN("%s", "Failed to request fd from faultloggerd.");
        ReportCrashException(request->processName, request->pid, request->uid,
                             CrashExceptionCode::CRASH_DUMP_EWRITEFD);
    }
    if (!isJsonDump_) {
        DfxRingBufferWrapper::GetInstance().SetWriteBufFd(fd);
    }
    DfxRingBufferWrapper::GetInstance().StartThread();
    return isJsonDump_ ? jsonFd_ : fd;
}

int ProcessDumper::WriteDumpBuf(int fd, const char* buf, const int len)
{
    if (buf == nullptr) {
        return -1;
    }
    return WriteLog(fd, "%s", buf);
}

void ProcessDumper::WriteDumpRes(int32_t res)
{
    DFXLOG_INFO("%s :: res: %d", __func__, res);
    if (resFd_ > 0) {
        ssize_t nwrite = OHOS_TEMP_FAILURE_RETRY(write(resFd_, &res, sizeof(res)));
        if (nwrite < 0) {
            DFXLOG_ERROR("%s write fail, err:%d", __func__, errno);
        }
        close(resFd_);
        resFd_ = -1;
    } else {
        if (res != DUMP_ESUCCESS) {
            DfxRingBufferWrapper::GetInstance().AppendMsg("Result:\n");
            DfxRingBufferWrapper::GetInstance().AppendMsg(DfxDumpRes::ToString(res) + "\n");
        }
    }
}

bool ProcessDumper::IsCrash() const
{
    return isCrash_;
}

void ProcessDumper::ReportSigDumpStats(const std::shared_ptr<ProcessDumpRequest> &request) const
{
    if (isCrash_) {
        return;
    }

    std::vector<uint8_t> buf(sizeof(struct FaultLoggerdStatsRequest), 0);
    auto stat = reinterpret_cast<struct FaultLoggerdStatsRequest*>(buf.data());
    stat->type = PROCESS_DUMP;
    stat->pid = request->pid;
    stat->signalTime = request->timeStamp;
    stat->processdumpStartTime = startTime_;
    stat->processdumpFinishTime = finishTime_;
    if (memcpy_s(stat->targetProcess, sizeof(stat->targetProcess),
        request->processName, sizeof(request->processName)) != 0) {
        DFXLOG_ERROR("Failed to copy target processName (%d)", errno);
        return;
    }

    ReportDumpStats(stat);
}

void ProcessDumper::ReportCrashInfo(const std::string& jsonInfo)
{
    if (reporter_ != nullptr) {
        reporter_->SetCppCrashInfo(jsonInfo);
        reporter_->ReportToHiview();
        reporter_->ReportToAbilityManagerService();
    }
}

void ProcessDumper::ReadFdTable(const ProcessDumpRequest &request)
{
    bool isLeakDump = (request.siginfo.si_signo == SIGLEAK_STACK) && (request.msg.type != MESSAGE_FDSAN_DEBUG);
    if (isCrash_ && !isLeakDump) {
        process_->openFiles = GetOpenFiles(request.pid, request.nsPid, request.fdTableAddr);
        reporter_ = std::make_shared<CppCrashReporter>(request.timeStamp, process_, request.dumpMode);
    }
}
} // namespace HiviewDFX
} // namespace OHOS
