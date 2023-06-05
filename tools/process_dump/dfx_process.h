/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "dfx_maps.h"
#include "dfx_thread.h"

namespace OHOS {
namespace HiviewDFX {
struct DfxProcessInfo {
    pid_t pid = 0;
    pid_t nsPid = 0;
    uid_t uid = 0;
    pid_t recycleTid = 0;
    std::string processName = "";
};

class DfxProcess {
public:
    static std::shared_ptr<DfxProcess> CreateProcessWithKeyThread(pid_t pid, pid_t nsPid, std::shared_ptr<DfxThread> keyThread);
    DfxProcess() = default;
    virtual ~DfxProcess() = default;
    void Detach();

    bool InitProcessMaps();
    bool InitOtherThreads(bool attach = true);

    void SetFatalMessage(const std::string &msg);
    std::string GetFatalMessage() const;
    void SetMaps(std::shared_ptr<DfxElfMaps> maps);
    std::shared_ptr<DfxElfMaps> GetMaps() const;
    void SetThreads(const std::vector<std::shared_ptr<DfxThread>> &threads);
    std::vector<std::shared_ptr<DfxThread>> GetThreads() const;
    std::shared_ptr<DfxThread> GetKeyThread() const;

    DfxProcessInfo processInfo_;
private:
    bool InitProcessThreads(std::shared_ptr<DfxThread> keyThread);
    void InsertThreadNode(pid_t tid, pid_t nsTid, bool attach = true);

private:
    std::string fatalMsg_ = "";
    std::shared_ptr<DfxElfMaps> maps_;
    std::shared_ptr<DfxThread> keyThread_ = nullptr;
    std::vector<std::shared_ptr<DfxThread>> threads_;
};
} // namespace HiviewDFX
} // namespace OHOS
#endif
