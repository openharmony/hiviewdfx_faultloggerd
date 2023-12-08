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

#include "dfx_process.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <securec.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_signal.h"
#include "dfx_util.h"
#include "procinfo.h"

namespace OHOS {
namespace HiviewDFX {
std::shared_ptr<DfxProcess> DfxProcess::Create(pid_t pid, pid_t nsPid)
{
    auto process = std::make_shared<DfxProcess>(pid, nsPid);
    return process;
}

DfxProcess::DfxProcess(pid_t pid, pid_t nsPid)
{
    InitProcessInfo(pid, nsPid);
}

void DfxProcess::InitProcessInfo(pid_t pid, pid_t nsPid)
{
    processInfo_.pid = pid;
    processInfo_.nsPid = nsPid;
    ReadProcessName(processInfo_.pid, processInfo_.processName);
}

bool DfxProcess::InitOtherThreads(bool attach)
{
    std::vector<int> tids;
    std::vector<int> nstids;
    if (!GetTidsByPid(processInfo_.pid, tids, nstids)) {
        return false;
    }

    for (size_t i = 0; i < nstids.size(); ++i) {
        if ((recycleTid_ > 0) && (nstids[i] == static_cast<int>(recycleTid_))) {
            DFXLOG_DEBUG("skip recycle thread:%d.", nstids[i]);
            continue;
        }

        if ((keyThread_ != nullptr) && nstids[i] == keyThread_->threadInfo_.nsTid) {
            DFXLOG_DEBUG("skip key thread:%d.", nstids[i]);
            continue;
        }

        auto thread = DfxThread::Create(processInfo_.pid, tids[i], nstids[i]);
        if (attach) {
            thread->Attach();
        }
        otherThreads_.push_back(thread);
    }
    return true;
}

pid_t DfxProcess::ChangeTid(pid_t tid, bool ns)
{
    if (processInfo_.pid == processInfo_.nsPid) {
        return tid;
    }

    if (kvThreads_.empty()) {
        std::vector<int> tids;
        std::vector<int> nstids;
        if (!GetTidsByPid(processInfo_.pid, tids, nstids)) {
            return tid;
        }
        for (size_t i = 0; i < nstids.size(); ++i) {
            kvThreads_[nstids[i]] = tids[i];
        }
    }

    for (auto iter = kvThreads_.begin(); iter != kvThreads_.end(); iter++) {
        if (ns && (iter->first == tid)) {
            return iter->second;
        }
        if (!ns && (iter->second == tid)) {
            return iter->first;
        }
    }
    return tid;
}

std::vector<std::shared_ptr<DfxThread>> DfxProcess::GetOtherThreads() const
{
    return otherThreads_;
}

void DfxProcess::ClearOtherThreads()
{
    otherThreads_.clear();
}

void DfxProcess::Attach(bool hasKey)
{
    if (hasKey && keyThread_) {
        keyThread_->Attach();
    }

    if (otherThreads_.empty()) {
        return;
    }
    for (auto thread : otherThreads_) {
        thread->Attach();
    }
}

void DfxProcess::Detach()
{
    if (otherThreads_.empty()) {
        return;
    }

    for (auto thread : otherThreads_) {
        thread->Detach();
    }
}

void DfxProcess::SetFatalMessage(const std::string &msg)
{
    fatalMsg_ = msg;
}

std::string DfxProcess::GetFatalMessage() const
{
    return fatalMsg_;
}

#if defined(__x86_64__)
void DfxProcess::SetMaps(std::shared_ptr<DfxElfMaps> maps)
{
    maps_ = maps;
}

void DfxProcess::InitProcessMaps()
{
    if (maps_ == nullptr) {
        if (processInfo_.pid <= 0) {
            return;
        }
        maps_ = DfxElfMaps::Create(processInfo_.pid);
    }
}

std::shared_ptr<DfxElfMaps> DfxProcess::GetMaps() const
{
    return maps_;
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
