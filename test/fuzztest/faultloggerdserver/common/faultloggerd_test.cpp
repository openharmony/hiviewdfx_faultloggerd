/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "faultloggerd_test.h"

#include <thread>

#include "fault_logger_daemon.h"

using namespace OHOS::HiviewDFX;

void SendRequestToServer(const SocketRequestData &socketRequestData, SocketFdData* socketFdData)
{
    FaultLoggerdSocket faultLoggerdSocket;
    if (!faultLoggerdSocket.CreateSocketFileDescriptor(0)) {
        return;
    }
    std::string socketNames[] = { SERVER_SOCKET_NAME, SERVER_SDKDUMP_SOCKET_NAME, SERVER_CRASH_SOCKET_NAME };
    srand(static_cast<unsigned>(time(nullptr)));
    int randomSocketIndex = rand() % 3;
    if (faultLoggerdSocket.StartConnect(socketNames[randomSocketIndex].c_str())) {
        socketFdData ? faultLoggerdSocket.RequestFdsFromServer(socketRequestData, *socketFdData) :
            faultLoggerdSocket.RequestServer(socketRequestData);
    }
    faultLoggerdSocket.CloseSocketFileDescriptor();
}

void FillRequestHeadData(RequestDataHead& head, FaultLoggerClientType clientType)
{
    head.clientType = clientType;
    head.clientPid = getpid();
}

FaultLoggerdTestServer &FaultLoggerdTestServer::GetInstance()
{
    static FaultLoggerdTestServer faultLoggerdTestServer;
    return faultLoggerdTestServer;
}

FaultLoggerdTestServer::FaultLoggerdTestServer()
{
    std::thread([] {
        OHOS::HiviewDFX::FaultLoggerDaemon::GetInstance().StartServer();
    }).detach();
    constexpr int32_t faultLoggerdInitTime = 2;
    // Pause for two seconds to wait for the server to initialize.
    std::this_thread::sleep_for(std::chrono::seconds(faultLoggerdInitTime));
}