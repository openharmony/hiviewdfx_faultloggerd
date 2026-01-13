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
#include <cerrno>
#include <unistd.h>

#include "decorative_dump_info.h"
#include "dfx_frame_formatter.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_buffer_writer.h"
#include "dfx_test_util.h"
#include "dfx_thread.h"
#include "multithread_constructor.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class FaultStackUnittest : public testing::Test {
public:
    static void SetUpTestCase(void);
    void SetUp() override;
    static int WriteLogFunc(int32_t fd, const char *buf, size_t len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string FaultStackUnittest::result = "";

void FaultStackUnittest::SetUpTestCase(void)
{
    result = "";
}

void FaultStackUnittest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(FaultStackUnittest::WriteLogFunc);
}

int FaultStackUnittest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    FaultStackUnittest::result.append(std::string(buf, len));
    return 0;
}

namespace {
/**
 * @tc.name: FaultStackUnittest001
 * @tc.desc: check whether fault stack and register can be print out
 * @tc.type: FUNC
 */
HWTEST_F(FaultStackUnittest, FaultStackUnittest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "FaultStackUnittest001: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    bool isSuccess = testProcess >= 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        printf("Failed to fork child process, errno(%d).\n", errno);
    } else {
        sleep(1);
        pid_t tid = testProcess;
        pid_t nsPid = testProcess;
        pid_t pid = testProcess;
        ProcessDumpRequest request = {
            .type = ProcessDumpType::DUMP_TYPE_CPP_CRASH,
            .tid = tid,
            .pid = pid,
            .nsPid = pid,
        };
        DfxProcess process;
        process.InitProcessInfo(pid, nsPid, getuid(), "");
        process.InitKeyThread(request);
        Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
        unwinder.SetRegs(DfxRegs::CreateRemoteRegs(pid));
        unwinder.UnwindRemote(tid, true);
        process.GetKeyThread()->SetFrames(unwinder.GetFrames());
        FaultStack faultStack;
        faultStack.Collect(process, request, unwinder);
        faultStack.Print(process, request, unwinder);
        EXPECT_TRUE(result.find("FaultStack:") != std::string::npos) << result;
        EXPECT_TRUE(result.find("sp0") != std::string::npos) << result;
        GTEST_LOG_(INFO) << "FaultStackUnittest001: end.";
    }
}
}