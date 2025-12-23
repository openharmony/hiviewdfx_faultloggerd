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

#define FUZZ_PROJECT_NAME "faultloggerdcoredumpservice_fuzzer"

#include "dfx_socket_request.h"
#include "faultloggerd_test.h"
#include "faultloggerd_test_server.h"

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FaultLoggerdTestServer::GetInstance();
    if (data == nullptr) {
        return 0;
    }
    if (size < sizeof (CoreDumpRequestData)) {
        return 0;
    }
    CoreDumpRequestData coredumpRequestData = *reinterpret_cast<const CoreDumpRequestData*>(data);
    FillRequestHeadData(coredumpRequestData.head, FaultLoggerClientType::COREDUMP_CLIENT);
    SendRequestToServer({&coredumpRequestData, sizeof(CoreDumpRequestData)});

    if (size < sizeof (CoreDumpStatusData)) {
        return 0;
    }
    CoreDumpStatusData coredumpStatusData = *reinterpret_cast<const CoreDumpStatusData*>(data);
    FillRequestHeadData(coredumpStatusData.head, FaultLoggerClientType::COREDUMP_PROCESS_DUMP_CLIENT);
    SendRequestToServer({&coredumpStatusData, sizeof(CoreDumpStatusData)});

    return 0;
}
