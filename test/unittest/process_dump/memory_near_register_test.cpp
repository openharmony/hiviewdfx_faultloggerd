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
class MemoryNearRegisterTest : public testing::Test {
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

std::string MemoryNearRegisterTest::result = "";

void MemoryNearRegisterTest::SetUpTestCase(void)
{
    result = "";
}

void MemoryNearRegisterTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(MemoryNearRegisterTest::WriteLogFunc);
}

int MemoryNearRegisterTest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    MemoryNearRegisterTest::result.append(std::string(buf, len));
    return 0;
}
 
namespace {
/**
 * @tc.name: MemoryNearRegisterTest001
 * @tc.desc: test print memory near register
 * @tc.type: FUNC
 */
HWTEST_F(MemoryNearRegisterTest, MemoryNearRegisterTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MemoryNearRegisterTest001: start.";
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
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
    };
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    process.SetVmPid(pid);
    process.InitKeyThread(request);
    process.SetFaultThreadRegisters(DfxRegs::CreateRemoteRegs(pid)); // can not get context, so create regs self
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    MemoryNearRegister memoryNearRegister;
    memoryNearRegister.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Memory near registers:",
        "sp",
        "pc",
    };
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    std::string prevResult = result;
    result = "";
    CrashLogConfig crashLogConfig = {
        .extendPcLrPrinting = true,
    };
    process.SetCrashLogConfig(crashLogConfig);
    memoryNearRegister.Print(process, request, unwinder);
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    ASSERT_LT(prevResult.size(), result.size());
    process.Detach();
    GTEST_LOG_(INFO) << "MemoryNearRegisterTest001: end.";
}
}
 