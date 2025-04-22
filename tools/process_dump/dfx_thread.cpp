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

#include "dfx_thread.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstring>
#include <securec.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_frame_formatter.h"
#include "dfx_log.h"
#include "dfx_ptrace.h"
#include "dfx_util.h"
#include "procinfo.h"
#if defined(__aarch64__)
#include "printer.h"
#endif
namespace OHOS {
namespace HiviewDFX {
std::shared_ptr<DfxThread> DfxThread::Create(pid_t pid, pid_t tid, pid_t nsTid)
{
    auto thread = std::make_shared<DfxThread>(pid, tid, nsTid);
    return thread;
}

DfxThread::DfxThread(pid_t pid, pid_t tid, pid_t nsTid) : regs_(nullptr)
{
    InitThreadInfo(pid, tid, nsTid);
}

void DfxThread::InitThreadInfo(pid_t pid, pid_t tid, pid_t nsTid)
{
    threadInfo_.pid = pid;
    threadInfo_.tid = tid;
    threadInfo_.nsTid = nsTid;
    ReadThreadNameByPidAndTid(threadInfo_.pid, threadInfo_.tid, threadInfo_.threadName);
    threadStatus = ThreadStatus::THREAD_STATUS_INIT;
}

DfxThread::~DfxThread()
{
    threadStatus = ThreadStatus::THREAD_STATUS_INVALID;
}

std::shared_ptr<DfxRegs> DfxThread::GetThreadRegs() const
{
    return regs_;
}

void DfxThread::SetThreadRegs(const std::shared_ptr<DfxRegs> &regs)
{
    regs_ = regs;
}

void DfxThread::AddFrame(const DfxFrame& frame)
{
    frames_.emplace_back(frame);
}

void DfxThread::AddFrames(const std::vector<DfxFrame>& frames)
{
    frames_.insert(frames_.end(), frames.begin(), frames.end());
}

const std::vector<DfxFrame>& DfxThread::GetFrames() const
{
    return frames_;
}

std::string DfxThread::ToString() const
{
    if (frames_.size() == 0) {
        return "No frame info";
    }

    std::string ss = "Thread name:" + threadInfo_.threadName + "\n";
    bool needSkip = false;
    bool isSubmitter = true;
    for (const auto& frame : frames_) {
        if (frame.index == 0) {
            isSubmitter = !isSubmitter;
        }
        if (isSubmitter) {
            ss += "========SubmitterStacktrace========\n";
            isSubmitter = false;
            needSkip = false;
        }
        if (needSkip) {
            continue;
        }
        ss += DfxFrameFormatter::GetFrameStr(frame);
#if defined(__aarch64__)
        if (Printer::IsLastValidFrame(frame)) {
            needSkip = true;
        }
#endif
    }
    return ss;
}

void DfxThread::Detach()
{
    if (threadStatus == ThreadStatus::THREAD_STATUS_ATTACHED) {
        DfxPtrace::Detach(threadInfo_.nsTid);
        threadStatus = ThreadStatus::THREAD_STATUS_INIT;
    }
}

bool DfxThread::Attach(int timeout)
{
    if (threadStatus == ThreadStatus::THREAD_STATUS_ATTACHED) {
        return true;
    }

    if (!DfxPtrace::Attach(threadInfo_.nsTid, timeout)) {
        return false;
    }

    threadStatus = ThreadStatus::THREAD_STATUS_ATTACHED;
    return true;
}

void DfxThread::InitFaultStack(bool needParseStack)
{
    if (faultStack_ != nullptr) {
        return;
    }
    faultStack_ = std::make_shared<FaultStack>(threadInfo_.nsTid);
    faultStack_->CollectStackInfo(frames_, needParseStack);
}


void DfxThread::SetFrames(const std::vector<DfxFrame>& frames)
{
    frames_ = frames;
}

std::shared_ptr<FaultStack> DfxThread::GetFaultStack() const
{
    return faultStack_;
}

void DfxThread::ParseSymbol(Unwinder& unwinder)
{
    if (!needParseSymbol_) {
        return;
    }

    for (auto& frame : frames_) {
        if (frame.isJsFrame) { // js frame parse in unwinder
            continue;
        }
        unwinder.ParseFrameSymbol(frame);
    }
}

void DfxThread::SetParseSymbolNecessity(bool needParseSymbol)
{
    needParseSymbol_ = needParseSymbol;
}
} // namespace HiviewDFX
} // nampespace OHOS
