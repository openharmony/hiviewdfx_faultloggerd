/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include "dfx_dump_catcher.h"

namespace OHOS {
namespace HiviewDFX {
const int FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH = 50;
void DumpStackTraceTest(const uint8_t* data, size_t size)
{
    struct TestArkFrameData {
        int pid;
        int tid;
        char option[FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH];
    };
    if (data == nullptr || size <= sizeof(TestArkFrameData)) {
        return;
    }
    const auto* testData = reinterpret_cast<const TestArkFrameData *>(data);
    std::string msg;

    std::shared_ptr<DfxDumpCatcher> catcher = std::make_shared<DfxDumpCatcher>();
    catcher->DumpCatch(testData->pid, testData->tid, msg);
    std::string processdumpCmd = "dumpcatcher -p " + std::to_string(testData->pid) + " -t " +
        std::to_string(testData->tid);
    system(processdumpCmd.c_str());
    std::string processdumpInvalidCmd = std::string("dumpcatcher -") +
        std::string(testData->option, FAULTLOGGER_FUZZTEST_MAX_STRING_LENGTH) + " -p " +
        std::to_string(testData->pid) + " -t " + std::to_string(testData->tid);
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
