/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include "dfx_fault_stack.h"
#include "dfx_frame_formatter.h"
#include "dfx_maps.h"
#include "dfx_regs.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_thread.h"
#include "dfx_unwind_remote.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class FaultStackUnittest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();

    static int WriteLogFunc(int32_t fd, const char *buf, int len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string FaultStackUnittest::result = "";

void FaultStackUnittest::SetUpTestCase(void)
{
    result = "";
}

void FaultStackUnittest::TearDownTestCase(void)
{
}

void FaultStackUnittest::SetUp(void)
{
    DfxRingBufferWrapper::GetInstance().SetWriteFunc(FaultStackUnittest::WriteLogFunc);
}

void FaultStackUnittest::TearDown(void)
{
}

int FaultStackUnittest::WriteLogFunc(int32_t fd, const char *buf, int len)
{
    printf("%d: %s", fd, buf);
    FaultStackUnittest::result.append(std::string(buf, len));
    return 0;
}

namespace {
/**
 * @tc.name: FaultStackUnittest001
 * @tc.desc: check whether fault stack and register can be print out
 * @tc.type: FUNC
 */
HWTEST_F(FaultStackUnittest, FaultStackUnittest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultStackUnittest001: start.";

    auto unwinder = std::make_shared<Unwinder>();
    bool unwRet = unwinder->UnwindLocal();
    if (!unwRet) {
        FAIL() << "Failed to unwind local";
    }
    auto frames = unwinder->GetFrames();
    int childPid = fork();
    bool isSuccess = childPid >= 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        printf("Failed to fork child process, errno(%d).\n", errno);
        return;
    }
    if (childPid == 0) {
        uint32_t left = 10;
        while (left > 0) {
            left = sleep(left);
        }
        _exit(0);
    }
    DfxThread thread(childPid, childPid, childPid);
    ASSERT_EQ(true, thread.Attach());
    auto maps = DfxMaps::Create(childPid);
    auto reg = DfxRegs::CreateRemoteRegs(childPid);
    std::unique_ptr<FaultStack> stack = std::make_unique<FaultStack>(childPid);
    stack->CollectStackInfo(frames);
    stack->CollectRegistersBlock(reg, maps);
    stack->Print();
    thread.Detach();

    if (result.find("Memory near registers") == std::string::npos) {
        FAIL();
    }

    if (result.find("FaultStack") == std::string::npos) {
        FAIL();
    }

    if (result.find("pc") == std::string::npos) {
        FAIL();
    }

    if (result.find("sp2:") == std::string::npos) {
        FAIL();
    }
    GTEST_LOG_(INFO) << "Result Log length:" << result.length();
    ASSERT_GT(result.length(), 0);
    GTEST_LOG_(INFO) << "FaultStackUnittest001: end.";
}
}