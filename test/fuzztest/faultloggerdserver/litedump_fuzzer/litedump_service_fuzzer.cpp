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

#define FUZZ_PROJECT_NAME "litedump_fuzzer"

#include "dfx_socket_request.h"
#include "faultloggerd_test.h"

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FaultLoggerdTestServer::GetInstance();
    if (data == nullptr) {
        return 0;
    }
    if (size < sizeof (FaultLoggerdRequest)) {
        return 0;
    }
    FaultLoggerdRequest litedumpRequest = *reinterpret_cast<const FaultLoggerdRequest*>(data);
    FillRequestHeadData(litedumpRequest.head, FaultLoggerClientType::LIMITED_PROCESS_DUMP_CLIENT);
    SendRequestToServer({&litedumpRequest, sizeof(FaultLoggerdRequest)});

    if (size < sizeof (LiteDumpFdRequestData)) {
        return 0;
    }
    LiteDumpFdRequestData litedumpFdRequest = *reinterpret_cast<const LiteDumpFdRequestData*>(data);
    FillRequestHeadData(litedumpFdRequest.head, FaultLoggerClientType::PIPE_FD_LIMITED_CLIENT);
    SendRequestToServer({&litedumpFdRequest, sizeof(LiteDumpFdRequestData)});

    return 0;
}
