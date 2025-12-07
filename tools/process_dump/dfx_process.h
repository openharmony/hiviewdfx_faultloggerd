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

#ifndef DFX_PROCESS_H
#define DFX_PROCESS_H

#include <cinttypes>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "dfx_dump_request.h"
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
struct CrashLogConfig {
    bool extendPcLrPrinting = false;
    bool simplifyVmaPrinting = false;
    uint32_t logFileCutoffSizeBytes = 0;
};
class DfxProcess final {
public:
    void InitProcessInfo(pid_t pid, pid_t nsPid, uid_t uid, const std::string& processName);
    void Attach(bool hasKey = false);
    void Detach();
    bool InitKeyThread(const ProcessDumpRequest& request, bool isAttatch = true);
    bool InitOtherThreads(const ProcessDumpRequest& request);
    std::vector<std::shared_ptr<DfxThread>>& GetOtherThreads();
    std::shared_ptr<DfxThread>& GetKeyThread()
    {
        return keyThread_;
    }

    const DfxProcessInfo& GetProcessInfo() const
    {
        return processInfo_;
    }

    void SetFaultThreadRegisters(std::shared_ptr<DfxRegs> regs)
    {
        regs_ = regs;
    }

    const std::shared_ptr<DfxRegs>& GetFaultThreadRegisters() const
    {
        return regs_;
    }

    void SetReason(const std::string& reason)
    {
        reason_ = reason;
    }

    const std::string& GetReason() const
    {
        return reason_;
    }

    const std::string& GetCrashInfoJson() const
    {
        return crashInfoJson_;
    }

    void SetCrashInfoJson(const std::string& crashInfoJson)
    {
        crashInfoJson_ = crashInfoJson;
    }

    void SetVmPid(pid_t pid)
    {
        vmPid_ = pid;
    }

    pid_t GetVmPid() const
    {
        return vmPid_;
    }

    void SetLifeTime(const uint64_t lifeTime)
    {
        lifeTime_ = lifeTime;
    }

    uint64_t GetLifeTime() const
    {
        return lifeTime_;
    }

    void SetRss(const uint64_t rss)
    {
        rss_ = rss;
    }

    uint64_t GetRss() const
    {
        return rss_;
    }

    const CrashLogConfig& GetCrashLogConfig()
    {
        return crashLogConfig_;
    }

    void SetCrashLogConfig(const CrashLogConfig& crashLogConfig)
    {
        crashLogConfig_ = crashLogConfig;
    }

    const std::set<uintptr_t>& GetMemoryValues()
    {
        return memoryValues_;
    }

    void SetMemoryValues(const std::set<uintptr_t>& memoryValues)
    {
        memoryValues_ = memoryValues;
    }

    void AddNativeFramesTable(std::pair<uint64_t, DfxFrame> frame)
    {
        nativeFramesTable_.emplace(std::move(frame));
    }

    std::map<uint64_t, DfxFrame>& GetNativeFramesTable()
    {
        return nativeFramesTable_;
    }

    void ClearOtherThreads();
    pid_t ChangeTid(pid_t tid, bool ns);

    void AppendFatalMessage(const std::string &msg);
    const std::string& GetFatalMessage() const;
private:
    DfxProcessInfo processInfo_;
    CrashLogConfig crashLogConfig_;
    std::shared_ptr<DfxRegs> regs_;
    std::shared_ptr<DfxThread> keyThread_ = nullptr; // comment: crash thread or dump target thread
    std::vector<std::shared_ptr<DfxThread>> otherThreads_;
    std::string reason_ = "";
    std::string fatalMsg_ = "";
    uint64_t rss_{0};
    uint64_t lifeTime_{0};
    std::map<int, int> kvThreads_;
    std::string crashInfoJson_ = "";
    pid_t vmPid_ = 0;
    std::set<uintptr_t> memoryValues_;
    std::map<uint64_t, DfxFrame> nativeFramesTable_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
