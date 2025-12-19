/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#define FUZZ_PROJECT_NAME "filedescservice_fuzzer"

#include "dfx_socket_request.h"
#include "faultloggerd_test.h"
#include "faultloggerd_test_server.h"

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FaultLoggerdTestServer::GetInstance();
    if (data == nullptr || size < sizeof (FaultLoggerdRequest)) {
        return 0;
    }
    FaultLoggerdRequest requestData = *reinterpret_cast<const FaultLoggerdRequest*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::LOG_FILE_DES_CLIENT);
    int32_t fd = -1;
    OHOS::HiviewDFX::SocketFdData fds =  {&fd, 1};
    SendRequestToServer({&requestData, sizeof(FaultLoggerdRequest)}, &fds);
    OHOS::HiviewDFX::SmartFd {fd};
    return 0;
}
