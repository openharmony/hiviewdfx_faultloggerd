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

#ifndef DFX_THREAD_H
#define DFX_THREAD_H

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "proc_util.h"
#include "unwinder.h"

namespace OHOS {
namespace HiviewDFX {

struct DfxThreadInfo {
    pid_t pid = 0;
    pid_t tid = 0;
    pid_t nsTid = 0;
    std::string threadName = "";
};

class DfxThread {
public:
    static std::shared_ptr<DfxThread> Create(pid_t pid, pid_t tid, pid_t nsTid, bool readStat = false);
    DfxThread(pid_t pid, pid_t tid, pid_t nsTid, bool readStat = false);
    virtual ~DfxThread();

    std::shared_ptr<DfxRegs> GetThreadRegs() const;
    void SetThreadRegs(const std::shared_ptr<DfxRegs> &regs);
    void AddFrame(const DfxFrame& frame);
    const std::vector<DfxFrame>& GetFrames() const;
    void SetFrames(const std::vector<DfxFrame>& frames);
    void SetSubmitterFrames(const std::vector<DfxFrame>& frames)
    {
        submitterFrames_ = frames;
    }
    void FillSymbol(const std::map<uint64_t, DfxFrame>& frameTable);
    std::string ToString(bool needPrintTid = true) const;
    const DfxThreadInfo& GetThreadInfo() const
    {
        return threadInfo_;
    }
    void SetThreadName(const std::string& threadName)
    {
        threadInfo_.threadName = threadName;
    }

    void Detach();
    bool Attach(int timeout = PTRACE_ATTATCH_KEY_THREAD_TIMEOUT);
    void SetParseSymbolNecessity(bool needParseSymbol);
    const std::shared_ptr<ProcessInfo> GetProcessInfo() const
    {
        return processInfo_;
    }

private:
    enum class ThreadStatus {
        THREAD_STATUS_INVALID = -1,
        THREAD_STATUS_INIT = 0,
        THREAD_STATUS_ATTACHED = 1
    };

    DfxThread() = default;
    void InitThreadInfo(pid_t pid, pid_t tid, pid_t nsTid);
    DfxThreadInfo threadInfo_;
    ThreadStatus threadStatus = ThreadStatus::THREAD_STATUS_INVALID;
    std::shared_ptr<DfxRegs> regs_ = nullptr;
    std::vector<DfxFrame> frames_;
    std::vector<DfxFrame> submitterFrames_;
    bool needParseSymbol_ = true;
    std::shared_ptr<ProcessInfo> processInfo_ = nullptr;
};
} // namespace HiviewDFX
} // namespace OHOS

#endif
