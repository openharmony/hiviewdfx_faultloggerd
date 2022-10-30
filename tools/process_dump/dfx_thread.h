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

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>
#include "dfx_define.h"
#include "dfx_fault_stack.h"
#include "dfx_frame.h"
#include "dfx_maps.h"
#include "dfx_regs.h"

namespace OHOS {
namespace HiviewDFX {
class DfxThread {
public:
    DfxThread(pid_t pid, pid_t tid, pid_t nsTid, const ucontext_t &context);
    DfxThread(pid_t pid, pid_t tid, pid_t nsTid);
    ~DfxThread();
    void SetIsCrashThread(bool isCrashThread);
    bool GetIsCrashThread() const;
    pid_t GetProcessId() const;
    pid_t GetThreadId() const;
    pid_t GetRealTid() const;
    std::string GetThreadName() const;
    void SetThreadName(std::string &threadName);
    std::shared_ptr<DfxRegs> GetThreadRegs() const;
    std::vector<std::shared_ptr<DfxFrame>> GetThreadDfxFrames() const;
    void SetThreadRegs(const std::shared_ptr<DfxRegs> &regs);
    std::shared_ptr<DfxFrame> GetAvaliableFrame();
    void PrintThread(const int32_t fd, bool isSignalDump);
    void PrintThreadBacktraceByConfig(const int32_t fd);
    std::string PrintThreadRegisterByConfig();
    void PrintThreadFaultStackByConfig();
    void SetThreadUnwStopReason(int reason);
    void CreateFaultStack();
    void Detach();
    bool Attach();
    std::string ToString() const;
    bool IsThreadInitialized();
    void ClearLastFrame();
    void AddFrame(std::shared_ptr<DfxFrame> frame);

private:
    enum class ThreadStatus {
        THREAD_STATUS_INVALID =  0,
        THREAD_STATUS_INIT = 1,
        THREAD_STATUS_DETACHED = 2,
        THREAD_STATUS_ATTACHED = 3
    };

    bool InitThread();
    bool isCrashThread_;
    pid_t pid_;
    pid_t tid_;
    pid_t nsTid_;
    int unwStopReason_;
    ThreadStatus threadStatus_;
    std::string threadName_;
    std::shared_ptr<DfxRegs> regs_;
    std::vector<std::shared_ptr<DfxFrame>> dfxFrames_;
    std::unique_ptr<FaultStack> faultstack_ {nullptr};
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
