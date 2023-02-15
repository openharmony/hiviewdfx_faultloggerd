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

/* This files contains process dump thread module. */

#include "dfx_thread.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <securec.h>
#include <sstream>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_fault_stack.h"
#include "dfx_logger.h"
#include "dfx_regs.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {
DfxThread::DfxThread(pid_t pid, pid_t tid, pid_t nsTid, const ucontext_t &context)
    :isCrashThread_(false), pid_(pid), tid_(tid), nsTid_(nsTid), unwStopReason_(-1)
{
    threadStatus_ = ThreadStatus::THREAD_STATUS_INVALID;
    std::shared_ptr<DfxRegs> reg;
#if defined(__arm__)
    reg = std::make_shared<DfxRegsArm>(context);
#elif defined(__aarch64__)
    reg = std::make_shared<DfxRegsArm64>(context);
#elif defined(__x86_64__)
    reg = std::make_shared<DfxRegsX86_64>(context);
#endif
    regs_ = reg;

    ReadThreadName();
    threadStatus_ = ThreadStatus::THREAD_STATUS_INIT;
}

DfxThread::DfxThread(pid_t pid, pid_t tid, pid_t nsTid)
    :isCrashThread_(false), pid_(pid), tid_(tid), nsTid_(nsTid), unwStopReason_(-1)
{
    regs_ = nullptr;
    ReadThreadName();
    threadStatus_ = ThreadStatus::THREAD_STATUS_INIT;
}

void DfxThread::ReadThreadName()
{
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/comm", tid_) <= 0) {
        return;
    }

    char buf[NAME_LEN];
    ReadStringFromFile(path, buf, NAME_LEN);
    TrimAndDupStr(std::string(buf), threadName_);
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
        DfxLogWarn("No frame print for tid %d.", tid_);
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
        DfxLogWarn("Failed to seize thread(%d:%d), errno=%d", tid_, nsTid_, errno);
        return false;
    }

    if (ptrace(PTRACE_INTERRUPT, nsTid_, 0, 0) != 0) {
        DfxLogWarn("Failed to interrupt thread(%d:%d), errno=%d", tid_, nsTid_, errno);
        ptrace(PTRACE_DETACH, nsTid_, NULL, NULL);
        return false;
    }

    errno = 0;
    while (waitpid(nsTid_, nullptr, __WALL) < 0) {
        if (EINTR != errno) {
            ptrace(PTRACE_DETACH, nsTid_, NULL, NULL);
            DfxLogWarn("Failed to wait thread(%d:%d) attached, errno=%d.", tid_, nsTid_, errno);
            return false;
        }
        errno = 0;
    }
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
        threadInfoStream << dfxFrames_[i]->ToString();
    }

    return threadInfoStream.str();
}

void DfxThread::PrintThreadBacktraceByConfig(const int32_t fd)
{
    if (DfxConfig::GetInstance().GetDisplayBacktrace()) {
        WriteLog(fd, "Tid:%d, Name:%s\n", tid_, threadName_.c_str());
        PrintFrames(dfxFrames_);
    } else {
        DfxLogDebug("hidden backtrace");
    }
}

std::string DfxThread::PrintThreadRegisterByConfig()
{
    if (DfxConfig::GetInstance().GetDisplayRegister()) {
        if (regs_) {
            return regs_->PrintRegs();
        }
    } else {
        DfxLogDebug("hidden register");
    }
    return "";
}

void DfxThread::PrintThreadFaultStackByConfig()
{
    if (DfxConfig::GetInstance().GetDisplayFaultStack() && isCrashThread_) {
        if (faultstack_ != nullptr) {
            faultstack_->Print();
        }
    } else {
        DfxLogDebug("hidden faultStack");
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
