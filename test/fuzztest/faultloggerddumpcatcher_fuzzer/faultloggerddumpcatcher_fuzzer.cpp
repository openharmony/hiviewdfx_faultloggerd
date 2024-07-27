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
#include "dfx_dump_catcher.h"
#include "faultloggerd_client.h"
#include "fault_logger_daemon.h"
#include "faultloggerd_fuzzertest_common.h"

namespace OHOS {
namespace HiviewDFX {
const int FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH = 50;

void DumpStackTraceTest(const uint8_t* data, size_t size)
{
    int pid;
    int tid;
    int offsetTotalLength = sizeof(pid) + sizeof(tid) +
                            (2 * FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH); // 2 : Offset by 2 string length
    if (offsetTotalLength > size) {
        return;
    }

    STREAM_TO_VALUEINFO(data, pid);
    STREAM_TO_VALUEINFO(data, tid);

    std::string msg(reinterpret_cast<const char*>(data), FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH);
    data += FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH;
    std::string invalidOption(reinterpret_cast<const char*>(data), FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH);
    data += FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH;

    std::shared_ptr<DfxDumpCatcher> catcher = std::make_shared<DfxDumpCatcher>();
    catcher->DumpCatch(pid, tid, msg, DEFAULT_MAX_FRAME_NUM, false);

    std::string processdumpCmd = "dumpcatcher -p " + std::to_string(pid) + " -t " + std::to_string(tid);
    system(processdumpCmd.c_str());

    std::string processdumpInvalidCmd = "dumpcatcher -" + invalidOption + " -p " +
        std::to_string(pid) + " -t " + std::to_string(tid);
    system(processdumpInvalidCmd.c_str());
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
    OHOS::HiviewDFX::DumpStackTraceTest(data, size);
    return 0;
}
