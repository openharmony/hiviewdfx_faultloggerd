/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "remote_unwinder.h"

#include <string>
#include <unistd.h>

#include "dfx_regs.h"
#include "dfx_trace_dlsym.h"
#include "elapsed_time.h"
#include "unwind_define.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
constexpr int32_t REMOTE_TIME_LIMIT_MS = 20;
}

RemoteUnwinder::RemoteUnwinder(int pid, bool crash, std::shared_ptr<DfxMaps> maps)
    : UnwinderBase(std::make_shared<UnwinderSharedState>())
{
    state_->unwindType = UNWIND_TYPE_REMOTE;
    state_->pid = pid;
    state_->isCrash = crash;
    if (pid <= 0) {
        return;
    }
    DfxEnableTraceDlsym(true);
    state_->maps = (maps == nullptr) ? DfxMaps::Create(pid, crash) : maps;
    InitCommon();
}

RemoteUnwinder::RemoteUnwinder(int pid, int nspid, bool crash, std::shared_ptr<DfxMaps> maps)
    : UnwinderBase(std::make_shared<UnwinderSharedState>())
{
    state_->unwindType = UNWIND_TYPE_REMOTE;
    state_->pid = nspid;
    state_->isCrash = crash;
    if (pid <= 0 || nspid <= 0) {
        return;
    }
    DfxEnableTraceDlsym(true);
    state_->maps = (maps == nullptr) ? DfxMaps::Create(pid, crash, false) : maps;
    InitCommon();
}

bool RemoteUnwinder::DoUnwindRemote(pid_t tid, bool withRegs, size_t maxFrameNum, size_t skipFrameNum)
{
    std::string timeLimitCheck = "UnwindRemote, tid: " + std::to_string(tid);
    ElapsedTime counter(std::move(timeLimitCheck), REMOTE_TIME_LIMIT_MS);
    if (!IsRemoteParamsValid(tid)) {
        return false;
    }
    if (tid == 0) {
        tid = state_->pid;
    }
    if (!PrepareRemoteRegs(tid, withRegs)) {
        return false;
    }
    mixedStack_.SetBytecodePcAndInterruptPc();
    state_->firstFrameSp = state_->regs->GetSp();
    state_->context.pid = tid;
    state_->context.regs = state_->regs;
    state_->context.maps = state_->maps;
    return unwindCore_.Unwind(&state_->context, maxFrameNum, skipFrameNum);
}

bool RemoteUnwinder::IsRemoteParamsValid(pid_t tid)
{
    return (state_->maps != nullptr) && (state_->pid > 0) && (tid >= 0);
}

bool RemoteUnwinder::PrepareRemoteRegs(pid_t tid, bool withRegs)
{
    if (!withRegs) {
        state_->regs = DfxRegs::CreateRemoteRegs(tid);
    }
    return state_->regs != nullptr;
}

} // namespace HiviewDFX
} // namespace OHOS
