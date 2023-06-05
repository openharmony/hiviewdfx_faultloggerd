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
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_signal.h"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {

std::shared_ptr<DfxProcess> DfxProcess::CreateProcessWithKeyThread(pid_t pid, pid_t nsPid, std::shared_ptr<DfxThread> keyThread)
{
    auto dfxProcess = std::make_shared<DfxProcess>();
    dfxProcess->processInfo_.pid = pid;
    dfxProcess->processInfo_.nsPid = nsPid;
    ReadProcessName(pid, dfxProcess->processInfo_.processName);

    if (!dfxProcess->InitProcessThreads(keyThread)) {
        DFXLOG_WARN("Fail to init threads.");
        return nullptr;
    }

    DFXLOG_DEBUG("Init process dump with pid:%d.", pid);
    return dfxProcess;
}

bool DfxProcess::InitProcessMaps()
{
    if (processInfo_.pid <= 0) {
        return false;
    }

    auto maps = DfxElfMaps::Create(processInfo_.pid);
    if (!maps) {
        return false;
    }
    SetMaps(maps);
    return true;
}

bool DfxProcess::InitProcessThreads(std::shared_ptr<DfxThread> keyThread)
{
    if (!keyThread) {
        keyThread = DfxThread::Create(processInfo_.pid, processInfo_.pid, processInfo_.pid);
    }

    if (!keyThread->Attach()) {
        DFXLOG_WARN("Fail to attach thread.");
        return false;
    }
    keyThread_ = keyThread;
    threads_.push_back(keyThread);
    return true;
}

bool DfxProcess::InitOtherThreads(bool attach)
{
    std::vector<int> tids;
    std::vector<int> nstids;
    if (!GetTidsByPid(processInfo_.pid, tids, nstids)) {
        return false;
    }

    for (size_t i = 0; i < nstids.size(); ++i) {
        if (nstids[i] == processInfo_.recycleTid) {
            DFXLOG_DEBUG("skip recycle tid:%d.", processInfo_.recycleTid);
            continue;
        }

        if (nstids[i] == keyThread_->threadInfo_.nsTid) {
            DFXLOG_DEBUG("skip key thread tid:%d.", keyThread_->threadInfo_.nsTid);
            keyThread_->threadInfo_.tid = tids[i];
            ReadThreadName(keyThread_->threadInfo_.tid, keyThread_->threadInfo_.threadName);
            continue;
        }

        for (auto iter = threads_.begin(); iter != threads_.end(); iter++) {
            if ((*iter)->threadInfo_.nsTid == nstids[i]) {
                continue;
            }
        }
        auto thread = DfxThread::Create(processInfo_.pid, tids[i], nstids[i]);
        if (attach) {
            thread->Attach();
        }
        threads_.push_back(thread);
    }
    return true;
}

void DfxProcess::SetFatalMessage(const std::string &msg)
{
    fatalMsg_ = msg;
}

std::string DfxProcess::GetFatalMessage() const
{
    return fatalMsg_;
}

void DfxProcess::SetMaps(std::shared_ptr<DfxElfMaps> maps)
{
    maps_ = maps;
}

std::shared_ptr<DfxElfMaps> DfxProcess::GetMaps() const
{
    return maps_;
}

void DfxProcess::SetThreads(const std::vector<std::shared_ptr<DfxThread>> &threads)
{
    threads_ = threads;
}

std::vector<std::shared_ptr<DfxThread>> DfxProcess::GetThreads() const
{
    return threads_;
}

std::shared_ptr<DfxThread> DfxProcess::GetKeyThread() const
{
    return keyThread_;
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
} // namespace HiviewDFX
} // namespace OHOS
