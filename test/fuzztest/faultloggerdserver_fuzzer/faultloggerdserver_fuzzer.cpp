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

#include "faultloggerdserver_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <iostream>

#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {

bool GetValueFromData(const uint8_t** data, int32_t* key)
{
    errno_t err = memcpy_s(key, sizeof(int32_t), data, sizeof(int32_t));
    if (err != 0) {
        std::cout << "memcpy_s return value is abnormal!errno:" << errno << std::endl;
        return false;
    }
    *data += sizeof(int32_t);
    return true;
}

bool FaultloggerdServerTest(const uint8_t* data, size_t size)
{
    if (size < sizeof(int32_t) * 2) { // 2 : construct two int32_t parameters
        return true;
    }
    int32_t epollFd;
    int32_t connectionFd;
    int32_t type;
    int32_t pid;
    int32_t tid;
    int32_t uid;
    if (!GetValueFromData(&data, &epollFd)) {
        return false;
    }
    data += sizeof(int32_t);
    if (!GetValueFromData(&data, &connectionFd)) {
        return false;
    }
    if (!GetValueFromData(&data, &type)) {
        return false;
    }
    if (!GetValueFromData(&data, &pid)) {
        return false;
    }
    if (!GetValueFromData(&data, &tid)) {
        return false;
    }
    if (!GetValueFromData(&data, &uid)) {
        return false;
    }
    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.clientType = type % 10; // 10 : random a number between 1 and 10
    request.type = type;
    request.pid = pid;
    request.tid = tid;
    request.uid = uid;
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
#ifdef FAULTLOGGERD_FUZZER
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    daemon->HandleRequestForFuzzer(epollFd, connectionFd, &request, &request);
#endif
    return true;
}
} // namespace HiviewDFX
} // namespace OHOS

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (data == nullptr || size == 0) {
        std::cout << "invalid data" << std::endl;
        return 0;
    }

    /* Run your code on data */
    OHOS::HiviewDFX::FaultloggerdServerTest(data, size);
    return 0;
}
