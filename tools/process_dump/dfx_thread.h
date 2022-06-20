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
#ifndef DFX_THREAD_H
#define DFX_THREAD_H

#include <sys/types.h>
#include <cstdint>
#include <string>
#include <vector>

#include "dfx_define.h"
#include "dfx_frames.h"
#include "dfx_regs.h"
#include "dfx_maps.h"

namespace OHOS {
namespace HiviewDFX {
class DfxThread {
public:
    DfxThread(const pid_t pid, const pid_t tid, const ucontext_t &context);
    DfxThread(const pid_t pid, const pid_t tid);
    ~DfxThread();
    void SetIsCrashThread(bool isCrashThread);
    bool GetIsCrashThread() const;
    pid_t GetProcessId() const;
    pid_t GetThreadId() const;
    std::string GetThreadName() const;
    void SetThreadName(std::string &threadName);
    std::shared_ptr<DfxRegs> GetThreadRegs() const;
    std::vector<std::shared_ptr<DfxFrames>> GetThreadDfxFrames() const;
    void SetThreadRegs(const std::shared_ptr<DfxRegs> &regs);
    std::shared_ptr<DfxFrames> GetAvaliableFrame();
    void PrintThread(const int32_t fd, bool isSignalDump);
    void PrintThreadBacktraceByConfig(const int32_t fd);
    std::string PrintThreadRegisterByConfig();
    std::string PrintThreadFaultStackByConfig();
    void SkipFramesInSignalHandler();
    void SetThreadUnwStopReason(int reason);
    void CreateFaultStack(std::shared_ptr<DfxElfMaps> maps);
    void Detach();
    bool Attach();
    std::string ToString() const;
    bool IsThreadInitialized();

private:
    enum class ThreadStatus {
        THREAD_STATUS_INVALID =  0,
        THREAD_STATUS_INIT = 1,
        THREAD_STATUS_DETACHED = 2,
        THREAD_STATUS_ATTACHED = 3
    };

    bool InitThread(const pid_t pid, const pid_t tid);
    uint64_t DfxThreadDoAdjustPc(uint64_t pc);
    pid_t pid_;
    pid_t tid_;
    std::string threadName_;
    std::shared_ptr<DfxRegs> regs_;
    std::vector<std::shared_ptr<DfxFrames>> dfxFrames_;
    ThreadStatus threadStatus_;
    int unwStopReason_;
    bool isCrashThread_;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
