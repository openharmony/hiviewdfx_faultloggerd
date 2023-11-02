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

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxUnwinderSeparateCodeTest"
#define LOG_DOMAIN 0xD002D11

class UnwinderSeparateCodeTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: UnwinderSeparateCodeTest001
 * @tc.desc: test unwinder unwind interface with separate code
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderSeparateCodeTest, UnwinderSeparateCodeTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderSeparateCodeTest001: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderSeparateCodeTest001: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        unwRet = unwinder->Unwind(&context);
        EXPECT_EQ(true, unwRet) << "UnwinderSeparateCodeTest001: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        ASSERT_GT(frames.size(), 1);
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    ASSERT_EQ(status, 0);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderSeparateCodeTest001: end.";
}

/**
 * @tc.name: UnwinderSeparateCodeTest002
 * @tc.desc: test unwinder FpStep interface with separate code
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderSeparateCodeTest, UnwinderSeparateCodeTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderSeparateCodeTest002: start.";
#if defined(__aarch64__)
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderSeparateCodeTest002: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        int idx = 0;
        uintptr_t pc, fp;
        while (true) {
            pc = regs->GetPc();
            fp = regs->GetFp();
            if (!unwinder->FpStep(fp, pc, &context) || (pc == 0)) {
                break;
            }
            idx++;
        };
        DfxPtrace::Detach(pid);
        ASSERT_GT(idx, 1);
        GTEST_LOG_(INFO) << "idx: " << idx;
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    ASSERT_EQ(status, 0);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
#endif
    GTEST_LOG_(INFO) << "UnwinderSeparateCodeTest002: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS