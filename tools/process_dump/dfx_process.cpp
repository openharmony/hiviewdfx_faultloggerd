/*
 * Copyright (c) 2021-2025 Huawei Device Co., Ltd.
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

#include "dfx_process.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <securec.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <vector>

#include "process_dump_config.h"
#include "crash_exception.h"
#include "dfx_define.h"

#include "dfx_buffer_writer.h"
#include "dfx_log.h"
#include "dfx_signal.h"
#include "dfx_util.h"
#include "procinfo.h"

namespace OHOS {
namespace HiviewDFX {
void DfxProcess::InitProcessInfo(pid_t pid, pid_t nsPid, uid_t uid, const std::string& processName)
{
    processInfo_.pid = pid;
    processInfo_.nsPid = nsPid;
    processInfo_.uid = uid;
    processInfo_.processName = processName;
}

bool DfxProcess::InitKeyThread(const ProcessDumpRequest& request)
{
    pid_t nsTid = request.tid;
    pid_t tid = ChangeTid(nsTid, true);
    keyThread_ = DfxThread::Create(processInfo_.pid, tid, nsTid, request.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH);
    if (!keyThread_->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT)) {
        DFXLOGE("Failed to attach key thread(%{public}d).", nsTid);
        ReportCrashException(CrashExceptionCode::CRASH_DUMP_EATTACH);
        if (request.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) {
            return false;
        }
    }
    if (keyThread_->GetThreadInfo().threadName.empty()) {
        keyThread_->SetThreadName(std::string(request.threadName));
    }
    keyThread_->SetThreadRegs(DfxRegs::CreateFromUcontext(request.context));
    if (request.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH) { // dumpcatch set target thread to key thread
        pid_t dumpCatchTargetTid = request.siginfo.si_value.sival_int == 0 ?
            request.nsPid : request.siginfo.si_value.sival_int;
        DFXLOGE("dumpCatchTargetTid(%{public}d).", dumpCatchTargetTid);
        if (dumpCatchTargetTid != tid) {
            otherThreads_.emplace_back(keyThread_);
            keyThread_ = DfxThread::Create(processInfo_.pid, dumpCatchTargetTid, dumpCatchTargetTid, true);
            if (keyThread_ != nullptr && keyThread_->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT)) {
                keyThread_->SetThreadRegs(DfxRegs::CreateRemoteRegs(dumpCatchTargetTid));
            }
        }
    }
    return true;
}

bool DfxProcess::InitOtherThreads(const ProcessDumpRequest& request)
{
    std::vector<int> tids;
    std::vector<int> nstids;
    if (!GetTidsByPid(processInfo_.pid, tids, nstids)) {
        return false;
    }
    pid_t keyThreadNsTid = 0;
    if (keyThread_ != nullptr) {
        keyThreadNsTid = keyThread_->GetThreadInfo().nsTid;
    }
    for (size_t i = 0; i < nstids.size(); ++i) {
        // KeyThread and requestThread have been initialized, skip directly
        if ((nstids[i] == keyThreadNsTid) || nstids[i] == request.tid) {
            continue;
        }
        auto thread = DfxThread::Create(processInfo_.pid, tids[i], nstids[i],
            request.type == ProcessDumpType::DUMP_TYPE_DUMP_CATCH);
        if (thread->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT)) {
            thread->SetThreadRegs(DfxRegs::CreateRemoteRegs(thread->GetThreadInfo().nsTid));
        }
        otherThreads_.push_back(thread);
    }
    return true;
}

pid_t DfxProcess::ChangeTid(pid_t tid, bool ns)
{
    if (processInfo_.pid == processInfo_.nsPid) {
        return tid;
    }

    if (kvThreads_.empty()) {
        std::vector<int> tids;
        std::vector<int> nstids;
        if (!GetTidsByPid(processInfo_.pid, tids, nstids)) {
            return tid;
        }
        for (size_t i = 0; i < nstids.size(); ++i) {
            kvThreads_[nstids[i]] = tids[i];
        }
    }

    for (auto iter = kvThreads_.begin(); iter != kvThreads_.end(); iter++) {
        if (ns && (iter->first == tid)) {
            return iter->second;
        }
        if (!ns && (iter->second == tid)) {
            return iter->first;
        }
    }
    return tid;
}

std::vector<std::shared_ptr<DfxThread>>& DfxProcess::GetOtherThreads()
{
    return otherThreads_;
}

void DfxProcess::ClearOtherThreads()
{
    otherThreads_.clear();
}

void DfxProcess::Attach(bool hasKey)
{
    if (hasKey && keyThread_) {
        keyThread_->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT);
    }

    if (otherThreads_.empty()) {
        return;
    }
    for (auto& thread : otherThreads_) {
        if (thread->GetThreadInfo().nsTid == processInfo_.nsPid) {
            thread->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT);
            continue;
        }
        thread->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT);
    }
}

void DfxProcess::Detach()
{
    if (keyThread_ != nullptr) {
        keyThread_->Detach();
    }

    for (const auto& thread : otherThreads_) {
        if (thread != nullptr) {
            thread->Detach();
        }
    }
}

void DfxProcess::AppendFatalMessage(const std::string &msg)
{
    fatalMsg_ += msg;
}

const std::string& DfxProcess::GetFatalMessage() const
{
    return fatalMsg_;
}
} // namespace HiviewDFX
} // namespace OHOS
