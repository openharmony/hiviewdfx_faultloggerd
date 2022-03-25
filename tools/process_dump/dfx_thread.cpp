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
#include "dfx_config.h"

namespace OHOS {
namespace HiviewDFX {
DfxThread::DfxThread(const pid_t pid, const pid_t tid, const ucontext_t &context)
{
    DfxLogDebug("Enter %s.", __func__);
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
    threadName_ = "";
    unwStopReason_ = -1;
    if (!InitThread(pid, tid)) {
        DfxLogWarn("Fail to init thread(%d).", tid);
        return;
    }

    DfxLogDebug("Exit %s.", __func__);
}

DfxThread::DfxThread(const pid_t pid, const pid_t tid)
{
    DfxLogDebug("Enter %s.", __func__);
    threadStatus_ = ThreadStatus::THREAD_STATUS_INIT;
    regs_ = nullptr;
    threadName_ = "";
    unwStopReason_ = -1;
    if (!InitThread(pid, tid)) {
        DfxLogWarn("Fail to init thread(%d).", tid);
        return;
    }
    DfxLogDebug("Exit %s.", __func__);
}

bool DfxThread::InitThread(const pid_t pid, const pid_t tid)
{
    DfxLogDebug("Enter %s.", __func__);
    pid_ = pid;
    tid_ = tid;
    isCrashThread_ = false;
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
        DfxLogWarn("Fail to attach thread(%d), errno=%d", tid, errno);
        return false;
    }

    errno = 0;
    while (waitpid(tid, nullptr, __WALL) < 0) {
        if (EINTR != errno) {
            ptrace(PTRACE_DETACH, tid, NULL, NULL);
            DfxLogWarn("Fail to wait thread(%d) attached, errno=%d.", tid, errno);
            return false;
        }
        errno = 0;
    }
#endif
    threadStatus_ = ThreadStatus::THREAD_STATUS_ATTACHED;
    DfxLogDebug("Exit %s.", __func__);
    return true;
}

bool DfxThread::IsThreadInititalized()
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

void DfxThread::SetThreadName(std::string &threadName)
{
    threadName_ = threadName;
}

std::shared_ptr<DfxRegs> DfxThread::GetThreadRegs() const
{
    return regs_;
}

std::vector<std::shared_ptr<DfxFrames>> DfxThread::GetThreadDfxFrames() const
{
    return dfxFrames_;
}

void DfxThread::SetThreadRegs(const std::shared_ptr<DfxRegs> &regs)
{
    regs_ = regs;
}

std::shared_ptr<DfxFrames> DfxThread::GetAvaliableFrame()
{
    DfxLogDebug("Enter %s.", __func__);
    std::shared_ptr<DfxFrames> frame = std::make_shared<DfxFrames>();
    dfxFrames_.push_back(frame);
    return frame;
}

void DfxThread::PrintThread(const int32_t fd, bool isSignalDump)
{
    DfxLogDebug("Enter %s.", __func__);
    if (dfxFrames_.size() == 0) {
        DfxLogWarn("No frame print for tid %d.", tid_);
        return;
    }

    PrintThreadBacktraceByConfig(fd);
    if (isSignalDump == false) {
        PrintThreadRegisterByConfig(fd);
        PrintThreadFaultStackByConfig(fd);
    }
    DfxLogDebug("Exit %s.", __func__);
}

uint64_t DfxThread::DfxThreadDoAdjustPc(uint64_t pc)
{
    DfxLogDebug("Enter %s :: pc(0x%x).", __func__, pc);

    uint64_t ret = 0;

    if (pc == 0) {
        ret = pc; // pc zero is abnormal case, so we don't adjust pc.
    } else {
#if defined(__arm__)
        if (pc & 1) { // thumb mode, pc step is 2 byte.
            ret = pc - ARM_EXEC_STEP_THUMB;
        } else {
            ret = pc - ARM_EXEC_STEP_NORMAL;
        }
#elif defined(__aarch64__)
        ret = pc - ARM_EXEC_STEP_NORMAL;
#endif
    }

    DfxLogDebug("Exit %s :: ret(0x%x).", __func__, ret);
    return ret;
}

void DfxThread::SkipFramesInSignalHandler()
{
    DfxLogDebug("Enter %s.", __func__);
    if (dfxFrames_.size() == 0) {
        return;
    }

    if (regs_ == nullptr) {
        return;
    }

    std::vector<std::shared_ptr<DfxFrames>> skippedFrames;
    int framesSize = (int)dfxFrames_.size();
    bool skipPos = false;
    size_t index = 0;
    std::vector<uintptr_t> regs = regs_->GetRegsData();
    uintptr_t adjustedLr = DfxThreadDoAdjustPc(regs[REG_LR_NUM]);
    for (int i = 0; i < framesSize; i++) {
        if (dfxFrames_[i] == nullptr) {
            continue;
        }

        if (regs[REG_PC_NUM] == dfxFrames_[i]->GetFramePc()) {
            DfxLogDebug("%s :: frame i(%d), adjustedLr=0x%x, dfxFrames_[i]->GetFramePc()=0x%x, regs[REG_PC_NUM](0x%x)",
                __func__, i, adjustedLr, dfxFrames_[i]->GetFramePc(), regs[REG_PC_NUM]);
            skipPos = true;
        }
        /* when pc is zero the REG_LR_NUM for filtering */
        if ((regs[REG_PC_NUM] == 0) && (adjustedLr == dfxFrames_[i]->GetFramePc())) {
            DfxLogDebug("%s :: add pc=0 frame :: index(%d), i(%d), adjustedLr=0x%x, dfxFrames_[i]->GetFramePc()=0x%x",
                __func__, index, i, adjustedLr, dfxFrames_[i]->GetFramePc());
            skipPos = true;
            std::shared_ptr<DfxFrames> frame = std::make_shared<DfxFrames>();
            frame->SetFrameIndex(index);
            skippedFrames.push_back(frame);
            index++;
        }

        /* when pc is zero the REG_LR_NUM for filtering */
        if (skipPos) {
            dfxFrames_[i]->SetFrameIndex(index);
            skippedFrames.push_back(dfxFrames_[i]);
            index++;
        }
    }

    if (skipPos) {
        dfxFrames_.clear();
        dfxFrames_ = skippedFrames;
    } else {
        DfxLogWarn("signal frame is not skipped.");
    }

    DfxLogDebug("Exit %s :: index(%d).", __func__, index);
}

void DfxThread::SetThreadUnwStopReason(int reason)
{
    DfxLogDebug("Enter %s.", __func__);
    unwStopReason_ = reason;
    DfxLogDebug("Exit %s :: unwStopReason_(%d).", __func__, unwStopReason_);
}

void DfxThread::CreateFaultStack(std::shared_ptr<DfxElfMaps> maps)
{
    DfxLogDebug("Enter %s.", __func__);
    char codeBuffer[FAULTSTACK_ITEM_BUFFER_LENGTH] = {};
    int lowAddressStep = (int)DfxConfig::GetInstance().GetFaultStackLowAddressStep();
    int highAddressStep = (int)DfxConfig::GetInstance().GetFaultStackHighAddressStep();
    for (size_t i = 0; i < dfxFrames_.size(); i++) {
        std::string strFaultStack;
        bool displayAll = true;
        bool displayAdjust = false;
#if defined(__arm__)
#define printLength "08"
        int startSp, currentSp, nextSp, storeData, totalStepSize, filterStart, filterEnd, regLr;
        int stepLength = 4;
        currentSp = (int)dfxFrames_[i]->GetFrameSp();
        regLr = (i+1 == dfxFrames_.size()) ? 0 : (int)dfxFrames_[i+1]->GetFrameLr();
#elif defined(__aarch64__)
#define printLength "16"
        uint64_t startSp, currentSp, nextSp, storeData, totalStepSize, filterStart, filterEnd, regLr;
        int stepLength = 8;
        currentSp = dfxFrames_[i]->GetFrameSp();
        regLr = (i+1 == dfxFrames_.size()) ? 0 : dfxFrames_[i+1]->GetFrameLr();
#endif
        std::shared_ptr<DfxElfMap> map = nullptr;
        bool mapCheck = maps->FindMapByAddr((uintptr_t)regLr, map);
        startSp = currentSp = (unsigned int)currentSp & (~FAULTSTACK_SP_REVERSE);

        if (i == (dfxFrames_.size() - 1)) {
            nextSp = currentSp + FAULTSTACK_FIRST_FRAME_SEARCH_LENGTH;
        } else {
#if defined(__arm__)
            nextSp = (int)dfxFrames_[i+1]->GetFrameSp();
#elif defined(__aarch64__)
            nextSp = dfxFrames_[i+1]->GetFrameSp();
#endif
        }
        totalStepSize = (nextSp - currentSp)/stepLength;
        if (totalStepSize > (lowAddressStep + highAddressStep)) {
            displayAll = false;
            filterStart = currentSp + highAddressStep*stepLength;
            filterEnd   = currentSp + (totalStepSize - lowAddressStep)*stepLength;
        }
        while (currentSp < nextSp) {
            if (!displayAll && (currentSp == filterStart)) {
                std::string itemFaultStack("    ...\n");

                strFaultStack += itemFaultStack;
                currentSp = filterEnd;
                continue;
            }
            errno_t err = memset_s(codeBuffer, sizeof(codeBuffer), '\0', sizeof(codeBuffer));
            if (err != EOK) {
                DfxLogError("%s :: memset_s failed, err = %d\n", __func__, err);
            }
            if (currentSp == startSp) {
                auto pms = sprintf_s(codeBuffer, sizeof(codeBuffer), "%" printLength "x", currentSp);
                if (pms <= 0) {
                    DfxLogError("%s :: sprintf_s failed.", __func__);
                }
            } else {
                auto pms = sprintf_s(codeBuffer, sizeof(codeBuffer), "    %" printLength "x", currentSp);
                if (pms <= 0) {
                    DfxLogError("%s :: sprintf_s failed.", __func__);
                }
            }
            storeData = ptrace(PTRACE_PEEKTEXT, tid_, (void*)currentSp, NULL);
            (void)sprintf_s(codeBuffer + strlen(codeBuffer),
                            sizeof(codeBuffer) - strlen(codeBuffer),
                            " %" printLength "x",
                            storeData);
            if ((storeData == regLr) && (mapCheck)) {
                auto pms = sprintf_s(codeBuffer + strlen(codeBuffer), \
                    sizeof(codeBuffer) - strlen(codeBuffer), " %s", map->GetMapPath().c_str());
                if (pms <= 0) {
                    DfxLogError("%s :: sprintf_s failed.", __func__);
                }
            }
            std::string itemFaultStack(codeBuffer, codeBuffer + strlen(codeBuffer));
            itemFaultStack.append("\n");
            currentSp += stepLength;
            strFaultStack += itemFaultStack;
        }
        dfxFrames_[i]->SetFrameFaultStack(strFaultStack);
    }
    DfxLogDebug("Exit %s.", __func__);
}

void DfxThread::Detach()
{
    DfxLogDebug("Enter %s.", __func__);
    ptrace(PTRACE_DETACH, tid_, NULL, NULL);
    if (threadStatus_ == ThreadStatus::THREAD_STATUS_ATTACHED) {
        threadStatus_ = ThreadStatus::THREAD_STATUS_DETACHED;
    } else {
        DfxLogError("%s(%d), current status: %d, can't detached.", __FILE__, __LINE__, threadStatus_);
    }
    DfxLogDebug("Exit %s.", __func__);
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

void DfxThread::PrintThreadBacktraceByConfig(const int32_t fd)
{
    if (DfxConfig::GetInstance().GetDisplayBacktrace()) {
        WriteLog(fd, "Tid:%d, Name:%s\n", tid_, threadName_.c_str());
        PrintFrames(dfxFrames_, fd);
    } else {
        DfxLogDebug("hidden backtrace");
    }
}

void DfxThread::PrintThreadRegisterByConfig(const int32_t fd)
{
    if (DfxConfig::GetInstance().GetDisplayRegister()) {
        if (regs_) {
            regs_->PrintRegs(fd);
        }
    } else {
        DfxLogDebug("hidden register");
    }
}

void DfxThread::PrintThreadFaultStackByConfig(const int32_t fd)
{
    if (DfxConfig::GetInstance().GetDisplayFaultStack() && isCrashThread_) {
        WriteLog(fd, "FaultStack:\n");
        PrintFaultStacks(dfxFrames_, fd);
    } else {
        DfxLogDebug("hidden faultStack");
    }
}

} // namespace HiviewDFX
} // nampespace OHOS
