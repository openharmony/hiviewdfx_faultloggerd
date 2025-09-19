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
#include "dump_utils.h"
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

const std::vector<DfxFrame>& DfxThread::GetFrames() const
{
    return frames_;
}

std::string DfxThread::ToString(bool needPrintTid) const
{
    std::string dumpThreadInfo;
    if (needPrintTid) {
        dumpThreadInfo += "Tid:" + std::to_string(threadInfo_.tid) +
        ", Name:" + threadInfo_.threadName + "\n";
    } else {
        if (frames_.size() == 0) {
            return "No frame info";
        }
        dumpThreadInfo += "Thread name:" + threadInfo_.threadName + "\n";
    }

    dumpThreadInfo += DumpUtils::GetStackTrace(frames_);

    if (!submitterFrames_.empty()) {
        dumpThreadInfo += "========SubmitterStacktrace========\n";
        dumpThreadInfo += DumpUtils::GetStackTrace(submitterFrames_);
    }
    return dumpThreadInfo;
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

void DfxThread::SetFrames(const std::vector<DfxFrame>& frames)
{
    frames_ = frames;
}

void DfxThread::FillSymbol(const std::map<uint64_t, DfxFrame>& frameTable)
{
    if (!needParseSymbol_) {
        return;
    }
    for (auto& frame : frames_) {
        if (frame.isJsFrame) { // js frame parse in unwinder
            continue;
        }
        auto foundFrame = frameTable.find(frame.pc);
        if (foundFrame != frameTable.end() && foundFrame->second.parseSymbolState.IsParseSymbolComplete()) {
            frame.funcName = foundFrame->second.funcName;
            frame.funcOffset = foundFrame->second.funcOffset;
        }
    }
}

void DfxThread::SetParseSymbolNecessity(bool needParseSymbol)
{
    needParseSymbol_ = needParseSymbol;
}
} // namespace HiviewDFX
} // nampespace OHOS
