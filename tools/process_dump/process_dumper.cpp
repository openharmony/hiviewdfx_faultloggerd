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
#include "dfx_dump_writer.h"
#include "dfx_log.h"
#include "dfx_process.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "dfx_unwind_remote.h"

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

static const std::string DUMP_STACK_TAG_FAILED = "failed:";
std::condition_variable ProcessDumper::backTracePrintCV;
std::mutex ProcessDumper::backTracePrintMutx;

void LoopPrintBackTraceInfo()
{
    DfxLogDebug("Enter %s.", __func__);
    std::unique_lock<std::mutex> lck(ProcessDumper::backTracePrintMutx);
    while (true) {
        bool hasFinished = ProcessDumper::GetInstance().backTraceIsFinished_;
        unsigned int available = ProcessDumper::GetInstance().backTraceRingBuffer_.Available();
        DfxRingBufferBlock<std::string> item = \
            ProcessDumper::GetInstance().backTraceRingBuffer_.Read(available);
        DfxLogDebug("%s :: available(%d), item.Length(%d) -1.", __func__, available, item.Length());
        if ((available == 0) && hasFinished) {
            DfxLogDebug("%s :: print finished, exit loop -1.\n", __func__);
            break;
        } else if (available != 0) {
            for (unsigned int i = 0; i < item.Length(); i++) {
                DfxLogDebug("%s :: print: %s\n", __func__, item.At(i).c_str());
                WriteLog(ProcessDumper::GetInstance().backTraceFileFd_, "%s", item.At(i).c_str());
            }
            ProcessDumper::GetInstance().backTraceRingBuffer_.Skip(item.Length());
        } else {
            ProcessDumper::backTracePrintCV.wait_for(
                lck, std::chrono::milliseconds(BACK_TRACE_RING_BUFFER_PRINT_WAIT_TIME_MS));
        }
    }
    DfxLogDebug("Exit %s.", __func__);
}

void ProcessDumper::PrintDumpProcessMsg(std::string msg)
{
    DfxLogDebug("Enter %s, msg(%s).", __func__, msg.c_str());
    backTraceRingBuffer_.Append(msg);
    backTracePrintCV.notify_one();
    DfxLogDebug("Exit %s.", __func__);
}

void ProcessDumper::PrintDumpProcessWithSignalContextHeader(std::shared_ptr<DfxProcess> process, siginfo_t info,
                                                            const std::string& msg)
{
    DfxLogDebug("Enter %s.", __func__);
    char buf[LOG_BUF_LEN] = {0};
    int ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Pid:%d\n", process->GetPid());
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    PrintDumpProcessMsg(std::string(buf));
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: msmset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Uid:%d\n", process->GetUid());
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    PrintDumpProcessMsg(std::string(buf));
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: msmset_s failed, line: %d.", __func__, __LINE__);
    }

    ret = snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "Process name:%s\n", process->GetProcessName().c_str());
    if (ret <= 0) {
        DfxLogError("%s :: snprintf_s failed, line: %d.", __func__, __LINE__);
    }
    PrintDumpProcessMsg(std::string(buf));
    ret = memset_s(buf, LOG_BUF_LEN, '\0', LOG_BUF_LEN);
    if (ret != EOK) {
        DfxLogError("%s :: msmset_s failed, line: %d.", __func__, __LINE__);
    }

    if (info.si_signo != SIGDUMP) {
        std::string reason = "Reason:";
        PrintDumpProcessMsg(reason);

        PrintDumpProcessMsg(PrintSignal(info));

        if (info.si_signo == SIGABRT && !msg.empty()) {
            PrintDumpProcessMsg("LastFatalMessage:" + msg);
            PrintDumpProcessMsg("\n");
        }

        if (process->GetThreads().size() != 0) {
            PrintDumpProcessMsg("Fault thread Info:\n");
        }
    }

    DfxLogDebug("Exit %s.", __func__);
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

    backTracePrintThread_ = std::thread(LoopPrintBackTraceInfo);
}



void ProcessDumper::DumpProcessWithSignalContext(std::shared_ptr<DfxProcess> &process,
                                                 std::shared_ptr<ProcessDumpRequest> request)
{
    DfxLogDebug("Enter %s.", __func__);
    ssize_t readCount = read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest));
    if (readCount != static_cast<long>(sizeof(ProcessDumpRequest))) {
        DfxLogError("Fail to read DumpRequest(%d).", errno);
        return;
    }
    std::string storeThreadName = request->GetThreadNameString();
    std::string storeProcessName = request->GetProcessNameString();
    FaultLoggerType type = (request->GetSiginfo().si_signo == SIGDUMP) ?
        FaultLoggerType::CPP_STACKTRACE : FaultLoggerType::CPP_CRASH;
    bool isLogPersist = DfxConfig::GetInstance().GetLogPersist();
    InitDebugLog((int)type, request->GetPid(), request->GetTid(), request->GetUid(), isLogPersist);
    // We need check pid is same with getppid().
    // As in signal handler, current process is a child process, and target pid is our parent process.
    if (getppid() != request->GetPid()) {
        DfxLogError("Target pid is not our parent process, some un-expected happened.");
        return;
    }

    std::shared_ptr<DfxThread> keyThread = std::make_shared<DfxThread>(request->GetPid(),
                                                                       request->GetTid(),
                                                                       request->GetContext());
    if (!keyThread || !keyThread->IsThreadInititalized()) {
        DfxLogError("Fail to init key thread.");
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
        process->UpdateProcessName(storeProcessName);
    }

    if (request->GetSiginfo().si_signo != SIGDUMP) {
        process->SetIsSignalDump(false);
    } else {
        process->SetIsSignalDump(true);
    }

    process->InitOtherThreads();
    process->SetUid(request->GetUid());
    process->SetIsSignalHdlr(true);

    InitPrintThread(true, request, process);
    PrintDumpProcessWithSignalContextHeader(process, request->GetSiginfo(), request->GetLastFatalMessage());

    DfxUnwindRemote::GetInstance().UnwindProcess(process);
    DfxLogDebug("Exit %s.", __func__);
}

void ProcessDumper::DumpProcess(std::shared_ptr<DfxProcess> &process,
                                std::shared_ptr<ProcessDumpRequest> request)
{
    DfxLogDebug("Enter %s.", __func__);
    if (request != nullptr) {
        if (request->GetType() == DUMP_TYPE_PROCESS) {
        process = DfxProcess::CreateProcessWithKeyThread(request->GetPid(), nullptr);
        if (process) {
            process->InitOtherThreads();
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
    
    DfxLogDebug("Exit %s.", __func__);
}

ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper dumper;
    return dumper;
}

void ProcessDumper::PrintDumpFailed()
{
    std::cout << DUMP_STACK_TAG_FAILED << std::endl;
    std::cout << "Dump failed, please check permission and whether pid is valid." << std::endl;
}

void ProcessDumper::Dump(bool isSignalHdlr, ProcessDumpType type, int32_t pid, int32_t tid)
{
    DfxLogDebug("Enter %s.", __func__);
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

        DumpProcess(process, request);
    }

    if (process == nullptr) {
        DfxLogError("process == nullptr");
        PrintDumpFailed();
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
    backTracePrintThread_.join();
    close(backTraceFileFd_);
    backTraceFileFd_ = -1;

    if (isSignalHdlr && process && !process->GetIsSignalDump()) {
        process->Detach();
    }
    DfxLogInfo("processdump :: finished write crash info to file.");
    DfxLogDebug("Exit %s.", __func__);

    CloseDebugLog();
}

} // namespace HiviewDFX
} // namespace OHOS
