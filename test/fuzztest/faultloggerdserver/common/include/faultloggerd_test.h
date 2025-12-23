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

#ifndef FAULTLOGGERD_TEST_H
#define FAULTLOGGERD_TEST_H

#include "dfx_socket_request.h"
#include "faultloggerd_socket.h"

void SendRequestToServer(const OHOS::HiviewDFX::SocketRequestData &socketRequestData,
    OHOS::HiviewDFX::SocketFdData* socketFdData = nullptr);

void FillRequestHeadData(RequestDataHead& head, FaultLoggerClientType clientType);

#endif