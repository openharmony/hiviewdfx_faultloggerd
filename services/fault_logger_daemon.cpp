/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "fault_logger_daemon.h"

#include <csignal>
#include <memory>
#include <thread>

#include "dfx_log.h"
#include "dfx_util.h"
#include "epoll_manager.h"
#include "fault_logger_server.h"

#ifndef is_ohos_lite
#include "kernel_snapshot_task.h"
#endif

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr const char* const FAULTLOGGERD_DAEMON_TAG = "FAULT_LOGGER_DAEMON";
constexpr int32_t MAX_CONNECTION = 30;
constexpr int32_t MAX_EPOLL_EVENT = 1024;
}

FaultLoggerDaemon& FaultLoggerDaemon::GetInstance()
{
    static FaultLoggerDaemon faultLoggerDaemon;
    return faultLoggerDaemon;
}

FaultLoggerDaemon::FaultLoggerDaemon() : mainServer_(mainEpollManager_), tempFileManager_(secondaryEpollManager_)
{
    if (!mainEpollManager_.Init(MAX_EPOLL_EVENT)) {
        DFXLOGE("%{public}s :: Failed to init main epollManager", FAULTLOGGERD_DAEMON_TAG);
        return;
    }
    if (!secondaryEpollManager_.Init(MAX_EPOLL_EVENT)) {
        DFXLOGE("%{public}s :: Failed to init secondary epollManager", FAULTLOGGERD_DAEMON_TAG);
        return;
    }
    if (!mainServer_.Init()) {
        DFXLOGE("%{public}s :: Failed to init Faultloggerd Server", FAULTLOGGERD_DAEMON_TAG);
        return;
    }
    if (!tempFileManager_.Init()) {
        DFXLOGE("%{public}s :: Failed to init tempFileManager", FAULTLOGGERD_DAEMON_TAG);
        return;
    }
#ifndef is_ohos_lite
    if (OHOS::HiviewDFX::IsBetaVersion()) {
        secondaryEpollManager_.AddListener(std::make_unique<ReadKernelSnapshotTask>());
    }
#endif
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR || signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        DFXLOGE("%{public}s :: Failed to signal SIGCHLD or SIGPIPE, errno: %{public}d",
            FAULTLOGGERD_DAEMON_TAG, errno);
    }
}

FaultLoggerDaemon::~FaultLoggerDaemon()
{
    mainEpollManager_.StopEpoll();
    secondaryEpollManager_.StopEpoll();
}

int32_t FaultLoggerDaemon::StartServer()
{
    std::thread([this] {
        pthread_setname_np(pthread_self(), "HelperServer");
        secondaryEpollManager_.StartEpoll(MAX_CONNECTION);
    }).detach();
#ifdef FAULTLOGGERD_TEST
    constexpr auto epollTimeoutInMilliseconds = 3 * 1000;
#else
    constexpr auto epollTimeoutInMilliseconds = 20 * 1000;
#endif
    mainEpollManager_.StartEpoll(MAX_CONNECTION, epollTimeoutInMilliseconds);
    return 0;
}

EpollManager& FaultLoggerDaemon::GetEpollManager(EpollManagerType type)
{
    if (type == EpollManagerType::MAIN_SERVER) {
        return GetInstance().mainEpollManager_;
    }
    return GetInstance().secondaryEpollManager_;
}
}
}