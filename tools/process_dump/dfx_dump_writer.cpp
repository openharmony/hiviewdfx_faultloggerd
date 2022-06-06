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

/* This files is writer log to file module on process dump module. */

#include "dfx_dump_writer.h"

#include <cinttypes>
#include <csignal>
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
    errno_t err = memset_s(&siginfo_, sizeof(siginfo_), 0, sizeof(siginfo_));
    if (err != EOK) {
        DfxLogError("%s :: memset_s siginfo_ failed.", __func__);
    }
    err = memset_s(&context_, sizeof(context_), 0, sizeof(context_));
    if (err != EOK) {
        DfxLogError("%s :: memset_s context_ failed.", __func__);
    }
    err = memset_s(&threadName_, sizeof(threadName_), 0, sizeof(threadName_));
    if (err != EOK) {
        DfxLogError("%s :: memset_s threadName_ failed.", __func__);
    }
    err = memset_s(&processName_, sizeof(processName_), 0, sizeof(processName_));
    if (err != EOK) {
        DfxLogError("%s :: memset_s processName_ failed.", __func__);
    }
    err = memset_s(&lastFatalMessage_, sizeof(lastFatalMessage_), 0, sizeof(lastFatalMessage_));
    if (err != EOK) {
        DfxLogError("%s :: memset_s lastFatalMessage_ failed.", __func__);
    }
    type_ = DUMP_TYPE_PROCESS;
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

std::string ProcessDumpRequest::GetThreadNameString() const
{
    std::string threadName(threadName_, sizeof(threadName_) - 1);
    return threadName;
}

std::string ProcessDumpRequest::GetProcessNameString() const
{
    std::string processName(processName_, sizeof(processName_) - 1);
    return processName;
}

std::string ProcessDumpRequest::GetLastFatalMessage() const
{
    std::string lastMsg(lastFatalMessage_, sizeof(lastFatalMessage_) - 1);
    return lastMsg;
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
} // namespace HiviewDFX
} // namespace OHOS
