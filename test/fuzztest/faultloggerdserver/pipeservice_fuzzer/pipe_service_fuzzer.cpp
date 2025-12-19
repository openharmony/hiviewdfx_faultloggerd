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

#define FUZZ_PROJECT_NAME "pipeservice_fuzzer"

#include "dfx_define.h"
#include "faultloggerd_client.h"
#include "faultloggerd_test.h"
#include "faultloggerd_test_server.h"

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FaultLoggerdTestServer::GetInstance();
    if (data == nullptr || size < sizeof (PipFdRequestData)) {
        return 0;
    }
    PipFdRequestData requestData = *reinterpret_cast<const PipFdRequestData*>(data);
    FillRequestHeadData(requestData.head, FaultLoggerClientType::PIPE_FD_CLIENT);
    int32_t fds[PIPE_NUM_SZ] = {-1, -1};
    OHOS::HiviewDFX::SocketFdData fdPair =  {fds, 2};
    SendRequestToServer({&requestData, sizeof(PipFdRequestData)}, &fdPair);
    OHOS::HiviewDFX::SmartFd {fds[PIPE_BUF_INDEX]};
    OHOS::HiviewDFX::SmartFd {fds[PIPE_RES_INDEX]};
    RequestDelPipeFd(requestData.pid);
    return 0;
}
