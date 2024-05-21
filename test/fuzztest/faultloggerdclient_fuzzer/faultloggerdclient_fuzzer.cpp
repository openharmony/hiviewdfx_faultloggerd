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
#include <cstdint>
#include <iostream>
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {

bool FaultloggerdClientTest(const uint8_t* data, size_t size)
{
    std::cout << "enter FaultloggerdClientTest, size:" << size << std::endl;
    if (size < sizeof(int32_t) * 3) { // 3 : construct three int32_t parameters
        return true;
    }
    int32_t type[1];
    int32_t pid[1];
    int32_t tid[1];
    errno_t err = memcpy_s(type, sizeof(type), data, sizeof(int32_t));
    if (err != 0) {
        std::cout << "memcpy_s return value is abnormal" << std::endl;
        return false;
    }
    data += sizeof(int32_t);
    err = memcpy_s(tid, sizeof(tid), data, sizeof(int32_t));
    if (err != 0) {
        std::cout << "memcpy_s return value is abnormal" << std::endl;
        return false;
    }
    data += sizeof(int32_t);
    err = memcpy_s(pid, sizeof(pid), data, sizeof(int32_t));
    if (err != 0) {
        std::cout << "memcpy_s return value is abnormal" << std::endl;
        return false;
    }

    RequestFileDescriptor(type[0]);
    RequestPipeFd(pid[0], type[0]);
    RequestDelPipeFd(pid[0]);
    RequestCheckPermission(pid[0]);
    RequestSdkDump(pid[0], tid[0]);
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
    OHOS::HiviewDFX::FaultloggerdClientTest(data, size);
    return 0;
}
