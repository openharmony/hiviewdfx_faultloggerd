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

#include "fault_logger_server.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_log.h"
#include "faultloggerd_socket.h"
#include "fault_coredump_service.h"

namespace OHOS {
namespace HiviewDFX {

namespace {
constexpr const char* const FAULTLOGGERD_SERVER_TAG = "FAULT_LOGGER_SERVER";
}

SocketServer::SocketServer(EpollManager& epollManager) : epollManager_(epollManager) {}

bool SocketServer::Init()
{
    std::unique_ptr<IFaultLoggerService> logFileDesService(new (std::nothrow) FileDesService());
    AddService(LOG_FILE_DES_CLIENT, std::move(logFileDesService));
#ifndef HISYSEVENT_DISABLE
    std::unique_ptr<IFaultLoggerService> reportExceptionService(new (std::nothrow) ExceptionReportService());
    AddService(REPORT_EXCEPTION_CLIENT, std::move(reportExceptionService));
    std::unique_ptr<IFaultLoggerService> statsClientService(new (std::nothrow) StatsService());
    AddService(DUMP_STATS_CLIENT, std::move(statsClientService));
#endif
    if (!AddServerListener(SERVER_SOCKET_NAME) || !AddServerListener(SERVER_CRASH_SOCKET_NAME)) {
        return false;
    }
#ifndef is_ohos_lite
    AddService(COREDUMP_CLIENT, std::make_unique<CoredumpService>());
    AddService(COREDUMP_PROCESS_DUMP_CLIENT, std::make_unique<CoredumpStatusService>());

    AddService(PIPE_FD_CLIENT, std::make_unique<PipeService>());
    AddService(SDK_DUMP_CLIENT, std::make_unique<SdkDumpService>());
    if (!AddServerListener(SERVER_SDKDUMP_SOCKET_NAME)) {
        return false;
    }
#endif
    return true;
}

void SocketServer::AddService(int32_t clientType, std::unique_ptr<IFaultLoggerService> service)
{
    if (service) {
        faultLoggerServices_.emplace_back(clientType, std::move(service));
    }
}

bool SocketServer::AddServerListener(const char* socketName)
{
    constexpr int32_t maxConnection = 30;
    SmartFd fd = StartListen(socketName, maxConnection);
    if (!fd) {
        return false;
    }
    std::unique_ptr<EpollListener> serverListener(new SocketServerListener(*this, std::move(fd), socketName));
    return epollManager_.AddListener(std::move(serverListener));
}

SocketServer::SocketServerListener::SocketServerListener(SocketServer& socketServer, SmartFd fd, std::string socketName)
    : EpollListener(std::move(fd), true), socketServer_(socketServer), socketName_(std::move(socketName)) {}

SocketServer::ClientRequestListener::ClientRequestListener(
    SocketServerListener& socketServerListener, SmartFd fd, uid_t clientUid)
    : EpollListener(std::move(fd)), socketServerListener_(socketServerListener), clientUid_(clientUid) {}

SocketServer::ClientRequestListener::~ClientRequestListener()
{
    uint32_t& connectionNum = socketServerListener_.socketServer_.connectionNums_[clientUid_];
    if (--connectionNum == 0) {
        socketServerListener_.socketServer_.connectionNums_.erase(clientUid_);
    }
}

IFaultLoggerService* SocketServer::ClientRequestListener::GetTargetService(int32_t faultLoggerClientType) const
{
    for (const auto& faultLoggerServicePair : socketServerListener_.socketServer_.faultLoggerServices_) {
        if (faultLoggerServicePair.first == faultLoggerClientType) {
            return faultLoggerServicePair.second.get();
        }
    }
    return nullptr;
}

void SocketServer::ClientRequestListener::OnEventPoll()
{
    constexpr int32_t maxBuffSize = 2048;
    std::vector<uint8_t> buf(maxBuffSize, 0);
    ssize_t nread = OHOS_TEMP_FAILURE_RETRY(read(GetFd(), buf.data(), maxBuffSize));
    if (nread >= static_cast<ssize_t>(sizeof(RequestDataHead))) {
        auto dataHead = reinterpret_cast<RequestDataHead*>(buf.data());
        DFXLOGI("%{public}s :: %{public}s receive request from pid: %{public}d, clientType: %{public}d",
                FAULTLOGGERD_SERVER_TAG, socketServerListener_.socketName_.c_str(),
                dataHead->clientPid, dataHead->clientType);
        IFaultLoggerService* service = GetTargetService(dataHead->clientType);
        int32_t retCode = service ? service->OnReceiveMsg(socketServerListener_.socketName_, GetFd(), nread, buf)
            : ResponseCode::UNKNOWN_CLIENT_TYPE;
        if (retCode != ResponseCode::REQUEST_SUCCESS) {
            SendMsgToSocket(GetFd(), &retCode, sizeof(retCode));
        }
        DFXLOGI("%{public}s :: %{public}s has processed request for pid: %{public}d, clientType: %{public}d, "
            "and retCode %{public}d", FAULTLOGGERD_SERVER_TAG, socketServerListener_.socketName_.c_str(),
            dataHead->clientPid, dataHead->clientType, retCode);
    }
}

void SocketServer::SocketServerListener::OnEventPoll()
{
    struct sockaddr_un clientAddr{};
    auto clientAddrSize = static_cast<socklen_t>(sizeof(clientAddr));
    SmartFd connectionFd{static_cast<int>(OHOS_TEMP_FAILURE_RETRY(accept(GetFd(),
        reinterpret_cast<struct sockaddr*>(&clientAddr), &clientAddrSize)))};
    if (!connectionFd) {
        DFXLOGW("%{public}s :: Failed to accept connection from %{public}s",
            FAULTLOGGERD_SERVER_TAG, socketName_.c_str());
        return;
    }
    struct ucred credentials{};
    socklen_t len = sizeof(credentials);
    if (getsockopt(connectionFd.GetFd(), SOL_SOCKET, SO_PEERCRED, &credentials, &len) == -1) {
        DFXLOGE("%{public}s :: Failed to GetCredential, errno: %{public}d", FAULTLOGGERD_SERVER_TAG, errno);
        return;
    }
    auto& connectionNum = socketServer_.connectionNums_[credentials.uid];
#ifdef FAULTLOGGERD_TEST
    constexpr auto connectionLimit = 1;
#else
    constexpr auto connectionLimit = 5;
#endif
    if (connectionNum >= connectionLimit) {
        DFXLOGE("%{public}s :: reject new connection for uid %{public}d.", FAULTLOGGERD_SERVER_TAG, credentials.uid);
        return;
    }
    connectionNum++;
    std::unique_ptr<EpollListener> clientRequestListener(
        new (std::nothrow) ClientRequestListener(*this, std::move(connectionFd), credentials.uid));
    socketServer_.epollManager_.AddListener(std::move(clientRequestListener));
}
}
}