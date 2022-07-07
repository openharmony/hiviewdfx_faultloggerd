/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
#include <memory>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <securec.h>
#include <string>

#include <faultloggerd_client.h>
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_log.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "dfx_unwind_remote.h"
#include "dfx_util.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_dump_res.h"
#include "cppcrash_reporter.h"

namespace OHOS {
namespace HiviewDFX {
ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper ins;
    return ins;
}

void ProcessDumper::PrintDumpProcessWithSignalContextHeader(std::shared_ptr<DfxProcess> process, siginfo_t info,
                                                            const std::string& msg)
{
    DfxRingBufferWrapper::GetInstance().AppendBuf("Pid:%d\n", process->GetPid());
    DfxRingBufferWrapper::GetInstance().AppendBuf("Uid:%d\n", process->GetUid());
    DfxRingBufferWrapper::GetInstance().AppendBuf("Process name:%s\n", process->GetProcessName().c_str());

    if (info.si_signo != SIGDUMP) {
        DfxRingBufferWrapper::GetInstance().AppendBuf("Reason:");

        DfxRingBufferWrapper::GetInstance().AppendMsg(PrintSignal(info));

        if (info.si_signo == SIGABRT && !msg.empty()) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("LastFatalMessage:%s\n", msg.c_str());
        }

        if (process->GetThreads().size() != 0) {
            DfxRingBufferWrapper::GetInstance().AppendBuf("Fault thread Info:\n");
        }
    }
}

void ProcessDumper::InitPrintThread(int32_t fromSignalHandler, std::shared_ptr<ProcessDumpRequest> request, \
    std::shared_ptr<DfxProcess> process)
{
    int fd = -1;
    if (fromSignalHandler == 0) {
        fd = STDOUT_FILENO;
        DfxRingBufferWrapper::GetInstance().SetWriteFunc(ProcessDumper::WriteDumpBuf);
    } else {
        int32_t pid = request->GetPid();
		int32_t signo = request->GetSiginfo().si_signo;
        bool isCrash = (signo != SIGDUMP);
        FaultLoggerType type = isCrash ? FaultLoggerType::CPP_CRASH : FaultLoggerType::CPP_STACKTRACE;

        struct FaultLoggerdRequest faultloggerdRequest;
        if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
            DfxLogError("memset_s error.");
            return;
        }
        
        if (isCrash) {
            faultloggerdRequest.type = (int32_t)type;
            faultloggerdRequest.pid = request->GetPid();
            faultloggerdRequest.tid = request->GetTid();
            faultloggerdRequest.uid = request->GetUid();
            faultloggerdRequest.time = request->GetTimeStamp();
            if (strncpy_s(faultloggerdRequest.module, sizeof(faultloggerdRequest.module),
                process->GetProcessName().c_str(), sizeof(faultloggerdRequest.module) - 1) != 0) {
                DfxLogWarn("Failed to set process name.");
                return;
            }
            fd = RequestFileDescriptorEx(&faultloggerdRequest);

            DfxRingBufferWrapper::GetInstance().SetWriteFunc(ProcessDumper::WriteDumpBuf);
            reporter_ = std::make_shared<CppCrashReporter>(request->GetTimeStamp(), signo, process);
        } else {
            fd = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_WRITE_BUF);
            DfxLogDebug("write buf fd: %d", fd);

            resFd_ = RequestPipeFd(pid, FaultLoggerPipeType::PIPE_FD_WRITE_RES);
            DfxLogDebug("write res fd: %d", resFd_);
        }

        if (fd < 0) {
            DfxLogWarn("Failed to request fd from faultloggerd.");
        }
    }

    DfxRingBufferWrapper::GetInstance().SetWriteBufFd(fd);
    DfxRingBufferWrapper::GetInstance().StartThread();
}



void ProcessDumper::DumpProcessWithSignalContext(std::shared_ptr<DfxProcess> &process,
                                                 std::shared_ptr<ProcessDumpRequest> request)
{
    ssize_t readCount = read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest));
    if (readCount != static_cast<long>(sizeof(ProcessDumpRequest))) {
        DfxLogError("Fail to read DumpRequest(%d).", errno);
        return;
    }
    std::string storeThreadName = request->GetThreadNameString();
    std::string storeProcessName = request->GetProcessNameString();

    bool isCrash = (request->GetSiginfo().si_signo != SIGDUMP);
    FaultLoggerType type = isCrash ? FaultLoggerType::CPP_CRASH : FaultLoggerType::CPP_STACKTRACE;
    bool isLogPersist = DfxConfig::GetInstance().GetLogPersist();
    InitDebugLog((int)type, request->GetPid(), request->GetTid(), request->GetUid(), isLogPersist);
    // We need check pid is same with getppid().
    // As in signal handler, current process is a child process, and target pid is our parent process.
    if (getppid() != request->GetPid()) {
        DfxLogError("Target process(%s:%d) is not our parent(%d), exit processdump for signal(%d).",
            storeProcessName.c_str(), request->GetPid(), getppid(), request->GetSiginfo().si_signo);
        return;
    }

    std::shared_ptr<DfxThread> keyThread = std::make_shared<DfxThread>(request->GetPid(),
                                                                       request->GetTid(),
                                                                       request->GetContext());
    if (!keyThread->Attach()) {
        DfxLogError("Fail to attach key thread.");
        return;
    }

    keyThread->SetIsCrashThread(true);
    if ((keyThread->GetThreadName()).empty()) {
        keyThread->SetThreadName(storeThreadName);
    }

    process = DfxProcess::CreateProcessWithKeyThread(request->GetPid(), keyThread);
    if (!process) {
        DfxLogError("Fail to init process with key thread.");
        return;
    }

    if ((process->GetProcessName()).empty()) {
        process->SetProcessName(storeProcessName);
    }

    if (isCrash) {
        process->SetIsSignalDump(false);
        DfxRingBufferWrapper::GetInstance().AppendMsg("Timestamp:" + GetCurrentTimeStr(request->GetTimeStamp()));
    } else {
        process->SetIsSignalDump(true);
        DfxRingBufferWrapper::GetInstance().AppendMsg("Timestamp:" + GetCurrentTimeStr());
    }

    process->InitOtherThreads(isCrash);
    process->SetUid(request->GetUid());
    process->SetIsSignalHdlr(true);

    InitPrintThread(true, request, process);
    PrintDumpProcessWithSignalContextHeader(process, request->GetSiginfo(), request->GetLastFatalMessage());

    DfxUnwindRemote::GetInstance().UnwindProcess(process);
}

void ProcessDumper::DumpProcessWithPidTid(std::shared_ptr<DfxProcess> &process, \
                                          std::shared_ptr<ProcessDumpRequest> request)
{
    FaultLoggerType type = FaultLoggerType::CPP_STACKTRACE;
    bool isLogPersist = DfxConfig::GetInstance().GetLogPersist();
    if (isLogPersist) {
        InitDebugLog((int)type, request->GetPid(), request->GetTid(), request->GetUid(), isLogPersist);
    } else {
        int devNull = OHOS_TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
        if (devNull < 0) {
            std::cout << "Failed to open dev/null." << std::endl;
        } else {
            OHOS_TEMP_FAILURE_RETRY(dup2(devNull, STDERR_FILENO));
        }
    }

    if (request->GetType() == DUMP_TYPE_PROCESS) {
        process = DfxProcess::CreateProcessWithKeyThread(request->GetPid(), nullptr);
        if (process) {
            process->InitOtherThreads(false);
        }
    } else if (request->GetType() == DUMP_TYPE_THREAD) {
        process = DfxProcess::CreateProcessWithKeyThread(request->GetTid(), nullptr);
    } else {
        DfxLogError("dump type is not support.");
        return;
    }

    if (!process) {
        DfxLogError("Fail to init key thread.");
        return;
    }

    process->SetIsSignalDump(true);
    process->SetIsSignalHdlr(false);
    InitPrintThread(false, nullptr, process);

    DfxUnwindRemote::GetInstance().UnwindProcess(process);
}

void ProcessDumper::Dump(bool isSignalHdlr, ProcessDumpType type, int32_t pid, int32_t tid)
{
    std::shared_ptr<ProcessDumpRequest> request = std::make_shared<ProcessDumpRequest>();
    if (!request) {
        DfxLogError("Fail to create dump request.");
        return;
    }

    DfxLogDebug("isSignalHdlr(%d), type(%d), pid(%d), tid(%d).", isSignalHdlr, type, pid, tid);

    std::shared_ptr<DfxProcess> process = nullptr;
    if (isSignalHdlr) {
        DumpProcessWithSignalContext(process, request);
        if (getppid() != request->GetPid()) {
            resDump_ = ProcessDumpRes::DUMP_EGETPPID;
            DfxRingBufferWrapper::GetInstance().AppendBuf("after unwind, check again: \
                Target process(%s:%d) is not parent pid(%d)\n.", \
                request->GetProcessNameString().c_str(), request->GetPid(), getppid());
        }
    } else {
        if (type == DUMP_TYPE_PROCESS) {
            request->SetPid(pid);
        } else {
            request->SetPid(pid);
            request->SetTid(tid);
        }
        request->SetType(type);
        DumpProcessWithPidTid(process, request);
    }

    if (process == nullptr) {
        DfxLogError("Dump process failed, please check permission and whether pid is valid.");
    } else {
        if (!isSignalHdlr || (isSignalHdlr && process->GetIsSignalDump())) {
            process->Detach();
        }
    }

    if (reporter_ != nullptr) {
        reporter_->ReportToHiview();
    }

    DfxRingBufferWrapper::GetInstance().StopThread();
    WriteDumpRes(resDump_);

    if (isSignalHdlr && process && !process->GetIsSignalDump()) {
        process->Detach();
    }

    CloseDebugLog();
    _exit(0);
}

int ProcessDumper::WriteDumpBuf(int fd, const char* buf, const int len)
{
    if ((fd < 0) || (buf == nullptr)) {
        return -1;
    }
    return WriteLog(fd, "%s", buf);
}

void ProcessDumper::WriteDumpRes(int32_t res)
{
    if (resFd_ < 0) {
        return;
    }
    DfxLogDebug("%s :: res: %d", __func__, res);
    DumpResMsg dumpResMsg;
    dumpResMsg.res = res;
    const char* strRes = DfxDumpRes::GetInstance().GetResStr(res);
    if (strncpy_s(dumpResMsg.strRes, sizeof(dumpResMsg.strRes), strRes, strlen(strRes)) != 0) {
        DfxLogError("%s :: strncpy failed.", __func__);
    }
    write(resFd_, &dumpResMsg, sizeof(struct DumpResMsg));
}

} // namespace HiviewDFX
} // namespace OHOS
