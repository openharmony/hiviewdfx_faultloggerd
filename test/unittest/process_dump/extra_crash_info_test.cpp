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

#include <gtest/gtest.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_buffer_writer.h"
#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "decorative_dump_info.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ExtraCrashInfoTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
    static int WriteLogFunc(int32_t fd, const char *buf, size_t len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS
std::string ExtraCrashInfoTest::result = "";

void ExtraCrashInfoTest::SetUpTestCase(void)
{
    result = "";
}

void ExtraCrashInfoTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(ExtraCrashInfoTest::WriteLogFunc);
}

int ExtraCrashInfoTest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    ExtraCrashInfoTest::result.append(std::string(buf, len));
    return 0;
}

namespace {
/**
 * @tc.name: ExtraCrashInfoTest001
 * @tc.desc: test KeyThreadDumpInfo dumpcatch
 * @tc.type: FUNC
 */
HWTEST_F(ExtraCrashInfoTest, ExtraCrashInfoTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ExtraCrashInfoTest001: start.";
    uint8_t type = 5;
    constexpr size_t bufSize = 4096;
    uintptr_t memory[bufSize];
    for (size_t i = 0; i < bufSize; i++) {
        memory[i] = i;
    }
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    pid_t tid = pid;
    pid_t nsPid = pid;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_CPP_CRASH,
        .tid = pid,
        .pid = tid,
        .nsPid = nsPid,
    };
    const int moveBit = 56;
    request.crashObj = (static_cast<uintptr_t>(type) << moveBit) |
        (reinterpret_cast<uintptr_t>(memory) & 0x00ffffffffffffff);
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.InitKeyThread(request);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    ExtraCrashInfo extraCrashInfo;
    extraCrashInfo.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        StringPrintf("ExtraCrashInfo(Memory start address %016" PRIx64 "):",
        reinterpret_cast<uint64_t>(memory)),
        "0x000:", "0x020:", "0x040:", "0x060:", "0x080:",
        "0x0a0:", "0x0c0:", "0x0e0:", "0x100:", "0x120:",
    };
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    process.Detach();
    GTEST_LOG_(INFO) << "ExtraCrashInfoTest001: end.";
}
}
