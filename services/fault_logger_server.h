/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include <map>
#include "epoll_manager.h"
#include "fault_logger_service.h"

namespace OHOS {
namespace HiviewDFX {
class SocketServer {
public:
    bool Init();
    void AddService(int32_t clientType, std::unique_ptr<IFaultLoggerService> service);
private:
    bool AddServerListener(const char* socketName);
    class SocketServerListener : public EpollListener {
    public:
        SocketServerListener(SocketServer& socketServer, SmartFd fd, std::string socketName);
        void OnEventPoll() override;
        SocketServer& socketServer_;
        const std::string socketName_;
    };

    class ClientRequestListener : public EpollListener {
    public:
        ClientRequestListener(SocketServerListener& socketServerListener, SmartFd fd, uid_t clientUid);
        ~ClientRequestListener() override;
        void OnEventPoll() override;
    private:
        SocketServerListener& socketServerListener_;
        uid_t clientUid_;
        IFaultLoggerService* GetTargetService(int32_t faultLoggerClientType) const;
    };
    std::vector<std::pair<int32_t, std::unique_ptr<IFaultLoggerService>>> faultLoggerServices_{};
    std::map<uid_t, uint32_t> connectionNums_{};
};
}
}
#endif // FAULT_LOGGER_SERVER_H_
