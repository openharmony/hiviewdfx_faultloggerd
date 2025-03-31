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

#include "dfx_process.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <securec.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <vector>

#include "dfx_config.h"
#include "dfx_define.h"
#include "dfx_logger.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_signal.h"
#include "dfx_util.h"
#include "procinfo.h"
#include "unique_fd.h"

namespace OHOS {
namespace HiviewDFX {
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
            DFXLOGD("skip recycle thread:%{public}d.", nstids[i]);
            continue;
        }

        if ((keyThread_ != nullptr) && nstids[i] == keyThread_->threadInfo_.nsTid) {
            DFXLOGD("skip key thread:%{public}d.", nstids[i]);
            continue;
        }

        auto thread = DfxThread::Create(processInfo_.pid, tids[i], nstids[i]);
        if (attach) {
            thread->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT);
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

std::vector<std::shared_ptr<DfxThread>>& DfxProcess::GetOtherThreads()
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
        keyThread_->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT);
    }

    if (otherThreads_.empty()) {
        return;
    }
    for (auto& thread : otherThreads_) {
        if (thread->threadInfo_.nsTid == processInfo_.nsPid) {
            thread->Attach(PTRACE_ATTATCH_KEY_THREAD_TIMEOUT);
            continue;
        }
        thread->Attach(PTRACE_ATTATCH_OTHER_THREAD_TIMEOUT);
    }
}

void DfxProcess::Detach()
{
    if (otherThreads_.empty()) {
        return;
    }

    for (auto& thread : otherThreads_) {
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

namespace {
bool GetProcessInfo(pid_t tid, unsigned long long &startTime)
{
    std::string path = "/proc/" +std::to_string(tid);
    UniqueFd dirFd(open(path.c_str(), O_DIRECTORY | O_RDONLY));
    if (dirFd == -1) {
        DFXLOGE("GetProcessInfo open %{public}s fail. errno %{public}d", path.c_str(), errno);
        return false;
    }

    UniqueFd statFd(openat(dirFd.Get(), "stat", O_RDONLY | O_CLOEXEC));
    if (statFd == -1) {
        DFXLOGE("GetProcessInfo open %{public}s/stat fail. errno %{public}d", path.c_str(), errno);
        return false;
    }

    std::string statStr;
    if (!ReadFdToString(statFd.Get(), statStr)) {
        DFXLOGE("GetProcessInfo read string fail.");
        return false;
    }

    std::string eoc = statStr.substr(statStr.find_last_of(")"));
    std::istringstream is(eoc);
    constexpr int startTimePos = 21;
    constexpr int base = 10;
    int pos = 0;
    std::string tmp;
    while (is >> tmp && pos <= startTimePos) {
        pos++;
        if (pos == startTimePos) {
            startTime = strtoull(tmp.c_str(), nullptr, base);
            return true;
        }
    }
    DFXLOGE("GetProcessInfo Get process info fail.");
    return false;
}
}

std::string DfxProcess::GetProcessLifeCycle(pid_t pid)
{
    struct sysinfo si;
    sysinfo(&si);
    unsigned long long startTime = 0;
    if (GetProcessInfo(pid, startTime)) {
        auto clkTck = sysconf(_SC_CLK_TCK);
        if (clkTck == -1) {
            DFXLOGE("Get _SC_CLK_TCK fail. errno %{public}d", errno);
            return "";
        }
        uint64_t upTime = si.uptime - startTime / static_cast<uint32_t>(clkTck);
        return std::to_string(upTime) + "s";
    }
    return "";
}
} // namespace HiviewDFX
} // namespace OHOS
