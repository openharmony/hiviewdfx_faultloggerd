/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <thread>
#include <unistd.h>
#include <malloc.h>
#include <securec.h>
#include "dfx_ptrace.h"
#include "dfx_regs_get.h"
#include "unwinder.h"
#include "dfx_test_util.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxUnwinderPacTest"
#define LOG_DOMAIN 0xD002D11

class UnwinderPacTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: UnwinderPacTest001
 * @tc.desc: test unwinder unwind interface with pac
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderPacTest, UnwinderPacTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "UnwinderPacTest001: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderPacTest001: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        auto maps = DfxMaps::Create(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        context.maps = maps;
        unwRet = unwinder->Unwind(&context);
        EXPECT_EQ(true, unwRet) << "UnwinderPacTest001: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        EXPECT_GT(frames.size(), 1);
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        DfxPtrace::Detach(pid);
        CheckAndExit(HasFailure());
    }
    int status;
    wait(&status);
    ASSERT_EQ(status, 0);
    GTEST_LOG_(INFO) << "UnwinderPacTest001: end.";
}

/**
 * @tc.name: UnwinderPacTest002
 * @tc.desc: test unwinder FpStep interface with pac
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderPacTest, UnwinderPacTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderPacTest002: start.";
#if defined(__aarch64__)
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderPacTest002: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        unwRet = unwinder->UnwindByFp(&context);
        EXPECT_TRUE(unwRet) << "UnwinderPacTest002: unwRet:" << unwRet;
        DfxPtrace::Detach(pid);
        CheckAndExit(HasFailure());
    }
    int status;
    wait(&status);
    ASSERT_EQ(status, 0);
#endif
    GTEST_LOG_(INFO) << "UnwinderPacTest002: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS