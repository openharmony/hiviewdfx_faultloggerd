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
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_fuzzertest_common.h"

namespace OHOS {
namespace HiviewDFX {
void FaultloggerdServerTest(const uint8_t* data, size_t size)
{
    int32_t epollFd;
    int32_t connectionFd;
    int32_t type;
    int32_t pid;
    int32_t tid;
    int32_t uid;

    int offsetTotalLength = sizeof(epollFd) + sizeof(connectionFd) + sizeof(type) + sizeof(pid) +
                            sizeof(tid) + sizeof(uid);
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, epollFd);
    STREAM_TO_VALUEINFO(data, connectionFd);
    STREAM_TO_VALUEINFO(data, type);
    STREAM_TO_VALUEINFO(data, pid);
    STREAM_TO_VALUEINFO(data, tid);
    STREAM_TO_VALUEINFO(data, uid);

    if (epollFd < 0 || connectionFd < 3) { // 3: not allow fd = 0,1,2 because they are reserved by system
        return;
    }

    struct FaultLoggerdRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.type = type;
    request.pid = pid;
    request.tid = tid;
    request.uid = uid;
    request.time = OHOS::HiviewDFX::GetTimeMilliSeconds();
#ifdef FAULTLOGGERD_FUZZER
    std::shared_ptr<FaultLoggerDaemon> daemon = std::make_shared<FaultLoggerDaemon>();
    daemon->HandleRequestForFuzzer(epollFd, connectionFd, &request, &request);
#endif
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
    OHOS::HiviewDFX::FaultloggerdServerTest(data, size);
    return 0;
}
