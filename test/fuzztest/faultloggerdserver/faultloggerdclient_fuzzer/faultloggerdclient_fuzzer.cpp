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

#define FUZZ_PROJECT_NAME "faultloggerdclient_fuzzer"

#include "dfx_define.h"
#include "faultloggerd_client.h"
#include "faultloggerd_test.h"

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FaultLoggerdTestServer::GetInstance();
    if (data == nullptr || size < sizeof(FaultLoggerdRequest)) {
        return 0;
    }
    FaultLoggerdRequest request = *reinterpret_cast<const FaultLoggerdRequest*>(data);
    RequestFileDescriptor(request.type);
    int pipeReadFd[PIPE_NUM_SZ] = {-1, -1};
    RequestSdkDump(request.pid, request.tid, pipeReadFd);
    OHOS::HiviewDFX::SmartFd {pipeReadFd[PIPE_BUF_INDEX]};
    OHOS::HiviewDFX::SmartFd {pipeReadFd[PIPE_RES_INDEX]};
    RequestPipeFd(request.pid, request.type, pipeReadFd);
    OHOS::HiviewDFX::SmartFd {pipeReadFd[PIPE_BUF_INDEX]};
    OHOS::HiviewDFX::SmartFd {pipeReadFd[PIPE_RES_INDEX]};
    RequestDelPipeFd(request.pid);
    OHOS::HiviewDFX::SmartFd {RequestFileDescriptorEx(&request)};
    return 0;
}
