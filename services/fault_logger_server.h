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

#ifndef FAULT_LOGGER_SERVER_H_
#define FAULT_LOGGER_SERVER_H_

#include "epoll_manager.h"
#include "fault_logger_service.h"
#include "dfx_socket_request.h"

namespace OHOS {
namespace HiviewDFX {
class SocketServer {
public:
    explicit SocketServer(EpollManager& epollManager);
    bool Init();
    void AddService(int32_t clientType, std::unique_ptr<IFaultLoggerService> service);
private:
    bool AddServerListener(const char* socketName);
    class SocketServerListener : public EpollListener {
    public:
        SocketServerListener(SocketServer& socketServer, int32_t fd, std::string socketName);
        void OnEventPoll() override;
        SocketServer& socketServer_;
        const std::string socketName_;
    };

    class ClientRequestListener : public EpollListener {
    public:
        ClientRequestListener(SocketServerListener& socketServerListener, int32_t fd);
        void OnEventPoll() override;
        SocketServerListener& socketServerListener_;

    private:
        IFaultLoggerService* GetTargetService(int32_t faultLoggerClientType);
    };
    EpollManager& epollManager_;
    std::vector<std::pair<int32_t, std::unique_ptr<IFaultLoggerService>>> faultLoggerServices_{};
};
}
}
#endif // FAULT_LOGGER_SERVER_H_
