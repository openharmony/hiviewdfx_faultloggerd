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

#include "faultloggerddumpcatcher_fuzzer.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include "dfx_dump_catcher.h"
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "securec.h"

namespace OHOS {
namespace HiviewDFX {
static const int PID_SIZE = 4;
static const int RAND_BUF_LIMIT = 9;

bool DumpStackTraceTest(const uint8_t* data, size_t size)
{
    if (size < RAND_BUF_LIMIT) {
        return true;
    }
    std::shared_ptr<DfxDumpCatcher> catcher = std::make_shared<DfxDumpCatcher>();
    std::string msg;
    int pid[1];
    int tid[1];
    errno_t err = memcpy_s(pid, sizeof(pid), data, PID_SIZE);
    if (err != 0) {
        std::cout << "memcpy_s return value is abnormal" << std::endl;
        return false;
    }
    data += PID_SIZE;
    err = memcpy_s(tid, sizeof(tid), data, PID_SIZE);
    if (err != 0) {
        std::cout << "memcpy_s return value is abnormal" << std::endl;
        return false;
    }
    data += PID_SIZE;
    char invalidOption = *data;
    catcher->DumpCatch(pid[0], tid[0], msg, DEFAULT_MAX_FRAME_NUM, false);

    std::string processdumpCmd = "dumpcatcher -p " + std::to_string(pid[0]) + " -t " + std::to_string(tid[0]);
    system(processdumpCmd.c_str());

    std::string processdumpInvalidCmd = "dumpcatcher -" + std::to_string(invalidOption) + " -p " +
        std::to_string(pid[0]) + " -t " + std::to_string(tid[0]);
    system(processdumpInvalidCmd.c_str());
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
    OHOS::HiviewDFX::DumpStackTraceTest(data, size);
    return 0;
}
