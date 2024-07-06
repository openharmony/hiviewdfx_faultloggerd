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

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <securec.h>
#include <string>
#include <syscall.h>
#include <sys/types.h>
#include <sys/ptrace.h>
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
#include "faultloggerd_client.h"
#include "dfx_trace.h"
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


void ReadPids(int& realPid, int& vmPid)
{
    pid_t pids[PID_MAX] = {0};
    OHOS_TEMP_FAILURE_RETRY(read(STDIN_FILENO, pids, sizeof(pids)));
    realPid = pids[REAL_PROCESS_PID];
    vmPid = pids[VIRTUAL_PROCESS_PID];
    DFXLOG_INFO("procecdump get real pid is %d vm pid is %d", realPid, vmPid);
}

void InfoRemoteProcessResult(std::shared_ptr<ProcessDumpRequest> request, int result, int type)
{
    if (request == nullptr) {
        return;
    }
    if (request->pmPipeFd[0] != -1) {
        close(request->pmPipeFd[0]);
        request->pmPipeFd[0] = -1;
    }
    switch (type) {
        case MAIN_PROCESS:
            OHOS_TEMP_FAILURE_RETRY(write(request->pmPipeFd[1], &result, sizeof(result)));
            break;
        case VIRTUAL_PROCESS:
            OHOS_TEMP_FAILURE_RETRY(write(request->vmPipeFd[1], &result, sizeof(result)));
            break;
        default:
            break;
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
        if (process_->keyThread_ != nullptr) {
            process_->keyThread_->Detach();
        }
        if (isCrash_ && (request->dumpMode == FUSION_MODE) && IsBlockCrashProcess()) {
            DFXLOG_INFO("start block crash process pid %d nspid %d", request->pid, request->nsPid);
            if (syscall(SYS_tgkill, request->nsPid, request->tid, SIGSTOP) != 0) {
                DFXLOG_ERROR("send signal stop to nsPid %d fail %s", request->nsPid, strerror(errno));
            }
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

    WriteDumpRes(resDump_);
    DfxRingBufferWrapper::GetInstance().StopThread();
    DFXLOG_INFO("Finish dump stacktrace for %s(%d:%d).", request->processName, request->pid, request->tid);
    CloseDebugLog();
    if (resDump_ != DumpErrorCode::DUMP_ENOMAP) {
        ReportCrashInfo(jsonInfo);
    }
    if ((request->dumpMode == FUSION_MODE) && isCrash_) {
        InfoRemoteProcessResult(request, OPE_CONTINUE, MAIN_PROCESS);
    }
}

static int32_t ReadRequestAndCheck(std::shared_ptr<ProcessDumpRequest> request)
{
    DFX_TRACE_SCOPED("ReadRequestAndCheck");
    ssize_t readCount = OHOS_TEMP_FAILURE_RETRY(read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest)));
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
    uint32_t opeResult = OPE_SUCCESS;
    if (request->dumpMode == FUSION_MODE) {
        if (!DfxUnwindRemote::GetInstance().InitProcessAllThreadRegs(request, process_)) {
            DFXLOG_ERROR("%s", "Failed to init process regs.");
            dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
            opeResult = OPE_FAIL;
        }

        InfoRemoteProcessResult(request, opeResult, MAIN_PROCESS);
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

bool ProcessDumper::Unwind(std::shared_ptr<ProcessDumpRequest> request, int &dumpRes, pid_t &vmPid)
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
    uint32_t opeResult = OPE_SUCCESS;
    do {
        if ((dumpRes = ReadRequestAndCheck(request)) != DumpErrorCode::DUMP_ESUCCESS) {
            break;
        }
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
            opeResult = OPE_FAIL;
            break;
        }
        if (InitPrintThread(request) < 0) {
            DFXLOG_ERROR("%s", "Failed to init print thread.");
            dumpRes = DumpErrorCode::DUMP_EGETFD;
        }
        if (isCrash_ && !isLeakDump) {
            process_->openFiles = GetOpenFiles(request->pid, request->nsPid, request->fdTableAddr);
            reporter_ = std::make_shared<CppCrashReporter>(request->timeStamp, process_, request->dumpMode);
        }
        if (!Unwind(request, dumpRes, vmPid)) {
            opeResult = OPE_FAIL;
        }
    } while (false);
    if (request->dumpMode == FUSION_MODE) {
        InfoRemoteProcessResult(request, opeResult, VIRTUAL_PROCESS);
    }
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
        ReadPids(realPid, vmPid);
    }
    // frezze detach after vm process create
    if (!isCrash_) {
        if (process_->keyThread_ != nullptr) {
            process_->keyThread_->Detach();
        }
        process_->Detach();
        DFXLOG_INFO("%s", "ptrace detach all tids");
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
    process_->SetFatalMessage(request->lastFatalMessage);

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

int ProcessDumper::GetLogTypeBySignal(int sig)
{
    switch (sig) {
        case SIGLEAK_STACK:
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
    std::stringstream ss;
    ss << logFilePath << "/" << logFileType << "-" << pid << "-" << time;
    std::string logPath = ss.str();
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
    faultloggerdRequest.type = ProcessDumper::GetLogTypeBySignal(request->siginfo.si_signo);
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
} // namespace HiviewDFX
} // namespace OHOS
