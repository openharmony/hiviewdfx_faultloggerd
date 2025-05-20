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
class MapsTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
    static int WriteLogFunc(int32_t fd, const char *buf, int len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string MapsTest::result = "";

void MapsTest::SetUpTestCase(void)
{
    result = "";
}

void MapsTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(MapsTest::WriteLogFunc);
}

int MapsTest::WriteLogFunc(int32_t fd, const char *buf, int len)
{
    MapsTest::result.append(std::string(buf, len));
    return 0;
}
 
namespace {
/**
 * @tc.name: MapsTest001
 * @tc.desc: test print maps
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, MapsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MapsTest001: start.";
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
    auto regs = DfxRegs::CreateRemoteRegs(pid); // can not get context, so create regs self
    process.SetFaultThreadRegisters(regs);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    unwinder.SetRegs(regs);
    unwinder.UnwindRemote(tid, true);
    process.GetKeyThread()->SetFrames(unwinder.GetFrames());
    Maps maps;
    maps.Print(process, request, unwinder);
    std::vector<std::string> keyWords = {
        "Maps:",
        "test_processdump",
    };
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    std::string prevResult = result;
    result = "";
    CrashLogConfig crashLogConfig = {
        .simplifyVmaPrinting = true,
    };
    process.SetCrashLogConfig(crashLogConfig);
    maps.Print(process, request, unwinder);
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    ASSERT_GT(prevResult.size(), result.size());
    process.Detach();
    GTEST_LOG_(INFO) << "MapsTest001: end.";
}
}
 