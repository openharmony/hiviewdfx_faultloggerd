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
#ifndef DFX_PROCESS_H
#define DFX_PROCESS_H

#include <cinttypes>
#include <memory>
#include <string>

#include "dfx_define.h"
#include "dfx_elf.h"
#include "dfx_maps.h"
#include "dfx_thread.h"

namespace OHOS {
namespace HiviewDFX {
class DfxProcess {
public:
    DfxProcess() = default;
    virtual ~DfxProcess() = default;
    static std::shared_ptr<DfxProcess> CreateProcessWithKeyThread(pid_t pid, std::shared_ptr<DfxThread> keyThread);
    bool InitProcessMaps();
    bool InitProcessThreads(std::shared_ptr<DfxThread> keyThread);
    bool InitOtherThreads();
    void FillProcessName();
    void PrintProcessMapsByConfig(int32_t fd);
    void PrintThreadsHeaderByConfig(int32_t fd);
    void InsertThreadNode(pid_t tid);
    virtual void PrintProcess(int32_t fd, bool printMapFlag);
    virtual void PrintProcessWithSiginfo(const std::shared_ptr<siginfo_t> info, int32_t fd);

    void SetIsSignalHdlr(bool isSignalHdlr);
    bool GetIsSignalHdlr() const;
    void SetIsSignalDump(bool isSignalDump);
    bool GetIsSignalDump() const;
    pid_t GetPid() const;
    pid_t GetUid() const;
    std::string GetProcessName() const;
    std::shared_ptr<DfxElfMaps> GetMaps() const;
    std::vector<std::shared_ptr<DfxThread>> GetThreads() const;

    void SetPid(pid_t pid);
    void SetUid(pid_t uid);
    void SetProcessName(const std::string &processName);
    void SetMaps(std::shared_ptr<DfxElfMaps> maps);
    void SetThreads(const std::vector<std::shared_ptr<DfxThread>> &threads);
    void Detach();
private:
    pid_t pid_ = 0;
    pid_t uid_ = 0;
    bool isSignalHdlr_ = false;
    bool isSignalDump_ = false;
    std::string processName_;
    std::shared_ptr<DfxElfMaps> maps_;
    std::vector<std::shared_ptr<DfxThread>> threads_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
