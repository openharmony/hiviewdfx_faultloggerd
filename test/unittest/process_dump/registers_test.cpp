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
class RegistersTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    void SetUp() override;
    static int WriteLogFunc(int32_t fd, const char *buf, size_t len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string RegistersTest::result = "";

void RegistersTest::SetUpTestCase(void)
{
    result = "";
}

void RegistersTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(RegistersTest::WriteLogFunc);
}

int RegistersTest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    RegistersTest::result.append(std::string(buf, len));
    return 0;
}

namespace {
/**
 * @tc.name: RegistersTest001
 * @tc.desc: test print register
 * @tc.type: FUNC
 */
HWTEST_F(RegistersTest, RegistersTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RegistersTest001: start.";
    Registers registers;
    pid_t pid = getpid();
    pid_t tid = gettid();
    pid_t nsPid = getpid();
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_CPP_CRASH,
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
    };
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), "");
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    registers.Collect(process, request, unwinder);
    registers.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Registers:",
        "pc",
        "lr",
#ifdef __aarch64__
        "pstate",
        "esr",
#elif __arm__
        "cpsr",
#endif
    };
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    GTEST_LOG_(INFO) << "RegistersTest001: end.";
}
}
