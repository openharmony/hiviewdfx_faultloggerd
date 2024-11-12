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

#include <memory>
#include <thread>
#include "epoll_manager.h"
#include "fault_logger_server.h"
#include "dfx_log.h"
#include "csignal"
#include "dfx_util.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr int32_t MAX_CONNECTION = 30;
constexpr const char* const FAULTLOGGERD_DAEMON_TAG = "FAULTLOGGERD_DAEMON";
constexpr int32_t MAX_EPOLL_EVENT = 1024;
}

FaultLoggerDaemon& FaultLoggerDaemon::GetInstance()
{
    static FaultLoggerDaemon faultLoggerDaemon;
    return faultLoggerDaemon;
}

FaultLoggerDaemon::FaultLoggerDaemon()
{
    if (!mainEpollManager_.CreateEpoll(MAX_EPOLL_EVENT)) {
        DFXLOGE("%{public}s :: Failed to init main epollManager", FAULTLOGGERD_DAEMON_TAG);
        return;
    }
    mainServer_ = std::unique_ptr<SocketServer>(new (std::nothrow) SocketServer(mainEpollManager_));
    if (!mainServer_ || !mainServer_->Init()) {
        DFXLOGE("%{public}s :: Failed to init Faultloggerd Server", FAULTLOGGERD_DAEMON_TAG);
        return;
    }

    if (!secondaryEpollManager_.CreateEpoll(MAX_EPOLL_EVENT)) {
        DFXLOGE("%{public}s :: Failed to init secondary epollManager", FAULTLOGGERD_DAEMON_TAG);
        return;
    }
    tempFileManager_ = std::unique_ptr<TempFileManager>(new (std::nothrow) TempFileManager(secondaryEpollManager_));
    if (!tempFileManager_->Init()) {
        DFXLOGE("%{public}s :: Failed to init tempFileManager", FAULTLOGGERD_DAEMON_TAG);
    }
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR || signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        DFXLOGE("%{public}s :: Failed to signal SIGCHLD or SIGPIPE", FAULTLOGGERD_DAEMON_TAG);
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
        secondaryEpollManager_.StartEpoll(MAX_CONNECTION);
    }).detach();
    mainEpollManager_.StartEpoll(MAX_CONNECTION);
    return 0;
}
}
}