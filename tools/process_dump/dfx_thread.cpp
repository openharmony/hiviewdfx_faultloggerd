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

/* This files contains process dump thread module. */

#include "dfx_thread.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <sstream>

#include <sys/ptrace.h>
#include <sys/wait.h>

#include <securec.h>

#include "dfx_define.h"
#include "dfx_frames.h"
#include "dfx_log.h"
#include "dfx_regs.h"
#include "dfx_util.h"

#if defined(__arm__)
static const int USER_REG_NUM = 16;
static const int REG_PC_NUM = 15;
#elif defined(__aarch64__)
static const int USER_REG_NUM = 34;
static const int REG_PC_NUM = 32;
#elif defined(__x86_64__)
static const int USER_REG_NUM = 27;
static const int REG_PC_NUM = 16;
#endif

namespace OHOS {
namespace HiviewDFX {
DfxThread::DfxThread(const pid_t pid, const pid_t tid, const ucontext_t &context)
{
    DfxLogInfo("Enter %s.", __func__);
    threadStatus_ = ThreadStatus::THREAD_STATUS_INVALID;
    if (!InitThread(pid, tid)) {
        DfxLogWarn("Fail to init thread(%d).", tid);
        return;
    }
    std::shared_ptr<DfxRegs> reg;
#if defined(__arm__)
    reg = std::make_shared<DfxRegsArm>(context);
#elif defined(__aarch64__)
    reg = std::make_shared<DfxRegsArm64>(context);
#elif defined(__x86_64__)
    reg = std::make_shared<DfxRegsX86_64>(context);
#endif
    regs_ = reg;
    DfxLogInfo("Exit %s.", __func__);
}

DfxThread::DfxThread(const pid_t pid, const pid_t tid)
{
    DfxLogInfo("Enter %s.", __func__);
    threadStatus_ = ThreadStatus::THREAD_STATUS_INIT;
    if (!InitThread(pid, tid)) {
        DfxLogWarn("Fail to init thread(%d).", tid);
        return;
    }
    DfxLogInfo("Exit %s.", __func__);
}

bool DfxThread::InitThread(const pid_t pid, const pid_t tid)
{
    DfxLogInfo("Enter %s.", __func__);
    pid_ = pid;
    tid_ = tid;
    char path[NAME_LEN] = {0};
    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/comm", tid) <= 0) {
        return false;
    }
    std::string pathName = path;
    std::string buf;
    ReadStringFromFile(pathName, buf, NAME_LEN);
    TrimAndDupStr(buf, threadName_);
#ifndef DFX_LOCAL_UNWIND
    if (ptrace(PTRACE_ATTACH, tid, NULL, NULL) != 0) {
        DfxLogWarn("Fail to attach thread(%d), errno=%s", tid, strerror(errno));
        return false;
    }

    errno = 0;
    while (waitpid(tid, nullptr, __WALL) < 0) {
        if (EINTR != errno) {
            ptrace(PTRACE_DETACH, tid, NULL, NULL);
            DfxLogWarn("Fail to wait thread(%d) attached, errno=%s", tid, strerror(errno));
            return false;
        }
        errno = 0;
    }
#endif
    threadStatus_ = ThreadStatus::THREAD_STATUS_ATTACHED;
    DfxLogInfo("Exit %s.", __func__);
    return true;
}

DfxThread::~DfxThread()
{
    threadStatus_ = ThreadStatus::THREAD_STATUS_INVALID;
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

std::shared_ptr<DfxRegs> DfxThread::GetThreadRegs() const
{
    return regs_;
}

void DfxThread::SetThreadRegs(const std::shared_ptr<DfxRegs> &regs)
{
    regs_ = regs;
}

std::shared_ptr<DfxFrames> DfxThread::GetAvaliableFrame()
{
    DfxLogInfo("Enter %s.", __func__);
    std::shared_ptr<DfxFrames> frame = std::make_shared<DfxFrames>();
    dfxFrames_.push_back(frame);
    return frame;
}

void DfxThread::PrintThread(const int32_t fd)
{
    DfxLogInfo("Enter %s.", __func__);
    if (dfxFrames_.size() == 0) {
        return;
    }

    WriteLog(fd, "Tid:%d, Name:%s\n", tid_, threadName_.c_str());
    if (regs_) {
        regs_->PrintRegs(fd);
    }
    PrintFrames(dfxFrames_, fd);
    WriteLog(fd, "\n");
    DfxLogInfo("Exit %s.", __func__);
}

void DfxThread::SkipFramesInSignalHandler()
{
    DfxLogInfo("Enter %s.", __func__);
    if (dfxFrames_.size() == 0) {
        return;
    }

    if (regs_ == NULL) {
        return;
    }

    std::vector<std::shared_ptr<DfxFrames>> skippedFrames;
    int framesSize = dfxFrames_.size();
    bool skipPos = false;
    size_t index = 0;
    std::vector<uintptr_t> regs = regs_->GetRegsData();
    for (int i = 0; i < framesSize; i++) {
        if (dfxFrames_[i] != NULL && regs[REG_PC_NUM] == dfxFrames_[i]->GetFramePc()) {
            skipPos = true;
        }
        if (skipPos) {
            dfxFrames_[i]->SetFrameIndex(index);
            skippedFrames.push_back(dfxFrames_[i]);
            index++;
        }
    }

    dfxFrames_.clear();
    dfxFrames_ = skippedFrames;
    DfxLogInfo("Exit %s.", __func__);
}

void DfxThread::Detach()
{
    DfxLogInfo("Enter %s.", __func__);
    ptrace(PTRACE_DETACH, tid_, NULL, NULL);
    if (threadStatus_ == ThreadStatus::THREAD_STATUS_ATTACHED) {
        threadStatus_ = ThreadStatus::THREAD_STATUS_DETACHED;
    } else {
        DfxLogError("%s(%d), current status: %d, can't detached.", __FILE__, __LINE__, threadStatus_);
    }
    DfxLogInfo("Exit %s.", __func__);
}

std::string DfxThread::ToString() const
{
    if (dfxFrames_.size() == 0) {
        return "No frame info";
    }

    std::stringstream threadInfoStream;
    threadInfoStream << "Tid:" << tid_ << " ";
    threadInfoStream << "Name:" << threadName_ << "" << std::endl;
    for (size_t i = 0; i < dfxFrames_.size(); i++) {
        if (dfxFrames_[i] == nullptr) {
            continue;
        }
        threadInfoStream << dfxFrames_[i]->ToString();
    }

    return threadInfoStream.str();
}
} // namespace HiviewDFX
} // nampespace OHOS
