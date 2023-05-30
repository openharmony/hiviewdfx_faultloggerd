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

/* This files contains process dump thread module. */

#include "dfx_thread.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>
#include <securec.h>
#include <sstream>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_fault_stack.h"
#include "dfx_frame_format.h"
#include "dfx_logger.h"
#include "dfx_regs.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
DfxThread::DfxThread(pid_t pid, pid_t tid, pid_t nsTid, const ucontext_t &context)
    :isCrashThread_(false), pid_(pid), tid_(tid), nsTid_(nsTid), unwStopReason_(-1)
{
    threadStatus_ = ThreadStatus::THREAD_STATUS_INVALID;
    std::shared_ptr<DfxRegs> regs = DfxRegs::CreateFromContext(context);
    regs_ = regs;

    ReadThreadName(tid_, threadName_);
    threadStatus_ = ThreadStatus::THREAD_STATUS_INIT;
}

DfxThread::DfxThread(pid_t pid, pid_t tid, pid_t nsTid)
    :isCrashThread_(false), pid_(pid), tid_(tid), nsTid_(nsTid), unwStopReason_(-1)
{
    regs_ = nullptr;
    ReadThreadName(tid_, threadName_);
    threadStatus_ = ThreadStatus::THREAD_STATUS_INIT;
}

bool DfxThread::IsThreadInitialized()
{
    return threadStatus_ == ThreadStatus::THREAD_STATUS_ATTACHED;
}

DfxThread::~DfxThread()
{
    threadStatus_ = ThreadStatus::THREAD_STATUS_INVALID;
}

bool DfxThread::GetIsCrashThread() const
{
    return isCrashThread_;
}

void DfxThread::SetIsCrashThread(bool isCrashThread)
{
    isCrashThread_ = isCrashThread;
}

pid_t DfxThread::GetProcessId() const
{
    return pid_;
}

pid_t DfxThread::GetThreadId() const
{
    return tid_;
}

std::string DfxThread::GetThreadName() const
{
    return threadName_;
}

void DfxThread::SetThreadName(const std::string &threadName)
{
    threadName_ = threadName;
}

std::shared_ptr<DfxRegs> DfxThread::GetThreadRegs() const
{
    return regs_;
}

std::vector<std::shared_ptr<DfxFrame>> DfxThread::GetThreadDfxFrames() const
{
    return dfxFrames_;
}

void DfxThread::SetThreadRegs(const std::shared_ptr<DfxRegs> &regs)
{
    regs_ = regs;
}

std::shared_ptr<DfxFrame> DfxThread::GetAvailableFrame()
{
    std::shared_ptr<DfxFrame> frame = std::make_shared<DfxFrame>();
    dfxFrames_.push_back(frame);
    return frame;
}

void DfxThread::PrintThread(const int32_t fd, bool isSignalDump)
{
    if (dfxFrames_.size() == 0) {
        DFXLOG_WARN("No frame print for tid %d.", tid_);
        return;
    }

    PrintThreadBacktraceByConfig(fd);
    if (isSignalDump == false) {
        PrintThreadRegisterByConfig();
        PrintThreadFaultStackByConfig();
    }
}

void DfxThread::SetThreadUnwStopReason(int reason)
{
    unwStopReason_ = reason;
}

void DfxThread::CreateFaultStack(int32_t vmPid)
{
    faultstack_ = std::unique_ptr<FaultStack>(new FaultStack(vmPid));
}

void DfxThread::CollectFaultMemorys(std::shared_ptr<DfxElfMaps> maps)
{
    faultstack_->CollectStackInfo(dfxFrames_);
    faultstack_->CollectRegistersBlock(regs_, maps);
}

void DfxThread::Detach()
{
    if (threadStatus_ == ThreadStatus::THREAD_STATUS_ATTACHED) {
        ptrace(PTRACE_CONT, nsTid_, 0, 0);
        ptrace(PTRACE_DETACH, nsTid_, NULL, NULL);
        threadStatus_ = ThreadStatus::THREAD_STATUS_DETACHED;
    }
}

pid_t DfxThread::GetRealTid() const
{
    return nsTid_;
}

void DfxThread::SetThreadId(pid_t tid)
{
    tid_ = tid;
}

bool DfxThread::Attach()
{
    if (threadStatus_ == ThreadStatus::THREAD_STATUS_ATTACHED) {
        return true;
    }

    if (ptrace(PTRACE_SEIZE, nsTid_, 0, 0) != 0) {
        DFXLOG_WARN("Failed to seize thread(%d:%d) from (%d:%d), errno=%d",
            tid_, nsTid_, getuid(), getgid(), errno);
        return false;
    }

    if (ptrace(PTRACE_INTERRUPT, nsTid_, 0, 0) != 0) {
        DFXLOG_WARN("Failed to interrupt thread(%d:%d) from (%d:%d), errno=%d",
            tid_, nsTid_, getuid(), getgid(), errno);
        ptrace(PTRACE_DETACH, nsTid_, NULL, NULL);
        return false;
    }

    int64_t startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    do {
        if (waitpid(nsTid_, nullptr, WNOHANG) > 0) {
            break;
        }
        int64_t curTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (curTime - startTime > 1000) { // 1000 : 1s timeout
            ptrace(PTRACE_DETACH, nsTid_, NULL, NULL);
            DFXLOG_WARN("Failed to wait thread(%d:%d) attached.", tid_, nsTid_);
            return false;
        }
        usleep(5); // 5 : sleep 5us
    } while (true);
    threadStatus_ = ThreadStatus::THREAD_STATUS_ATTACHED;
    return true;
}

std::string DfxThread::ToString() const
{
    if (dfxFrames_.size() == 0) {
        return "No frame info";
    }

    std::stringstream threadInfoStream;
    threadInfoStream << "Thread name:" << threadName_ << "" << std::endl;
    for (size_t i = 0; i < dfxFrames_.size(); i++) {
        if (dfxFrames_[i] == nullptr) {
            continue;
        }
        threadInfoStream << DfxFrameFormat::GetFrameStr(dfxFrames_[i]);
    }

    return threadInfoStream.str();
}

void DfxThread::PrintThreadBacktraceByConfig(const int32_t fd)
{
    if (DfxConfig::GetConfig().displayBacktrace) {
        WriteLog(fd, "Tid:%d, Name:%s\n", tid_, threadName_.c_str());
        WriteLog(fd, "%s", DfxFrameFormat::GetFramesStr(dfxFrames_).c_str());
    } else {
        DFXLOG_DEBUG("Hidden backtrace");
    }
}

std::string DfxThread::PrintThreadRegisterByConfig()
{
    if (DfxConfig::GetConfig().displayRegister) {
        if (regs_) {
            return regs_->PrintRegs();
        }
    } else {
        DFXLOG_DEBUG("hidden register");
    }
    return "";
}

void DfxThread::PrintThreadFaultStackByConfig()
{
    if (DfxConfig::GetConfig().displayFaultStack && isCrashThread_) {
        if (faultstack_ != nullptr) {
            faultstack_->Print();
        }
    } else {
        DFXLOG_DEBUG("hidden faultStack");
    }
}

void DfxThread::ClearLastFrame()
{
    dfxFrames_.pop_back();
}

void DfxThread::AddFrame(std::shared_ptr<DfxFrame> frame)
{
    dfxFrames_.push_back(frame);
}
} // namespace HiviewDFX
} // nampespace OHOS
