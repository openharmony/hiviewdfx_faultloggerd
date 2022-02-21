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

/* This files is writer log to file module on process dump module. */

#include "dfx_dump_writer.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <faultloggerd_client.h>
#include <securec.h>

#include "cppcrash_reporter.h"
#include "dfx_log.h"
#include "dfx_process.h"

namespace OHOS {
namespace HiviewDFX {
ProcessDumpRequest::ProcessDumpRequest()
{
    DfxLogDebug("Enter %s.", __func__);
    memset_s(&siginfo_, sizeof(siginfo_), 0, sizeof(siginfo_));
    memset_s(&context_, sizeof(context_), 0, sizeof(context_));
    DfxLogDebug("Exit %s.", __func__);
}

ProcessDumpType ProcessDumpRequest::GetType() const
{
    return type_;
}

void ProcessDumpRequest::SetType(ProcessDumpType type)
{
    type_ = type;
}

int32_t ProcessDumpRequest::GetTid() const
{
    return tid_;
}

void ProcessDumpRequest::SetTid(int32_t tid)
{
    tid_ = tid;
}

int32_t ProcessDumpRequest::GetPid() const
{
    return pid_;
}

void ProcessDumpRequest::SetPid(int32_t pid)
{
    pid_ = pid;
}

int32_t ProcessDumpRequest::GetUid() const
{
    return uid_;
}

void ProcessDumpRequest::SetUid(int32_t uid)
{
    uid_ = uid;
}

uint64_t ProcessDumpRequest::GetReserved() const
{
    return reserved_;
}

void ProcessDumpRequest::SetReserved(uint64_t reserved)
{
    reserved_ = reserved;
}

uint64_t ProcessDumpRequest::GetTimeStamp() const
{
    return timeStamp_;
}

void ProcessDumpRequest::SetTimeStamp(uint64_t timeStamp)
{
    timeStamp_ = timeStamp;
}

siginfo_t ProcessDumpRequest::GetSiginfo() const
{
    return siginfo_;
}

void ProcessDumpRequest::SetSiginfo(siginfo_t &siginfo)
{
    siginfo_ = siginfo;
}

ucontext_t ProcessDumpRequest::GetContext() const
{
    return context_;
}

void ProcessDumpRequest::SetContext(ucontext_t const &context)
{
    context_ = context;
}

DfxDumpWriter::DfxDumpWriter(std::shared_ptr<DfxProcess> process, int32_t fromSignalHandler)
{
    DfxLogDebug("Enter %s.", __func__);
    process_ = process;
    fromSignalHandler_ = fromSignalHandler;
    DfxLogDebug("Exit %s.", __func__);
}

void DfxDumpWriter::WriteProcessDump(std::shared_ptr<ProcessDumpRequest> request)
{
    DfxLogDebug("Enter %s.", __func__);
    if (!process_ || !request) {
        DfxLogError("Have no process or request is null.");
        return;
    }

    if (fromSignalHandler_ == 0) {
        process_->PrintProcess(STDOUT_FILENO, false);
    } else {
        struct FaultLoggerdRequest faultloggerdRequest;
        if (memset_s(&faultloggerdRequest, sizeof(faultloggerdRequest), 0, sizeof(struct FaultLoggerdRequest)) != 0) {
            DfxLogError("memset_s error.");
            return;
        }
        faultloggerdRequest.type = (request->GetSiginfo().si_signo == SIGDUMP) ?
            (int32_t)FaultLoggerType::CPP_STACKTRACE : (int32_t)FaultLoggerType::CPP_CRASH;
        faultloggerdRequest.pid = request->GetPid();
        faultloggerdRequest.tid = request->GetTid();
        faultloggerdRequest.uid = request->GetUid();
        faultloggerdRequest.time = request->GetTimeStamp();
        if (strncpy_s(faultloggerdRequest.module, sizeof(faultloggerdRequest.module),
            process_->GetProcessName().c_str(), process_->GetProcessName().length()) != 0) {
            DfxLogWarn("Failed to set process name.");
            return;
        }

        int32_t targetFd = RequestFileDescriptorEx(&faultloggerdRequest);
        if (targetFd < 0) {
            DfxLogWarn("Failed to request fd from faultloggerd.");
            return;
        }
        auto siginfo = std::make_shared<siginfo_t>(request->GetSiginfo());
        if (process_->GetIsSignalDump() == false) {
            process_->PrintProcessWithSiginfo(siginfo, targetFd);
            CppCrashReporter reporter(faultloggerdRequest.time, request->GetSiginfo().si_signo, process_);
            reporter.ReportToHiview();
        } else {
            process_->PrintProcess(targetFd, false);
        }
        close(targetFd);
    }
    DfxLogDebug("Exit %s.", __func__);
}
} // namespace HiviewDFX
} // namespace OHOS
