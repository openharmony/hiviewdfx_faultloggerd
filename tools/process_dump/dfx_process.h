/*
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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

#ifndef DFX_PROCESS_H
#define DFX_PROCESS_H

#include <cinttypes>
#include <map>
#include <memory>
#include <string>

#include "dfx_regs.h"
#include "dfx_thread.h"

namespace OHOS {
namespace HiviewDFX {
struct DfxProcessInfo {
    pid_t pid = 0;
    pid_t nsPid = 0;
    uid_t uid = 0;
    std::string processName = "";
};

class DfxProcess {
public:
    static std::shared_ptr<DfxProcess> Create(pid_t pid, pid_t nsPid);
    DfxProcess(pid_t pid, pid_t nsPid);
    virtual ~DfxProcess() = default;
    void Attach(bool hasKey = false);
    void Detach();

    bool InitOtherThreads(bool attach = false);
    std::vector<std::shared_ptr<DfxThread>>& GetOtherThreads();
    void ClearOtherThreads();
    pid_t ChangeTid(pid_t tid, bool ns);

    void SetFatalMessage(const std::string &msg);
    std::string GetFatalMessage() const;
    static std::string GetProcessLifeCycle(pid_t pid);

    DfxProcessInfo processInfo_;
    pid_t recycleTid_ = 0;
    std::shared_ptr<DfxThread> keyThread_ = nullptr; // comment: crash thread or dump target thread
    std::shared_ptr<DfxThread> vmThread_ = nullptr; // comment: vm thread object in crash scenario
    std::string reason = "";
    std::string openFiles = "";
    std::shared_ptr<DfxRegs> regs_;
private:
    DfxProcess() = default;
    void InitProcessInfo(pid_t pid, pid_t nsPid);

    std::string fatalMsg_ = "";
    std::vector<std::shared_ptr<DfxThread>> otherThreads_;
    std::map<int, int> kvThreads_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
