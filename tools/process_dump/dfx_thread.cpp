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
#include <sstream>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_frame_format.h"
#include "dfx_log.h"
#include "dfx_util.h"
#include "procinfo.h"

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
    ReadThreadName(threadInfo_.tid, threadInfo_.threadName);
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

void DfxThread::AddFrame(std::shared_ptr<DfxFrame> frame)
{
    frames_.push_back(frame);
}

const std::vector<std::shared_ptr<DfxFrame>>& DfxThread::GetFrames() const
{
    return frames_;
}

std::string DfxThread::ToString() const
{
    if (frames_.size() == 0) {
        return "No frame info";
    }

    std::stringstream ss;
    ss << "Thread name:" << threadInfo_.threadName << "" << std::endl;
    for (size_t i = 0; i < frames_.size(); i++) {
        if (frames_[i] == nullptr) {
            continue;
        }
        ss << DfxFrameFormat::GetFrameStr(frames_[i]);
    }
    return ss.str();
}

void DfxThread::Detach()
{
    if (threadStatus == ThreadStatus::THREAD_STATUS_ATTACHED) {
        ptrace(PTRACE_CONT, threadInfo_.nsTid, 0, 0);
        ptrace(PTRACE_DETACH, threadInfo_.nsTid, NULL, NULL);
        threadStatus = ThreadStatus::THREAD_STATUS_INIT;
    }
}

bool DfxThread::Attach()
{
    if (threadStatus == ThreadStatus::THREAD_STATUS_ATTACHED) {
        return true;
    }

    if (ptrace(PTRACE_SEIZE, threadInfo_.nsTid, 0, 0) != 0) {
        DFXLOG_WARN("Failed to seize thread(%d:%d) from (%d:%d), errno=%d",
            threadInfo_.tid, threadInfo_.nsTid, getuid(), getgid(), errno);
        return false;
    }

    if (ptrace(PTRACE_INTERRUPT, threadInfo_.nsTid, 0, 0) != 0) {
        DFXLOG_WARN("Failed to interrupt thread(%d:%d) from (%d:%d), errno=%d",
            threadInfo_.tid, threadInfo_.nsTid, getuid(), getgid(), errno);
        ptrace(PTRACE_DETACH, threadInfo_.nsTid, NULL, NULL);
        return false;
    }

    int64_t startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    do {
        if (waitpid(threadInfo_.nsTid, nullptr, WNOHANG) > 0) {
            break;
        }
        int64_t curTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (curTime - startTime > 1000) { // 1000 : 1s timeout
            ptrace(PTRACE_DETACH, threadInfo_.nsTid, NULL, NULL);
            DFXLOG_WARN("Failed to wait thread(%d:%d) attached.", threadInfo_.tid, threadInfo_.nsTid);
            return false;
        }
        usleep(5); // 5 : sleep 5us
    } while (true);
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
} // namespace HiviewDFX
} // nampespace OHOS
