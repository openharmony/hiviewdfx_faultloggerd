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
#include <ucontext.h>
#include <unistd.h>

#include "cppcrash_reporter.h"
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
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "procinfo.h"
#include "crash_exception.h"

#if defined(__x86_64__)
#include "dfx_unwind_remote_emulator.h"
#include "printer_emulator.h"
#else
#include "dfx_unwind_remote.h"
#include "printer.h"
#include "unwinder_config.h"
#endif

namespace OHOS {
namespace HiviewDFX {
namespace {
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

const int ARG_MAX_NUM = 131072;
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

void FillFdsaninfo(OpenFilesList &list, pid_t pid, uint64_t fdTableAddr)
{
    constexpr size_t fds = sizeof(FdTable::entries) / sizeof(*FdTable::entries);
    size_t entryOffset = offsetof(FdTable, entries);
    for (size_t i = 0; i < fds; i++) {
        uint64_t addr = fdTableAddr + entryOffset + sizeof(FdEntry) * i;
        FdEntry entry;
        if (DfxMemory::ReadProcMemByPid(pid, addr, &entry, sizeof(entry)) != sizeof(entry)) {
            DFXLOG_ERROR("%s", "read pid mem error");
            return;
        }
        if (entry.close_tag) {
            list[i].fdsanOwner = entry.close_tag;
        }
    }
    size_t overflowOffset = offsetof(FdTable, overflow);
    uintptr_t overflow = 0;
    uint64_t tmp = fdTableAddr + overflowOffset;
    if (DfxMemory::ReadProcMemByPid(pid, tmp, &overflow, sizeof(overflow)) != sizeof(overflow)) {
        return;
    }
    if (!overflow) {
        return;
    }

    size_t overflowLength;
    if (DfxMemory::ReadProcMemByPid(pid, overflow, &overflowLength, sizeof(overflowLength)) != sizeof(overflowLength)) {
        return;
    }
    if (overflowLength > ARG_MAX_NUM) {
        return;
    }
    for (size_t i = 0; i < overflowLength; i++) {
        int fd = i + fds;
        uint64_t address = overflow + offsetof(FdTableOverflow, entries) + sizeof(FdEntry) * i;
        FdEntry entry;
        if (DfxMemory::ReadProcMemByPid(pid, address, &entry, sizeof(entry)) != sizeof(entry)) {
            return;
        }
        if (entry.close_tag) {
            list[fd].fdsanOwner = entry.close_tag;
        }
    }
}

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
            openFiles += "OpenFilesList  contain an entry (fd " + std::to_string(fd) + ") with no path or owner\n";
        }
    }
    return openFiles;
}
}

ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper ins;
    return ins;
}

void ProcessDumper::Dump()
{
    std::shared_ptr<ProcessDumpRequest> request = std::make_shared<ProcessDumpRequest>();
    resDump_ = DumpProcess(request);
    if (process_ == nullptr) {
        DFXLOG_ERROR("%s", "Dump process failed, please check permission and whether pid is valid.");
    } else {
        if (isCrash_) {
            if (process_->vmThread_ != nullptr) {
                process_->vmThread_->Detach();
            }
            process_->Detach();
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

    WriteDumpRes(resDump_);
    DfxRingBufferWrapper::GetInstance().StopThread();
    DFXLOG_INFO("Finish dump stacktrace for %s(%d:%d).", request->processName, request->pid, request->tid);
    CloseDebugLog();
    // check dump result
    if (reporter_ != nullptr) {
        reporter_->SetValue("stackInfo", jsonInfo);
        reporter_->ReportToHiview();
        reporter_->ReportToAbilityManagerService();
    }
}

static int32_t ReadRequestAndCheck(std::shared_ptr<ProcessDumpRequest> request)
{
    ssize_t readCount = read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest));
    if (readCount != static_cast<long>(sizeof(ProcessDumpRequest))) {
        DFXLOG_ERROR("Failed to read DumpRequest(%d).", errno);
        ReportCrashException(request->processName, request->pid, request->uid,
                             GetTimeMillisec(), CrashExceptionCode::CRASH_DUMP_EREADREQ);
        return DumpErrorCode::DUMP_EREADREQUEST;
    }

    return DumpErrorCode::DUMP_ESUCCESS;
}

std::string GetOpenFiles(int32_t pid, uint64_t fdTableAddr)
{
    OpenFilesList openFies;
    CollectOpenFiles(openFies, pid);
    FillFdsaninfo(openFies, pid, fdTableAddr);
    std::string fds = DumpOpenFiles(openFies);
    return fds;
}

int ProcessDumper::DumpProcess(std::shared_ptr<ProcessDumpRequest> request)
{
    int dumpRes = DumpErrorCode::DUMP_ESUCCESS;
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
        if (((!isCrash_) && (syscall(SYS_getppid) != request->nsPid)) ||
            ((isCrash_ || isLeakDump) && (syscall(SYS_getppid) != request->vmNsPid))) {
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
        process_->openFiles = GetOpenFiles(request->pid, request->fdTableAddr);
        if (InitPrintThread(request) < 0) {
            DFXLOG_ERROR("%s", "Failed to init print thread.");
            dumpRes = DumpErrorCode::DUMP_EGETFD;
        }

        if (isCrash_ && !isLeakDump) {
            reporter_ = std::make_shared<CppCrashReporter>(request->timeStamp, process_);
        }

#if defined(__x86_64__)
        if (!DfxUnwindRemote::GetInstance().UnwindProcess(request, process_)) {
            Printer::PrintDumpHeader(request, process_);
#else
        if (!DfxUnwindRemote::GetInstance().UnwindProcess(request, process_, unwinder_)) {
#endif
            DFXLOG_ERROR("%s", "Failed to unwind process.");
            dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
        }

        if ((!isCrash_ && (syscall(SYS_getppid) != request->nsPid)) ||
            (isCrash_ && (syscall(SYS_getppid) != request->vmNsPid))) {
            DfxRingBufferWrapper::GetInstance().AppendMsg(
                "Target process has been killed, the crash log may not be fully generated.");
            dumpRes = DumpErrorCode::DUMP_EGETPPID;
            ReportCrashException(request->processName, request->pid, request->uid,
                                 GetTimeMillisec(), CrashExceptionCode::CRASH_DUMP_EKILLED);
            break;
        }
    } while (false);
    return dumpRes;
}

int ProcessDumper::InitProcessInfo(std::shared_ptr<ProcessDumpRequest> request)
{
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

    if (isCrash_ && request->vmPid != 0) {
        if (getppid() != request->vmNsPid) {
            DFXLOG_ERROR("VM process(%d) should be parent pid.", request->vmNsPid);
            ReportCrashException(request->processName, request->pid, request->uid,
                                 GetTimeMillisec(), CrashExceptionCode::CRASH_DUMP_EPARENTPID);
            return -1;
        }
        process_->vmThread_ = DfxThread::Create(request->vmPid, request->vmPid, request->vmNsPid);
        if ((process_->vmThread_ == nullptr) || (!process_->vmThread_->Attach())) {
            DFXLOG_ERROR("Failed to attach vm thread(%d).", request->vmNsPid);
            return -1;
        }

#if defined(__x86_64__)
        process_->vmThread_->SetThreadRegs(DfxRegs::CreateFromContext(request->context));
#else
        process_->vmThread_->SetThreadRegs(DfxRegs::CreateFromUcontext(request->context));
#endif
        process_->vmThread_->threadInfo_.threadName = std::string(request->threadName);
    }

    pid_t dumpTid = request->siginfo.si_value.sival_int;
    pid_t nsTid = request->tid;
    pid_t tid = process_->ChangeTid(nsTid, true);
    if (!isCrash_) {
        if (dumpTid != 0 && dumpTid != tid && (IsThreadInPid(process_->processInfo_.pid, dumpTid))) {
            tid = dumpTid;
            nsTid = process_->ChangeTid(tid, false);
        }
    }
    process_->keyThread_ = DfxThread::Create(process_->processInfo_.pid, tid, nsTid);
    if ((process_->keyThread_ == nullptr) || (!process_->keyThread_->Attach())) {
        DFXLOG_ERROR("Failed to attach key thread(%d).", nsTid);
        ReportCrashException(request->processName, request->pid, request->uid,
                             GetTimeMillisec(), CrashExceptionCode::CRASH_DUMP_EATTACH);
        if (!isCrash_) {
            return -1;
        }
    }
#if !defined(__x86_64__)
    if (!isCrash_) {
        process_->keyThread_->SetThreadRegs(DfxRegs::CreateFromUcontext(request->context));
    }
#endif
    if ((process_->keyThread_ != nullptr) && process_->keyThread_->threadInfo_.threadName.empty()) {
        process_->keyThread_->threadInfo_.threadName = std::string(request->threadName);
    }

    if (isCrash_) {
        process_->InitOtherThreads();
        process_->Attach();
    } else {
        if (dumpTid == 0) {
            process_->InitOtherThreads();
        }
    }
#if !defined(__x86_64__)
    if (isCrash_) {
        unwinder_ = std::make_shared<Unwinder>(process_->vmThread_->threadInfo_.pid, true);
    } else {
        unwinder_ = std::make_shared<Unwinder>(process_->processInfo_.pid);
    }
#if defined(PROCESSDUMP_MINIDEBUGINFO)
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    UnwinderConfig::SetEnableLoadSymbolLazily(true);
#endif
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

int ProcessDumper::InitPrintThread(std::shared_ptr<ProcessDumpRequest> request)
{
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
        DfxRingBufferWrapper::GetInstance().SetWriteFunc(ProcessDumper::WriteDumpBuf);
    } else {
        fd = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE_BUF);
        DFXLOG_DEBUG("write buf fd: %d", fd);
        if (fd < 0) {
            // If fd returns -1, we try to obtain the fd that needs to return JSON style
            jsonFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_BUF);
            resFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_JSON_WRITE_RES);
            DFXLOG_DEBUG("write json fd: %d, res fd: %d", jsonFd_, resFd_);
        } else {
            resFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE_RES);
            DFXLOG_DEBUG("write res fd: %d", resFd_);
        }
    }
    if (jsonFd_ > 0) {
        isJsonDump_ = true;
    }
    if ((fd < 0) && (jsonFd_ < 0)) {
        DFXLOG_WARN("%s", "Failed to request fd from faultloggerd.");
        ReportCrashException(request->processName, request->pid, request->uid,
                             GetTimeMillisec(), CrashExceptionCode::CRASH_DUMP_EWRITEFD);
    }

    if (!isJsonDump_) {
        DfxRingBufferWrapper::GetInstance().SetWriteBufFd(fd);
    }
    DfxRingBufferWrapper::GetInstance().StartThread();
    if (isJsonDump_) {
        return jsonFd_;
    } else {
        return fd;
    }
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
    DFXLOG_DEBUG("%s :: res: %d", __func__, res);
    if (resFd_ > 0) {
        write(resFd_, &res, sizeof(res));
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
} // namespace HiviewDFX
} // namespace OHOS
