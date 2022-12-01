/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "fault_stack_unittest.h"

#include <cerrno>

#include <libunwind_i-ohos.h>
#include <libunwind_i.h>
#include <map_info.h>
#include <unistd.h>

#include "dfx_fault_stack.h"
#include "dfx_frame.h"
#include "dfx_regs.h"
#include "dfx_maps.h"
#include "dfx_dump_catcher.h"
#include "dfx_ring_buffer_wrapper.h"
#include "dfx_thread.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;
std::string FaultStackUnittest::result {""};
int WriteLogFunc(int32_t fd, const char *buf, int len)
{
    printf("%d:%d:%s\n", fd, len, buf);
    FaultStackUnittest::result.append(std::string(buf, len));
    return 0;
}

void FaultStackUnittest::SetUpTestCase(void)
{
    result = "";
}

void FaultStackUnittest::TearDownTestCase(void)
{
}

void FaultStackUnittest::SetUp(void)
{
    DfxRingBufferWrapper::GetInstance().SetWriteFunc(WriteLogFunc);
}

void FaultStackUnittest::TearDown(void)
{
}
#if defined(__arm__)
std::vector<uintptr_t> GetCurrentRegs(unw_context_t ctx)
{
    std::vector<uintptr_t> regs {};
    auto sz = sizeof(ctx.regs) / sizeof(ctx.regs[0]);
    printf("ctx.regs:%zu\n", sz);
    for (size_t i = 0; i < sz; i++) {
        regs.push_back(ctx.regs[i]);
    }
    return regs;
}
#elif defined(__aarch64__)
std::vector<uintptr_t> GetCurrentRegs(unw_context_t ctx)
{
    std::vector<uintptr_t> regs {};
    auto sz = sizeof(ctx.uc_mcontext.regs) / sizeof(ctx.uc_mcontext.regs[0]);
    printf("uc_mcontext.regs:%zu\n", sz);
    for (size_t i = 0; i < sz; i++) {
        regs.push_back(ctx.uc_mcontext.regs[i]);
    }
    regs.push_back(ctx.uc_mcontext.sp);
    regs.push_back(ctx.uc_mcontext.pc);
    return regs;
}
#endif
std::shared_ptr<DfxRegs> GetCurrentReg()
{
    printf("GetCurrentReg begin\n");
    unw_context_t ctx;
    printf("GetCurrentReg ctx:%p\n", &ctx);
    unw_getcontext(&ctx);
#if defined(__arm__)
    std::shared_ptr<DfxRegsArm> reg = std::make_shared<DfxRegsArm>();
    std::vector<uintptr_t> regVec = GetCurrentRegs(ctx);
    reg->SetRegs(regVec);
#elif defined(__aarch64__)
    std::shared_ptr<DfxRegsArm64> reg = std::make_shared<DfxRegsArm64>();
    std::vector<uintptr_t> regVec = GetCurrentRegs(ctx);
    reg->SetRegs(regVec);
#else
    std::shared_ptr<DfxRegs> reg = nullptr;
#endif
    printf("GetCurrentReg end\n");
    return reg;
}

/**
 * @tc.name: FaultStackUnittest001
 * @tc.desc: check whether fault stack and register can be print out
 * @tc.type: FUNC
 */
HWTEST_F(FaultStackUnittest, FaultStackUnittest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultStackUnittest001: start.";
    std::vector<std::shared_ptr<DfxFrame>> frames;
    DfxDumpCatcher catcher(getpid());
    ASSERT_EQ(true, catcher.InitFrameCatcher());
    ASSERT_EQ(true, catcher.RequestCatchFrame(getpid()));
    ASSERT_EQ(true, catcher.CatchFrame(getpid(), frames));
    ASSERT_GT(frames.size(), 0);
    GTEST_LOG_(INFO) << "frame size:" << frames.size();
    int childPid = fork();
    if (childPid == 0) {
        uint32_t left = 10;
        while (left > 0) {
            left = sleep(left);
        }
        _exit(0);
    } else if (childPid < 0) {
        printf("Failed to fork child process, errno(%d).\n", errno);
        return;
    }

    DfxThread thread(childPid, childPid, childPid);
    ASSERT_EQ(true, thread.Attach());
    auto maps = DfxElfMaps::Create(childPid);
    auto reg = GetCurrentReg();
    std::unique_ptr<FaultStack> stack = std::make_unique<FaultStack>(childPid);
    stack->CollectStackInfo(frames);
    stack->CollectRegistersBlock(reg, maps);
    stack->Print();
    catcher.DestroyFrameCatcher();
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
