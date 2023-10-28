/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <map>
#include <thread>
#include <unistd.h>
#include <hilog/log.h>
#include <malloc.h>
#include <securec.h>
#include "dfx_ptrace.h"
#include "dfx_regs_get.h"
#include "unwinder.h"
#include "elapsed_time.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxUnwinderTest"
#define LOG_DOMAIN 0xD002D11

class UnwinderTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}

    std::map<int, std::shared_ptr<Unwinder>> unwinders_;
};

/**
 * @tc.name: UnwinderLocalTest001
 * @tc.desc: test unwinder local unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderLocalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderLocalTest001: start.";
    pid_t child = fork();
    if (child == 0) {
        auto unwinder = std::make_shared<Unwinder>();
        ElapsedTime counter;
        MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
        time_t elapsed1 = counter.Elapsed();
        EXPECT_EQ(true, unwRet) << "UnwinderLocalTest001: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        ASSERT_GT(frames.size(), 0);
        time_t elapsed2 = counter.Elapsed();
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderLocalTest001: end.";
}

/**
 * @tc.name: UnwinderLocalTest002
 * @tc.desc: test unwinder local unwind n counts
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderLocalTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderLocalTest002: start.";
    pid_t child = fork();
    if (child == 0) {
        unwinders_.clear();
        std::shared_ptr<Unwinder> unwinder = nullptr;
        pid_t pid = getpid();
        GTEST_LOG_(INFO) << "pid: " << pid;
        for (int i = 0; i < 10; ++i) {
            auto it = unwinders_.find(pid);
            if (it != unwinders_.end()) {
                unwinder = it->second;
            } else {
                unwinder = std::make_shared<Unwinder>();
            }
            ElapsedTime counter;
            MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
            time_t elapsed1 = counter.Elapsed();
            EXPECT_EQ(true, unwRet) << "UnwinderLocalTest002: Unwind:" << unwRet;
            auto frames = unwinder->GetFrames();
            ASSERT_GT(frames.size(), 0);
            time_t elapsed2 = counter.Elapsed();
            GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
            GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
            GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
            unwinders_[pid] = unwinder;
            sleep(1);
        }
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderLocalTest002: end.";
}

/**
 * @tc.name: UnwinderRemoteTest001
 * @tc.desc: test unwinder remote unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderRemoteTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderRemoteTest001: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest001: Attach:" << unwRet;
        ElapsedTime counter;
        unwRet = unwinder->UnwindRemote();
        time_t elapsed1 = counter.Elapsed();
        EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest001: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        ASSERT_GT(frames.size(), 0);
        time_t elapsed2 = counter.Elapsed();
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderRemoteTest001: end.";
}

/**
 * @tc.name: UnwinderRemoteTest002
 * @tc.desc: test unwinder remote unwind n counts
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderRemoteTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderRemoteTest002: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        unwinders_.clear();
        std::shared_ptr<Unwinder> unwinder = nullptr;
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest002: Attach:" << unwRet;
        for (int i = 0; i < 10; ++i) {
            auto it = unwinders_.find(pid);
            if (it != unwinders_.end()) {
                unwinder = it->second;
            } else {
                unwinder = std::make_shared<Unwinder>(pid);
            }
            ElapsedTime counter;
            unwRet = unwinder->UnwindRemote();
            time_t elapsed1 = counter.Elapsed();
            EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest002: Unwind:" << unwRet;
            auto frames = unwinder->GetFrames();
            ASSERT_GT(frames.size(), 0);
            time_t elapsed2 = counter.Elapsed();
            GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
            GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
            GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
            unwinders_[pid] = unwinder;
            sleep(1);
        }
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderRemoteTest002: end.";
}

/**
 * @tc.name: UnwindTest001
 * @tc.desc: test unwinder unwind interface in remote case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindTest001: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "UnwindTest001: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        ElapsedTime counter;
        unwRet = unwinder->Unwind(&context);
        time_t elapsed1 = counter.Elapsed();
        EXPECT_EQ(true, unwRet) << "UnwindTest001: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        ASSERT_GT(frames.size(), 0);
        time_t elapsed2 = counter.Elapsed();
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwindTest001: end.";
}

/**
 * @tc.name: UnwindTest002
 * @tc.desc: test unwinder unwind interface in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindTest002: start.";
    pid_t child = fork();
    if (child == 0) {
        auto unwinder = std::make_shared<Unwinder>();
        ElapsedTime counter;
        uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
        ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));

        auto regs = DfxRegs::Create();
        auto regsData = regs->RawData();
        GetLocalRegs(regsData);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = UNWIND_TYPE_LOCAL;
        context.regs = regs;
        context.stackCheck = false;
        context.stackBottom = stackBottom;
        context.stackTop = stackTop;
        time_t elapsed1 = counter.Elapsed();
        bool unwRet = unwinder->Unwind(&context);
        time_t elapsed2 = counter.Elapsed();
        EXPECT_EQ(true, unwRet) << "UnwindTest002: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        ASSERT_GT(frames.size(), 0);
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwindTest002: end.";
}

/**
 * @tc.name: UnwindTest003
 * @tc.desc: test GetPcs GetLastErrorCode GetLastErrorAddr GetFramesByPcs functions
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindTest003: start.";
    pid_t child = fork();
    if (child == 0) {
        auto unwinder = std::make_shared<Unwinder>();
        MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
        EXPECT_EQ(true, unwRet) << "UnwindTest003: Unwind ret:" << unwRet;
        auto pcs = unwinder->GetPcs();
        ASSERT_GT(pcs.size(), 0);
        GTEST_LOG_(INFO) << "pcs.size() > 0\n";

        uint16_t errorCode = unwinder->GetLastErrorCode();
        uint64_t errorAddr = unwinder->GetLastErrorAddr();
        GTEST_LOG_(INFO) << "errorCode:" << errorCode;
        GTEST_LOG_(INFO) << "errorAddr:" << errorAddr;

        std::vector<DfxFrame> frames;
        std::shared_ptr<DfxMaps> maps = unwinder->GetMaps();
        unwinder->GetFramesByPcs(frames, pcs, maps);
        ASSERT_GT(frames.size(), 0);
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwindTest003: end.";
}

/**
 * @tc.name: StepTest001
 * @tc.desc: test unwinder Step interface in remote case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest001: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "StepTest001: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        std::shared_ptr<DfxMaps> maps = DfxMaps::Create(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        ElapsedTime counter;
        uintptr_t pc, sp;
        pc = regs->GetPc();
        sp = regs->GetSp();
        std::shared_ptr<DfxMap> map = nullptr;
        ASSERT_TRUE(maps->FindMapByAddr(map, pc));
        context.map = map;
        time_t elapsed1 = counter.Elapsed();
        unwRet = unwinder->Step(pc, sp, &context);
        time_t elapsed2 = counter.Elapsed();
        ASSERT_TRUE(unwRet) << "StepTest001: Unwind:" << unwRet;
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "StepTest001: end.";
}

/**
 * @tc.name: StepTest002
 * @tc.desc: test unwinder Step interface in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest002: start.";
    pid_t child = fork();
    if (child == 0) {
        auto unwinder = std::make_shared<Unwinder>();
        ElapsedTime counter;
        uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
        ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));

        auto regs = DfxRegs::Create();
        auto regsData = regs->RawData();
        GetLocalRegs(regsData);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = UNWIND_TYPE_LOCAL;
        context.regs = regs;
        context.stackCheck = false;
        context.stackBottom = stackBottom;
        context.stackTop = stackTop;
        uintptr_t pc, sp;
        pc = regs->GetPc();
        sp = regs->GetSp();
        time_t elapsed1 = counter.Elapsed();
        bool unwRet = unwinder->Step(pc, sp, &context);
        time_t elapsed2 = counter.Elapsed();
        ASSERT_TRUE(unwRet) << "StepTest002: Unwind:" << unwRet;
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "StepTest002: end.";
}

#if defined(__aarch64__)
/**
 * @tc.name: StepTest003
 * @tc.desc: test unwinder FpStep interface in remote case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest003: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "StepTest003: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        std::shared_ptr<DfxMaps> maps = DfxMaps::Create(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        ElapsedTime counter;
        uintptr_t pc, fp;
        pc = regs->GetPc();
        fp = regs->GetFp();
        time_t elapsed1 = counter.Elapsed();
        unwRet = unwinder->FpStep(fp, pc, &context);
        time_t elapsed2 = counter.Elapsed();
        ASSERT_TRUE(unwRet) << "StepTest003: Unwind:" << unwRet;
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        DfxPtrace::Detach(pid);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "StepTest003: end.";
}

/**
 * @tc.name: StepTest004
 * @tc.desc: test unwinder FpStep interface in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest004: start.";
    pid_t child = fork();
    if (child == 0) {
        auto unwinder = std::make_shared<Unwinder>();
        ElapsedTime counter;
        uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
        ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));

        auto regs = DfxRegs::Create();
        auto regsData = regs->RawData();
        GetLocalRegs(regsData);
        UnwindContext context;
        context.pid = UNWIND_TYPE_LOCAL;
        context.regs = regs;
        context.stackCheck = false;
        context.stackBottom = stackBottom;
        context.stackTop = stackTop;
        unwinder->SetRegs(regs);
        uintptr_t pc, fp;
        pc = regs->GetPc();
        fp = regs->GetFp();
        time_t elapsed1 = counter.Elapsed();
        bool unwRet = unwinder->FpStep(fp, pc, &context);
        time_t elapsed2 = counter.Elapsed();
        ASSERT_TRUE(unwRet) << "StepTest004: Unwind:" << unwRet;
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1;
        GTEST_LOG_(INFO) << "Elapsed+: " << elapsed2;
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "StepTest004: end.";
}
#endif
} // namespace HiviewDFX
} // namepsace OHOS