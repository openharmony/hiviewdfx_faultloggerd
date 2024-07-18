/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "faultloggerdclient_fuzzer.h"

#include <cstddef>
#include <iostream>
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_fuzzertest_common.h"

namespace OHOS {
namespace HiviewDFX {

void FaultloggerdClientTest(const uint8_t* data, size_t size)
{
    int32_t type;
    int32_t pid;
    int32_t tid;

    int offsetTotalLength = sizeof(type) + sizeof(pid) + sizeof(tid);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, type);
    STREAM_TO_VALUEINFO(data, pid);
    STREAM_TO_VALUEINFO(data, tid);

    RequestFileDescriptor(type);
    RequestPipeFd(pid, type);
    RequestDelPipeFd(pid);
    RequestCheckPermission(pid);
    RequestSdkDump(pid, tid);
}
} // namespace HiviewDFX
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        return 0;
    }

    /* Run your code on data */
    OHOS::HiviewDFX::FaultloggerdClientTest(data, size);
    return 0;
}
