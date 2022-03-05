/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include <faultloggerd_client.h>
#include <fcntl.h>
#include <iostream>

#include "dfx_dump_writer.h"
#include "dfx_log.h"
#include "dfx_process.h"
#include "dfx_thread.h"
#include "dfx_unwind_remote.h"
#include "dfx_config.h"

namespace OHOS {
namespace HiviewDFX {

void ProcessDumper::DumpProcessWithSignalContext(std::shared_ptr<DfxProcess> &process,
                                                 std::shared_ptr<ProcessDumpRequest> request)
{
    DfxLogDebug("Enter %s.", __func__);
    ssize_t readCount = read(STDIN_FILENO, request.get(), sizeof(ProcessDumpRequest));
    if (readCount != sizeof(ProcessDumpRequest)) {
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
    if (!keyThread) {
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
    DfxUnwindRemote::GetInstance().UnwindProcess(process);
    DfxLogDebug("Exit %s.", __func__);
}

void ProcessDumper::DumpProcess(std::shared_ptr<DfxProcess> &process,
                                std::shared_ptr<ProcessDumpRequest> request)
{
    DfxLogDebug("Enter %s.", __func__);
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

    process->SetIsSignalHdlr(false);
    DfxUnwindRemote::GetInstance().UnwindProcess(process);
    DfxLogDebug("Exit %s.", __func__);
}

ProcessDumper &ProcessDumper::GetInstance()
{
    static ProcessDumper dumper;
    return dumper;
}

void ProcessDumper::Dump(bool isSignalHdlr, ProcessDumpType type, int32_t pid, int32_t tid)
{
    DfxLogDebug("Enter %s.", __func__);
    std::shared_ptr<ProcessDumpRequest> request = std::make_shared<ProcessDumpRequest>();
    if (!request) {
        DfxLogError("Fail to create dump request.");
        return;
    }

    DfxLogDebug("isSignalHdlr(%d), type(%d), pid(%d), tid(%d).", isSignalHdlr, type, pid, tid);

    std::shared_ptr<DfxProcess> process = nullptr;
    int32_t fromSignalHandler = 0;
    if (isSignalHdlr) {
        DumpProcessWithSignalContext(process, request);
        fromSignalHandler = 1;
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
            InitDebugLog((int)type, request->GetPid(), request->GetTid(), request->GetUid(),isLogPersist);
        } else {
            int devNull = TEMP_FAILURE_RETRY(open("/dev/null", O_RDWR));
            if (devNull < 0) {
                std::cout << "Failed to open dev/null." << std::endl;
            } else {
                TEMP_FAILURE_RETRY(dup2(devNull, STDERR_FILENO));
            }
        }

        DumpProcess(process, request);
    }

    if (process == nullptr) {
        DfxLogError("process == nullptr");
    } else {
        process->Detach();
        DfxDumpWriter dumpWriter(process, fromSignalHandler);
        dumpWriter.WriteProcessDump(request);
    }
    DfxLogDebug("Exit %s.", __func__);

    CloseDebugLog();
}

} // namespace HiviewDFX
} // namespace OHOS
