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

/* This files contains process module. */

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
#include "dfx_maps.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_signal.h"
#include "dfx_thread.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {

std::shared_ptr<DfxProcess> DfxProcess::CreateProcessWithKeyThread(pid_t pid, std::shared_ptr<DfxThread> keyThread)
{
    auto dfxProcess = std::make_shared<DfxProcess>();
    dfxProcess->SetPid(pid);
    std::string processName;
    ReadProcessName(pid, processName);
    dfxProcess->SetProcessName(processName);

    if (!dfxProcess->InitProcessMaps()) {
        DFXLOG_WARN("Fail to init process maps.");
        return nullptr;
    }
    if (!dfxProcess->InitProcessThreads(keyThread)) {
        DFXLOG_WARN("Fail to init threads.");
        return nullptr;
    }

    DFXLOG_DEBUG("Init process dump with pid:%d.", dfxProcess->GetPid());
    return dfxProcess;
}

bool DfxProcess::InitProcessMaps()
{
    auto maps = DfxElfMaps::Create(pid_);
    if (!maps) {
        return false;
    }

    SetMaps(maps);
    return true;
}

bool DfxProcess::InitProcessThreads(std::shared_ptr<DfxThread> keyThread)
{
    if (!keyThread) {
        keyThread = std::make_shared<DfxThread>(pid_, pid_, pid_);
    }

    if (!keyThread->Attach()) {
        DFXLOG_WARN("Fail to attach thread.");
        return false;
    }
    threads_.push_back(keyThread);
    return true;
}

void DfxProcess::SetRecycleTid(pid_t nstid)
{
    recycleTid_ = nstid;
}

bool DfxProcess::InitOtherThreads(bool attach)
{
    std::vector<int> tids;
    std::vector<int> nstids;
    if (!GetTidsByPid(GetPid(), tids, nstids)) {
        return false;
    }

    for (size_t i = 0; i < nstids.size(); ++i) {
        if (isSignalDump_ && (nstids[i] == recycleTid_)) {
            DFXLOG_DEBUG("skip recycle tid:%d nstid:%d.", recycleTid_, nstids[i]);
            continue;
        }

        InsertThreadNode(tids[i], nstids[i], attach);
    }
    return true;
}

void DfxProcess::InsertThreadNode(pid_t tid, pid_t nsTid, bool attach)
{
    for (auto iter = threads_.begin(); iter != threads_.end(); iter++) {
        if ((*iter)->GetRealTid() == nsTid) {
            (*iter)->SetThreadId(tid);
            std::string threadName;
            ReadThreadName(tid, threadName);
            (*iter)->SetThreadName(threadName);
            return;
        }
    }

    auto thread = std::make_shared<DfxThread>(pid_, tid, nsTid);
    if (attach) {
        thread->Attach();
    }
    threads_.push_back(thread);
}

void DfxProcess::SetIsSignalDump(bool isSignalDump)
{
    isSignalDump_ = isSignalDump;
}

bool DfxProcess::GetIsSignalDump() const
{
    return isSignalDump_;
}

pid_t DfxProcess::GetPid() const
{
    return pid_;
}

uid_t DfxProcess::GetUid() const
{
    return uid_;
}

bool DfxProcess::GetNs() const
{
    return pid_ != nsPid_;
}

std::string DfxProcess::GetProcessName() const
{
    return processName_;
}

std::string DfxProcess::GetFatalMessage() const
{
    return fatalMsg_;
}

std::shared_ptr<DfxElfMaps> DfxProcess::GetMaps() const
{
    return maps_;
}

std::vector<std::shared_ptr<DfxThread>> DfxProcess::GetThreads() const
{
    return threads_;
}

void DfxProcess::SetPid(pid_t pid)
{
    pid_ = pid;
}

void DfxProcess::SetUid(uid_t uid)
{
    uid_ = uid;
}

void DfxProcess::SetNsPid(pid_t pid)
{
    nsPid_ = pid;
}

pid_t DfxProcess::GetNsPid() const
{
    if (nsPid_ > 0) {
        return nsPid_;
    }
    return pid_;
}

void DfxProcess::SetProcessName(const std::string &processName)
{
    processName_ = processName;
}

void DfxProcess::SetFatalMessage(const std::string &msg)
{
    fatalMsg_ = msg;
}

void DfxProcess::SetMaps(std::shared_ptr<DfxElfMaps> maps)
{
    maps_ = maps;
}

void DfxProcess::SetThreads(const std::vector<std::shared_ptr<DfxThread>> &threads)
{
    threads_ = threads;
}

void DfxProcess::Detach()
{
    if (threads_.empty()) {
        return;
    }

    for (auto iter = threads_.begin(); iter != threads_.end(); iter++) {
        (*iter)->Detach();
    }
}

void DfxProcess::PrintProcessMapsByConfig()
{
    if (DfxConfig::GetConfig().displayMaps) {
        if (!GetMaps()) {
            DFXLOG_WARN("Maps is null");
            return;
        }
        DfxRingBufferWrapper::GetInstance().AppendMsg("\nMaps:\n");
        auto mapsVector = maps_->GetMaps();
        for (auto iter = mapsVector.begin(); iter != mapsVector.end(); iter++) {
            DfxRingBufferWrapper::GetInstance().AppendMsg((*iter)->PrintMap());
        }
    } else {
        DFXLOG_DEBUG("hidden Maps");
    }
}

void DfxProcess::PrintThreadsHeaderByConfig()
{
    if (DfxConfig::GetConfig().displayBacktrace) {
        if (!isSignalDump_) {
            DfxRingBufferWrapper::GetInstance().AppendMsg("Other thread info:\n");
        }
    } else {
        DFXLOG_DEBUG("hidden thread info.");
    }
}
} // namespace HiviewDFX
} // namespace OHOS
