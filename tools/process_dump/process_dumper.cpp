/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

/* This files contains process dump main module. */

#include "process_dumper.h"

#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <fcntl.h>
#include <pthread.h>
#include <syscall.h>
#include <ucontext.h>
#include <unistd.h>
#include <securec.h>

#include "faultloggerd_client.h"
#include "cppcrash_reporter.h"
#include "printer.h"
#include "procinfo.h"
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "dfx_dump_res.h"
#include "dfx_logger.h"
#include "dfx_process.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_thread.h"
#include "dfx_unwind_remote.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper ins;
    return ins;
}

int ProcessDumper::DumpProcess(std::shared_ptr<ProcessDumpRequest> request)
{
    int dumpRes = DumpErrorCode::DUMP_ESUCCESS;
    do {
        ssize_t readCount = read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest));
        if (readCount != static_cast<long>(sizeof(ProcessDumpRequest))) {
            DFXLOG_ERROR("Fail to read DumpRequest(%d).", errno);
            dumpRes = DumpErrorCode::DUMP_EREADREQUEST;
            break;
        }

        isCrash_ = (request->siginfo.si_signo != SIGDUMP);
        // We need check pid is same with getppid().
        // As in signal handler, current process is a child process, and target pid is our parent process.
        // If pid namespace is enalbed, both ppid and pid are equal one.
        // In this case, we have to parse /proc/self/status
        if ((!isCrash_ && (syscall(SYS_getppid) != request->pid)) ||
            (isCrash_ && (syscall(SYS_getppid) != request->vmPid))) {
            DFXLOG_ERROR("Target process(%s:%d) is not parent pid(%d), exit processdump for signal(%d).",
                request->processName, request->pid, syscall(SYS_getppid), request->siginfo.si_signo);
            dumpRes = DumpErrorCode::DUMP_EGETPPID;
            break;
        }
        DFXLOG_INFO("Processdump SigVal:%d, TargetPid:%d, TargetTid:%d.",
            request->siginfo.si_value.sival_int, request->pid, request->tid);

        InitNsInfo(request);

        CreateVmProcessIfNeed();

        if (InitProcessInfo(request) < 0) {
            DFXLOG_ERROR("Failed to init crash process info.");
            dumpRes = DumpErrorCode::DUMP_EATTACH;
            break;
        }

        if (InitPrintThread(request) < 0) {
            DFXLOG_ERROR("Failed to init print thread.");
            dumpRes = DumpErrorCode::DUMP_EGETFD;
        }

        if (isCrash_) {
            reporter_ = std::make_shared<CppCrashReporter>(request->timeStamp, request->siginfo, targetProcess_);
        }

        Printer::GetInstance().PrintDumpHeader(request, targetProcess_);
        if (DfxUnwindRemote::GetInstance().UnwindProcess(targetProcess_) == false) {
            DFXLOG_ERROR("Failed to unwind process.");
            dumpRes = DumpErrorCode::DUMP_ESTOPUNWIND;
        }

        if (syscall(SYS_getppid) != GetTargetNsPid()) {
            DfxRingBufferWrapper::GetInstance().AppendMsg(
                "Target process has been killed, the crash log may not be fully generated.");
            dumpRes = DumpErrorCode::DUMP_EGETPPID;
            break;
        }
    } while (false);

    return dumpRes;
}

bool ProcessDumper::InitNsInfo(std::shared_ptr<ProcessDumpRequest> request)
{
    bool ret = false;
    targetPid_ = request->pid;
    targetNsPid_ = targetPid_;
    targetVmPid_ = request->vmPid;
    targetVmNsPid_ = targetVmPid_;
    if (targetPid_ != 1) {
        return ret;
    }

    ProcInfo procInfo;
    (void)memset_s(&procInfo, sizeof(procInfo), 0, sizeof(struct ProcInfo));
    if (!GetProcStatus(procInfo)) {
        return ret;
    }

    if (!isCrash_) {
        if (procInfo.ppid == 0) { // real init case
            targetPid_ = 1;
            targetNsPid_ = 1;
            ret = false;
        } else {
            targetPid_ = procInfo.ppid;
            targetNsPid_ = getppid();
            ret = true;
        }
        DFXLOG_INFO("Dump in targetPid:%d targetNsPid:%d.", targetPid_, targetNsPid_);
    } else {
        targetVmPid_ = procInfo.ppid;
        targetVmNsPid_ = getppid();
        DFXLOG_INFO("Crash in vmPid:%d nsVmPid:%d.", targetVmPid_, targetVmNsPid_);
        (void)memset_s(&procInfo, sizeof(procInfo), 0, sizeof(struct ProcInfo));
        if (!GetProcStatusByPid(targetVmPid_, procInfo)) {
            DFXLOG_ERROR("Get targetVmPid:%d status failed.", targetVmPid_);
            return ret;
        }
        ret = true;
        targetPid_ = procInfo.ppid;
        targetNsPid_ = request->pid;
        DFXLOG_INFO("Crash in targetPid:%d targetNsPid:%d.", targetPid_, targetNsPid_);
    }

    request->pid = targetPid_;
    return ret;
}

void ProcessDumper::CreateVmProcessIfNeed()
{
    if (targetVmNsPid_ <= 0) {
        return;
    }
    if (getppid() != targetVmNsPid_) {
        DFXLOG_ERROR("VM Process(%d) should be our parent pid.", targetVmNsPid_);
        return;
    }

    std::shared_ptr<DfxThread> vmThread = DfxThread::Create(targetVmPid_, targetVmPid_, targetVmNsPid_);
    if (!vmThread->Attach()) {
        DFXLOG_ERROR("Fail to attach vm thread.");
        return;
    }
    vmProcess_ = DfxProcess::CreateProcessWithKeyThread(targetVmPid_, targetVmNsPid_, vmThread);
}

int ProcessDumper::InitProcessInfo(std::shared_ptr<ProcessDumpRequest> request)
{
    // if Nspid is enabled, target tid and real tid should be paresed from /proc/pid/task
    int dumpTid = request->siginfo.si_value.sival_int;
    int nsTid = request->tid;

    std::shared_ptr<DfxThread> keyThread = isCrash_ ?
        std::make_shared<DfxThread>(targetPid_, targetPid_, nsTid, request->context) :
        std::make_shared<DfxThread>(targetPid_, dumpTid == 0 ? targetPid_ : dumpTid, nsTid);
    if (!keyThread->Attach()) {
        DFXLOG_ERROR("Fail to attach key thread(%d).", nsTid);
        if (!isCrash_ || vmProcess_ == nullptr) {
            return -1;
        }
    }

    keyThread->threadInfo_.isKeyThread = true;
    if (keyThread->threadInfo_.threadName.empty()) {
        keyThread->threadInfo_.threadName = std::string(request->threadName);
    }

    targetProcess_ = DfxProcess::CreateProcessWithKeyThread(targetPid_, targetNsPid_, keyThread);
    if (!targetProcess_) {
        return -1;
    }

    if (targetProcess_->processInfo_.processName.empty()) {
        targetProcess_->processInfo_.processName = std::string(request->processName);
    }

    targetProcess_->processInfo_.uid = request->uid;
    targetProcess_->processInfo_.recycleTid = request->recycleTid;
    targetProcess_->InitProcessMaps();
    targetProcess_->SetFatalMessage(request->lastFatalMessage);

    if (dumpTid == 0) {
        targetProcess_->InitOtherThreads(isCrash_);
    }
    return 0;
}

int ProcessDumper::InitPrintThread(std::shared_ptr<ProcessDumpRequest> request)
{
    int fd = -1;
    bool isCrash = (request->siginfo.si_signo != SIGDUMP);
    FaultLoggerType type = isCrash ? FaultLoggerType::CPP_CRASH : FaultLoggerType::CPP_STACKTRACE;

    struct FaultLoggerdRequest faultloggerdRequest;
    (void)memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest));

    if (isCrash) {
        if (DfxConfig::GetConfig().logPersist) {
            InitDebugLog((int)type, request->pid, request->tid, request->uid);
        }

        faultloggerdRequest.type = (int32_t)type;
        faultloggerdRequest.pid = request->pid;
        faultloggerdRequest.tid = request->tid;
        faultloggerdRequest.uid = request->uid;
        faultloggerdRequest.time = request->timeStamp;
        fd = RequestFileDescriptorEx(&faultloggerdRequest);

        DfxRingBufferWrapper::GetInstance().SetWriteFunc(ProcessDumper::WriteDumpBuf);
    } else {
        fd = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE_BUF);
        DFXLOG_DEBUG("write buf fd: %d", fd);

        resFd_ = RequestPipeFd(request->pid, FaultLoggerPipeType::PIPE_FD_WRITE_RES);
        DFXLOG_DEBUG("write res fd: %d", resFd_);
    }

    if (fd < 0) {
        DFXLOG_WARN("Failed to request fd from faultloggerd.");
    }

    DfxRingBufferWrapper::GetInstance().SetWriteBufFd(fd);
    DfxRingBufferWrapper::GetInstance().StartThread();
    return fd;
}

void ProcessDumper::Dump()
{
    std::shared_ptr<ProcessDumpRequest> request = std::make_shared<ProcessDumpRequest>();
    resDump_ = DumpProcess(request);
    if (targetProcess_ == nullptr) {
        DFXLOG_ERROR("Dump process failed, please check permission and whether pid is valid.");
    } else {
        if (!isCrash_) {
            targetProcess_->Detach();
        }
    }
    if (vmProcess_ != nullptr) {
        vmProcess_->Detach();
    }

    WriteDumpRes(resDump_);
    DfxRingBufferWrapper::GetInstance().StopThread();
    DFXLOG_INFO("Finish dump stacktrace for %s(%d:%d).", request->processName, request->pid, request->tid);
    CloseDebugLog();

    // check dump result ?
    if (reporter_ != nullptr) {
        reporter_->ReportToHiview();
    }

    _exit(0);
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
    } else {
        if (res != DUMP_ESUCCESS) {
            DfxRingBufferWrapper::GetInstance().AppendMsg("Result:\n");
            DfxRingBufferWrapper::GetInstance().AppendMsg(DfxDumpRes::ToString(res) + "\n");
        }
    }
}

int32_t ProcessDumper::GetTargetPid()
{
    if (vmProcess_ != nullptr) {
        return vmProcess_->processInfo_.pid;
    }

    if (targetProcess_ != nullptr) {
        return targetProcess_->processInfo_.pid;
    }
    return -1;
}

int32_t ProcessDumper::GetTargetNsPid()
{
    if (vmProcess_ != nullptr) {
        return vmProcess_->processInfo_.nsPid;
    }

    if (targetProcess_ != nullptr) {
        return targetProcess_->processInfo_.nsPid;
    }
    return -1;
}

bool ProcessDumper::IsCrash()
{
    return isCrash_;
}
} // namespace HiviewDFX
} // namespace OHOS
