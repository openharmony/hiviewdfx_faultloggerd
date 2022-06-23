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

#include "cppcrash_reporter.h"

#define OHOS_TEMP_FAILURE_RETRY(exp)            \
    ({                                     \
    long int _rc;                          \
    do {                                   \
        _rc = (long int)(exp);             \
    } while ((_rc == -1) && (errno == EINTR)); \
    _rc;                                   \
    })

namespace OHOS {
namespace HiviewDFX {

std::condition_variable ProcessDumper::backTracePrintCV;
std::mutex ProcessDumper::backTracePrintMutx;

ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper dumper;
    return dumper;
}

void ProcessDumper::LoopPrintBackTraceInfo()
{
    std::unique_lock<std::mutex> lck(backTracePrintMutx);
    while (true) {
        bool hasFinished = ProcessDumper::GetInstance().backTraceIsFinished_;
        auto available = ProcessDumper::GetInstance().backTraceRingBuffer_.Available();
        auto item = ProcessDumper::GetInstance().backTraceRingBuffer_.Read(available);
        DfxLogDebug("%s :: available(%d), hasFinished(%d)", __func__, available, hasFinished);

        if (available != 0) {
            if (item.At(0).empty()) {
                ProcessDumper::GetInstance().backTraceRingBuffer_.Skip(item.Length());
                continue;
            }

            for (unsigned int i = 0; i < item.Length(); i++) {
                DfxLogDebug("%s :: [%d]print: %s\n", __func__, i, item.At(i).c_str());
                WriteLog(ProcessDumper::GetInstance().backTraceFileFd_, "%s", item.At(i).c_str());
            }
            ProcessDumper::GetInstance().backTraceRingBuffer_.Skip(item.Length());
        } else {
            if (hasFinished) {
                DfxLogDebug("%s :: print finished, exit loop.\n", __func__);
                break;
            }

            backTracePrintCV.wait_for(lck, std::chrono::milliseconds(BACK_TRACE_RING_BUFFER_PRINT_WAIT_TIME_MS));
        }
    }
}

void ProcessDumper::PrintDumpProcessMsg(std::string msg)
{
    DfxLogDebug("%s :: msg(%s)", __func__, msg.c_str());
    backTraceRingBuffer_.Append(msg);
    backTracePrintCV.notify_one();
}

int ProcessDumper::PrintDumpProcessBuf(const char *format, ...)
{
    int ret = -1;
    char buf[LOG_BUF_LEN] = {0};
    (void)memset_s(&buf, sizeof(buf), 0, sizeof(buf));
    va_list args;
    va_start(args, format);
    ret = vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format, args);
    va_end(args);
    if (ret <= 0) {
        DfxLogError("snprintf_s failed.");
    }
    PrintDumpProcessMsg(std::string(buf));
    return ret;
}

void ProcessDumper::PrintDumpProcessWithSignalContextHeader(std::shared_ptr<DfxProcess> process, siginfo_t info,
                                                            const std::string& msg)
{
    PrintDumpProcessBuf("Pid:%d\n", process->GetPid());
    PrintDumpProcessBuf("Uid:%d\n", process->GetUid());
    PrintDumpProcessBuf("Process name:%s\n", process->GetProcessName().c_str());

    if (info.si_signo != SIGDUMP) {
        PrintDumpProcessBuf("Reason:");

        PrintDumpProcessMsg(PrintSignal(info));

        if (info.si_signo == SIGABRT && !msg.empty()) {
            PrintDumpProcessBuf("LastFatalMessage:%s\n", msg.c_str());
        }

        if (process->GetThreads().size() != 0) {
            PrintDumpProcessBuf("Fault thread Info:\n");
        }
    }
}

void ProcessDumper::InitPrintThread(int32_t fromSignalHandler, std::shared_ptr<ProcessDumpRequest> request, \
    std::shared_ptr<DfxProcess> process)
{
    if (fromSignalHandler == 0) {
        backTraceFileFd_ = STDOUT_FILENO;
    } else {
        struct FaultLoggerdRequest faultloggerdRequest;
        if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
            DfxLogError("memset_s error.");
            return;
        }

        int32_t signo = request->GetSiginfo().si_signo;
        faultloggerdRequest.type = (signo == SIGDUMP) ?
            (int32_t)FaultLoggerType::CPP_STACKTRACE : (int32_t)FaultLoggerType::CPP_CRASH;
        faultloggerdRequest.pid = request->GetPid();
        faultloggerdRequest.tid = request->GetTid();
        faultloggerdRequest.uid = request->GetUid();
        faultloggerdRequest.time = request->GetTimeStamp();
        if (strncpy_s(faultloggerdRequest.module, sizeof(faultloggerdRequest.module),
            process->GetProcessName().c_str(), sizeof(faultloggerdRequest.module) - 1) != 0) {
            DfxLogWarn("Failed to set process name.");
            return;
        }

        backTraceFileFd_ = RequestFileDescriptorEx(&faultloggerdRequest);
        if (backTraceFileFd_ < 0) {
            DfxLogWarn("Failed to request fd from faultloggerd.");
        }

        if (signo != SIGDUMP) {
            reporter_ = std::make_shared<CppCrashReporter>(faultloggerdRequest.time, signo, process);
        }
    }

    backTracePrintThread_ = std::thread(ProcessDumper::LoopPrintBackTraceInfo);
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

    bool isCrashRequest = (request->GetSiginfo().si_signo != SIGDUMP);
    FaultLoggerType type = isCrashRequest ? FaultLoggerType::CPP_CRASH : FaultLoggerType::CPP_STACKTRACE;
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

    if (isCrashRequest) {
        process->SetIsSignalDump(false);
        PrintDumpProcessMsg("Timestamp:" + GetCurrentTimeStr(request->GetTimeStamp()));
    } else {
        process->SetIsSignalDump(true);
        PrintDumpProcessMsg("Timestamp:" + GetCurrentTimeStr());
    }

    process->InitOtherThreads(isCrashRequest);
    process->SetUid(request->GetUid());
    process->SetIsSignalHdlr(true);

    InitPrintThread(true, request, process);
    PrintDumpProcessWithSignalContextHeader(process, request->GetSiginfo(), request->GetLastFatalMessage());

    DfxUnwindRemote::GetInstance().UnwindProcess(process);

    if (getppid() != request->GetPid()) {
        DfxLogError("after unwind, check again: Target process(%s:%d) is not our parent(%d), exit processdump for signal(%d).",
            storeProcessName.c_str(), request->GetPid(), getppid(), request->GetSiginfo().si_signo);
        return;
    }
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
    backTraceIsFinished_ = false;
    std::shared_ptr<ProcessDumpRequest> request = std::make_shared<ProcessDumpRequest>();
    if (!request) {
        DfxLogError("Fail to create dump request.");
        return;
    }

    DfxLogDebug("isSignalHdlr(%d), type(%d), pid(%d), tid(%d).", isSignalHdlr, type, pid, tid);

    std::shared_ptr<DfxProcess> process = nullptr;
    if (isSignalHdlr) {
        DumpProcessWithSignalContext(process, request);
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

    backTraceIsFinished_ = true;
    backTracePrintCV.notify_one();
    if (backTracePrintThread_.joinable()) {
        backTracePrintThread_.join();
    }
    close(backTraceFileFd_);
    backTraceFileFd_ = -1;

    if (isSignalHdlr && process && !process->GetIsSignalDump()) {
        process->Detach();
    }

    CloseDebugLog();
    _exit(0);
}

} // namespace HiviewDFX
} // namespace OHOS
