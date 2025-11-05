/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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
#include <malloc.h>
#include <map>
#include <securec.h>
#include <thread>
#include <unistd.h>

#include "dfx_frame_formatter.h"
#include "dfx_ptrace.h"
#include "dfx_regs_get.h"
#include "dfx_test_util.h"
#include "elapsed_time.h"
#include "unwinder.h"

#if defined(__x86_64__)
#include <unwind.h> // GCC's internal unwinder, part of libgcc
#endif

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxUnwinderTest"
#define LOG_DOMAIN 0xD002D11
#define TIME_SLEEP 3

class UnwinderTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}

    std::map<int, std::shared_ptr<Unwinder>> unwinders_;
    const size_t skipFrameNum = 2;
};

/**
 * @tc.name: GetStackRangeTest001
 * @tc.desc: test unwinder GetStackRange interface in pid == tid
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, GetStackRangeTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetStackRangeTest001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    uintptr_t stackBottom = 1;
    uintptr_t stackTop = static_cast<uintptr_t>(-1);
    GTEST_LOG_(INFO) << "when pid == tid and maps_ != null, GetStackRange(stackBottom, stackTop) is true";
    ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));
    // When the param is less than -1, maps_ = null when method Unwinder is constructed
    auto unwinderNegative = std::make_shared<Unwinder>(-2);
    GTEST_LOG_(INFO) << "when pid == tid and maps_ == null, GetStackRange(stackBottom, stackTop) is false";
    ASSERT_TRUE(unwinderNegative->GetStackRange(stackBottom, stackTop));
    GTEST_LOG_(INFO) << "GetStackRangeTest001: end.";
}

/**
 * @tc.name: GetStackRangeTest002
 * @tc.desc: test unwinder GetStackRange interface in pid != tid
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, GetStackRangeTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetStackRangeTest002: start.";
    auto unwinder = std::make_shared<Unwinder>();
    uintptr_t stackBottom = 1;
    uintptr_t stackTop = static_cast<uintptr_t>(-1);
    bool result = false;
    GTEST_LOG_(INFO) << "Run the function with thread will get pid != tid, "
                        "GetStackRange(stackBottom, stackTop) is true";
    std::thread th([unwinder, &stackBottom, &stackTop, &result] {
        result = unwinder->GetStackRange(stackBottom, stackTop);
    });
    if (th.joinable()) {
        th.join();
    }
    ASSERT_TRUE(result);
    GTEST_LOG_(INFO) << "GetStackRangeTest002: end.";
}

/**
 * @tc.name: UnwinderLocalTest001
 * @tc.desc: test unwinder local unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderLocalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderLocalTest001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    ElapsedTime counter;
    MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
    time_t elapsed1 = counter.Elapsed();
    EXPECT_EQ(true, unwRet) << "UnwinderLocalTest001: Unwind:" << unwRet;
    auto frames = unwinder->GetFrames();
    ASSERT_GT(frames.size(), 1);
    time_t elapsed2 = counter.Elapsed();
    GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1 << "\tElapsed+: " << elapsed2;
    GTEST_LOG_(INFO) << "UnwinderLocalTest001: frames:\n" << Unwinder::GetFramesStr(frames);
    unwRet = unwinder->UnwindLocal(false, false, DEFAULT_MAX_FRAME_NUM, skipFrameNum);
    EXPECT_EQ(true, unwRet) << "UnwinderLocalTest001: Unwind:" << unwRet;
    auto frames2 = unwinder->GetFrames();
    ASSERT_GT(frames.size(), frames2.size());
    GTEST_LOG_(INFO) << "UnwinderLocalTest001: frames2:\n" << Unwinder::GetFramesStr(frames2);
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
        ASSERT_GT(frames.size(), 1);
        time_t elapsed2 = counter.Elapsed();
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1 << "\tElapsed+: " << elapsed2;
        GTEST_LOG_(INFO) << "UnwinderLocalTest002: frames:\n" << Unwinder::GetFramesStr(frames);
        unwinders_[pid] = unwinder;
        sleep(1);
    };
    GTEST_LOG_(INFO) << "UnwinderLocalTest002: end.";
}

/**
 * @tc.name: UnwinderLocalTest003
 * @tc.desc: test unwinder UnwindLocal interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderLocalTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderLocalTest003: start.";
    // When the param is less than -1, maps_ = null when method Unwinder is constructed
    auto unwinderNegative = std::make_shared<Unwinder>(-2);
    GTEST_LOG_(INFO) << "when pid == tid and maps_ == null, "
                         "UnwindLocal(maxFrameNum, skipFrameNum) is false";
    ASSERT_FALSE(unwinderNegative->UnwindLocal());
    auto unwinder = std::make_shared<Unwinder>();
    GTEST_LOG_(INFO) << "when pid == tid and maps_ != null, "
                        "UnwindLocal(maxFrameNum, skipFrameNum) is true";
    ASSERT_TRUE(unwinder->UnwindLocal());
    GTEST_LOG_(INFO) << "UnwinderLocalTest003: end.";
}

/**
 * @tc.name: UnwinderRemoteTest001
 * @tc.desc: test unwinder remote unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderRemoteTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderRemoteTest001: start.";
    pid_t child = fork();
    if (child == 0) {
        sleep(TIME_SLEEP);
        _exit(0);
    }

    GTEST_LOG_(INFO) << "pid: " << child << ", ppid:" << getpid();
    auto unwinder = std::make_shared<Unwinder>(child);
    bool unwRet = DfxPtrace::Attach(child);
    EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest001: Attach:" << unwRet;
    ElapsedTime counter;
    unwRet = unwinder->UnwindRemote(child);
    time_t elapsed1 = counter.Elapsed();
    EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest001: unwRet:" << unwRet;
    auto frames = unwinder->GetFrames();
    ASSERT_GT(frames.size(), 1);
    time_t elapsed2 = counter.Elapsed();
    GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1 << "\tElapsed+: " << elapsed2;
    GTEST_LOG_(INFO) << "UnwinderRemoteTest001: frames:\n" << Unwinder::GetFramesStr(frames);
    unwRet = unwinder->UnwindRemote(child, false, DEFAULT_MAX_FRAME_NUM, skipFrameNum);
    EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest001: unwRet:" << unwRet;
    auto frames2 = unwinder->GetFrames();
    ASSERT_GT(frames.size(), frames2.size());
    GTEST_LOG_(INFO) << "UnwinderRemoteTest001: frames2:\n" << Unwinder::GetFramesStr(frames2);
    DfxPtrace::Detach(child);
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
    pid_t child = fork();
    if (child == 0) {
        sleep(TIME_SLEEP);
        _exit(0);
    }

    GTEST_LOG_(INFO) << "pid: " << child << ", ppid:" << getpid();
    unwinders_.clear();
    std::shared_ptr<Unwinder> unwinder = nullptr;
    bool unwRet = DfxPtrace::Attach(child);
    EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest002: Attach:" << unwRet;
    for (int i = 0; i < 10; ++i) {
        auto it = unwinders_.find(child);
        if (it != unwinders_.end()) {
            unwinder = it->second;
        } else {
            unwinder = std::make_shared<Unwinder>(child);
        }
        ElapsedTime counter;
        unwRet = unwinder->UnwindRemote(child);
        time_t elapsed1 = counter.Elapsed();
        EXPECT_EQ(true, unwRet) << "UnwinderRemoteTest002: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        ASSERT_GT(frames.size(), 1);
        time_t elapsed2 = counter.Elapsed();
        GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1 << "\tElapsed+: " << elapsed2;
        GTEST_LOG_(INFO) << "UnwinderRemoteTest002: frames:\n" << Unwinder::GetFramesStr(frames);
        unwinders_[child] = unwinder;
        sleep(1);
    }
    DfxPtrace::Detach(child);
    GTEST_LOG_(INFO) << "UnwinderRemoteTest002: end.";
}

/**
 * @tc.name: UnwinderRemoteTest003
 * @tc.desc: test unwinder UnwindRemote interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderRemoteTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderRemoteTest003: start.";
    // When the param is less than -1, pid_ < 0 when method Unwinder is constructed
    auto unwinderNegative = std::make_shared<Unwinder>(-2);
    size_t maxFrameNum = 64;
    size_t skipFrameNum = 0;
    GTEST_LOG_(INFO) << "when pid <= 0, UnwindRemote(maxFrameNum, skipFrameNum) is false";
    ASSERT_FALSE(unwinderNegative->UnwindRemote(-2, maxFrameNum, skipFrameNum));
    GTEST_LOG_(INFO) << "UnwinderRemoteTest003: end.";
}

/**
 * @tc.name: UnwindTest001
 * @tc.desc: test unwinder unwind interface in remote case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindTest001: start.";
    pid_t child = fork();
    if (child == 0) {
        sleep(TIME_SLEEP);
        _exit(0);
    }

    GTEST_LOG_(INFO) << "pid: " << child << ", ppid:" << getpid();
    auto unwinder = std::make_shared<Unwinder>(child);
    bool unwRet = DfxPtrace::Attach(child);
    EXPECT_EQ(true, unwRet) << "UnwindTest001: Attach:" << unwRet;
    auto regs = DfxRegs::CreateRemoteRegs(child);
    unwinder->SetRegs(regs);
    auto maps = DfxMaps::Create(child);
    UnwindContext context;
    context.pid = child;
    context.regs = regs;
    context.maps = maps;
    ElapsedTime counter;
    unwRet = unwinder->Unwind(&context);
    time_t elapsed1 = counter.Elapsed();
    EXPECT_EQ(true, unwRet) << "UnwindTest001: Unwind:" << unwRet;
    auto frames = unwinder->GetFrames();
    ASSERT_GT(frames.size(), 1);
    time_t elapsed2 = counter.Elapsed();
    GTEST_LOG_(INFO) << "Elapsed-: " << elapsed1 << "\tElapsed+: " << elapsed2;
    GTEST_LOG_(INFO) << "UnwindTest001: frames:\n" << Unwinder::GetFramesStr(frames);
    DfxPtrace::Detach(child);
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
    auto unwinder = std::make_shared<Unwinder>();
    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));
    GTEST_LOG_(INFO) << "UnwindTest002: GetStackRange.";
    UnwindContext context;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;

    auto regs = DfxRegs::Create();
    auto regsData = regs->RawData();
    GetLocalRegs(regsData);
    unwinder->SetRegs(regs);
    auto maps = DfxMaps::Create(getpid());
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs;
    context.maps = maps;
    bool unwRet = unwinder->Unwind(&context);
    EXPECT_EQ(true, unwRet) << "UnwindTest002: unwRet:" << unwRet;
    auto frames = unwinder->GetFrames();
    ASSERT_GT(frames.size(), 1);
    GTEST_LOG_(INFO) << "UnwindTest002:frames:\n" << Unwinder::GetFramesStr(frames);
    GTEST_LOG_(INFO) << "UnwindTest002: end.";
}

/**
 * @tc.name: UnwindTest003
 * @tc.desc: test GetLastErrorCode GetLastErrorAddr functions
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindTest003: start.";

    auto unwinder = std::make_shared<Unwinder>();
    unwinder->IgnoreMixstack(true);
    MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
    EXPECT_EQ(true, unwRet) << "UnwindTest003: Unwind ret:" << unwRet;
    unwinder->EnableFillFrames(false);
    const auto& frames = unwinder->GetFrames();
    ASSERT_GT(frames.size(), 1) << "frames.size() error";

    uint16_t errorCode = unwinder->GetLastErrorCode();
    uint64_t errorAddr = unwinder->GetLastErrorAddr();
    GTEST_LOG_(INFO) << "errorCode:" << errorCode;
    GTEST_LOG_(INFO) << "errorAddr:" << errorAddr;
    GTEST_LOG_(INFO) << "UnwindTest003: end.";
}

/**
 * @tc.name: UnwindTest004
 * @tc.desc: test unwinder local unwind for
 * GetFramesStr(const std::vector<std::shared_ptr<DfxFrame>>& frames)
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindTest004: start.";

    auto unwinder = std::make_shared<Unwinder>();
    ElapsedTime counter;
    MAYBE_UNUSED bool unwRet = unwinder->UnwindLocal();
    ASSERT_EQ(true, unwRet) << "UnwindTest004: Unwind:" << unwRet;
    auto frames = unwinder->GetFrames();
    ASSERT_GT(frames.size(), 1);

    std::string framesStr = DfxFrameFormatter::GetFramesStr(frames);
    GTEST_LOG_(INFO) << "UnwindTest004: frames:\n" << framesStr;

    string log[] = {"pc", "test_unwind", "#00", "#01", "#02"};
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(framesStr, log, len);
    ASSERT_EQ(count, len) << "UnwindTest004 Failed";
    GTEST_LOG_(INFO) << "UnwindTest004: end.";
}

/**
 * @tc.name: StepTest001
 * @tc.desc: test unwinder Step interface in remote case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest001: start.";
    pid_t child = fork();
    if (child == 0) {
        sleep(TIME_SLEEP);
        _exit(0);
    }

    GTEST_LOG_(INFO) << "pid: " << child << ", ppid:" << getpid();
    auto unwinder = std::make_shared<Unwinder>(child);
    bool unwRet = DfxPtrace::Attach(child);
    EXPECT_EQ(true, unwRet) << "StepTest001: Attach:" << unwRet;
    auto regs = DfxRegs::CreateRemoteRegs(child);
    auto maps = DfxMaps::Create(child);
    unwinder->SetRegs(regs);
    UnwindContext context;
    context.pid = child;
    context.regs = regs;
    context.maps = maps;

    uintptr_t pc, sp;
    pc = regs->GetPc();
    sp = regs->GetSp();
    std::shared_ptr<DfxMap> map = nullptr;
    ASSERT_TRUE(maps->FindMapByAddr(pc, map));
    context.map = map;
    unwRet = unwinder->Step(pc, sp, &context);
    ASSERT_TRUE(unwRet) << "StepTest001: Unwind:" << unwRet;
    DfxPtrace::Detach(child);
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
    auto unwinder = std::make_shared<Unwinder>();
    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));
    GTEST_LOG_(INFO) << "StepTest002: GetStackRange.";
    auto maps = DfxMaps::Create(getpid());
    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;

    auto regs = DfxRegs::Create();
    auto regsData = regs->RawData();
    GetLocalRegs(regsData);
    unwinder->SetRegs(regs);
    context.regs = regs;
    context.maps = maps;

    uintptr_t pc, sp;
    pc = regs->GetPc();
    sp = regs->GetSp();
    bool unwRet = unwinder->Step(pc, sp, &context);
    ASSERT_TRUE(unwRet) << "StepTest002: unwRet:" << unwRet;
    GTEST_LOG_(INFO) << "StepTest002: end.";
}

#if defined(__aarch64__) || defined(__loongarch_lp64)
/**
 * @tc.name: StepTest003
 * @tc.desc: test unwinder UnwindByFp interface in remote case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest003: start.";
    pid_t child = fork();
    if (child == 0) {
        sleep(TIME_SLEEP);
        _exit(0);
    }

    GTEST_LOG_(INFO) << "pid: " << child << ", ppid:" << getpid();
    auto unwinder = std::make_shared<Unwinder>(child);
    bool unwRet = DfxPtrace::Attach(child);
    EXPECT_EQ(true, unwRet) << "StepTest003: Attach:" << unwRet;
    auto regs = DfxRegs::CreateRemoteRegs(child);
    unwinder->SetRegs(regs);
    UnwindContext context;
    context.pid = child;
    ElapsedTime counter;
    unwRet = unwinder->UnwindByFp(&context);
    ASSERT_TRUE(unwRet) << "StepTest003: unwind:" << unwRet;
    DfxPtrace::Detach(child);
    time_t elapsed = counter.Elapsed();
    GTEST_LOG_(INFO) << "StepTest003: Elapsed: " << elapsed;
    auto pcs = unwinder->GetPcs();
    std::vector<DfxFrame> frames;
    unwinder->GetFramesByPcs(frames, pcs);
    ASSERT_GT(frames.size(), 1);
    GTEST_LOG_(INFO) << "StepTest003: frames:\n" << Unwinder::GetFramesStr(frames);
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
    auto unwinder = std::make_shared<Unwinder>();
    ElapsedTime counter;
    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));
    GTEST_LOG_(INFO) << "StepTest004: GetStackRange.";

    auto regs = DfxRegs::Create();
    auto regsData = regs->RawData();
    GetLocalRegs(regsData);
    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
    unwinder->SetRegs(regs);

    bool unwRet = unwinder->UnwindByFp(&context);
    ASSERT_TRUE(unwRet) << "StepTest004: unwRet:" << unwRet;
    auto unwSize = unwinder->GetPcs().size();
    ASSERT_GT(unwSize, 1) << "pcs.size() error";

    uintptr_t miniRegs[FP_MINI_REGS_SIZE] = {0};
    GetFramePointerMiniRegs(miniRegs, sizeof(miniRegs) / sizeof(miniRegs[0]));
    regs = DfxRegs::CreateFromRegs(UnwindMode::FRAMEPOINTER_UNWIND, miniRegs, sizeof(miniRegs) / sizeof(miniRegs[0]));
    unwinder->SetRegs(regs);
    size_t idx = 0;
    uintptr_t pc, fp;
    while (true) {
        pc = regs->GetPc();
        fp = regs->GetFp();
        idx++;
        if (!unwinder->FpStep(fp, pc, &context) || (pc == 0)) {
            break;
        }
    };
    ASSERT_EQ(idx, unwSize) << "StepTest004: idx:" << idx;
    time_t elapsed = counter.Elapsed();
    GTEST_LOG_(INFO) << "StepTest004: Elapsed: " << elapsed;
    GTEST_LOG_(INFO) << "StepTest004: end.";
}
#endif

#if defined(__arm__) || defined(__aarch64__)
/**
 * @tc.name: StepTest005
 * @tc.desc: test unwinder Step interface in lr callback with apply failed case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest005: start.";
    auto unwinder = std::make_shared<Unwinder>();
    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));
    GTEST_LOG_(INFO) << "StepTest005: GetStackRange.";

    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;

    auto regs = DfxRegs::Create();
    auto regsData = regs->RawData();
    GetLocalRegs(regsData);
    unwinder->SetRegs(regs);
    context.regs = regs;
    context.maps = unwinder->GetMaps();

    uintptr_t lr = *(regs->GetReg(REG_LR));
    uintptr_t pc = regs->GetPc();
    uintptr_t failSp = stackTop + 1; // arm cfa get from sp
    regs->SetSp(failSp);
    uintptr_t failFp = stackTop + 1; // arm64 cfa get from fp
    regs->SetFp(failFp);
    bool unwRet = unwinder->Step(pc, failSp, &context);
    ASSERT_TRUE(unwRet) << "StepTest005: unwRet:" << unwRet;
    ASSERT_EQ(lr, pc) << "StepTest005: lr callback";
    GTEST_LOG_(INFO) << "StepTest005: end.";
}

/**
 * @tc.name: StepTest006
 * @tc.desc: test unwinder Step interface in lr callback with step failed case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, StepTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StepTest006: start.";
    auto unwinder = std::make_shared<Unwinder>();
    uintptr_t stackBottom = 1, stackTop = static_cast<uintptr_t>(-1);
    ASSERT_TRUE(unwinder->GetStackRange(stackBottom, stackTop));
    GTEST_LOG_(INFO) << "StepTest006: GetStackRange.";

    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.stackCheck = true;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;

    auto regs = DfxRegs::Create();
    auto regsData = regs->RawData();
    GetLocalRegs(regsData);
    unwinder->SetRegs(regs);
    context.regs = regs;
    context.maps = unwinder->GetMaps();

    uintptr_t lr = *(regs->GetReg(REG_LR));
    uintptr_t sp = regs->GetSp();
    uintptr_t failPc = stackTop + 1;
    regs->SetPc(failPc);
    bool unwRet = unwinder->Step(failPc, sp, &context);
    ASSERT_TRUE(unwRet) << "StepTest006: unwRet:" << unwRet;
    ASSERT_EQ(lr, failPc) << "StepTest006: lr callback";

    GTEST_LOG_(INFO) << "StepTest006: end.";
}
#endif

/**
 * @tc.name: FillFrameTest001
 * @tc.desc: test unwinder FillFrame interface
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, FillFrameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FillFrameTest001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    DfxFrame frame;
    unwinder->FillFrame(frame);
    GTEST_LOG_(INFO) << " when DfxFrame::map is null, frame.buildId.size() is 0";
    ASSERT_EQ(frame.buildId.size(), 0);
    string testMap = "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context.z.so.noexit";
    auto map = DfxMap::Create(testMap);
    frame.map = map;
    unwinder->FillFrame(frame);
    GTEST_LOG_(INFO) << " when DfxFrame::map is not null and file not exist, frame.buildId.size() is 0";
    ASSERT_EQ(frame.buildId.size(), 0);
#ifdef __arm__
    testMap = "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context.z.so";
#else
    testMap = "7f0ab40000-7f0ab41000 r--p 00000000 b3:07 1882 /system/lib64/init/libinit_context.z.so";
#endif
    map = DfxMap::Create(testMap);
    frame.map = map;
    unwinder->FillFrame(frame);
    GTEST_LOG_(INFO) << " when DfxFrame::map is not null and file exist, frame.buildId.size() is bigger than 0";
    ASSERT_EQ(frame.buildId.size() == 0, false);
    GTEST_LOG_(INFO) << "FillFrameTest001: end.";
}

/**
 * @tc.name: FillJsFrameTest001
 * @tc.desc: test unwinder FillJsFrame interface
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, FillJsFrameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FillJsFrameTest001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    DfxFrame frame;
    unwinder->FillJsFrame(frame);
    GTEST_LOG_(INFO) << " when DfxFrame::map is null, frame.map is nullptr";
    ASSERT_EQ(frame.map, nullptr);
    string testMap = "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context.z.so";
    auto map = DfxMap::Create(testMap);
    frame.map = map;
    unwinder->FillJsFrame(frame);
    GTEST_LOG_(INFO) << " when DfxFrame::map is not null and file exist, frame.map.GetHap is not nullptr";
    ASSERT_NE(frame.map->GetHap(), nullptr);
    GTEST_LOG_(INFO) << "FillJsFrameTest001: end.";
}

/**
 * @tc.name: FillFramesTest001
 * @tc.desc: test unwinder FillFrames interface
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, FillFramesTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FillFramesTest001: start.";
#ifdef __arm__
    const string testMap = "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context.z.so";
#else
    const string testMap = "7f0ab40000-7f0ab41000 r--p 00000000 b3:07 1882 /system/lib64/init/libinit_context.z.so";
#endif
    auto unwinder = std::make_shared<Unwinder>();
    std::vector<DfxFrame> frames;
    DfxFrame frame;
    auto map = DfxMap::Create(testMap);
    frame.map = map;
    frames.push_back(frame);
    ASSERT_EQ(frames[0].buildId.size(), 0);
    unwinder->FillFrames(frames);
    ASSERT_EQ(frames[0].buildId.size() == 0, false);
    GTEST_LOG_(INFO) << "FillFramesTest001: end.";
}

#if defined(__arm__) || defined(__aarch64__)
/**
 * @tc.name: UnwindLocalWithContextTest001
 * @tc.desc: test unwinder UnwindLocalWithContext interface
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindLocalWithContextTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindLocalWithContextTest001: start.";
    auto regs = DfxRegs::Create();
    auto regsData = regs->RawData();
    GetLocalRegs(regsData);
    ucontext_t context;
    (void)memset_s(&context, sizeof(context), 0, sizeof(context));
#ifdef __arm__
    context.uc_mcontext.arm_r0 = *(regs->GetReg(REG_ARM_R0));
    context.uc_mcontext.arm_r1 = *(regs->GetReg(REG_ARM_R1));
    context.uc_mcontext.arm_r2 = *(regs->GetReg(REG_ARM_R2));
    context.uc_mcontext.arm_r3 = *(regs->GetReg(REG_ARM_R3));
    context.uc_mcontext.arm_r4 = *(regs->GetReg(REG_ARM_R4));
    context.uc_mcontext.arm_r5 = *(regs->GetReg(REG_ARM_R5));
    context.uc_mcontext.arm_r6 = *(regs->GetReg(REG_ARM_R6));
    context.uc_mcontext.arm_r7 = *(regs->GetReg(REG_ARM_R7));
    context.uc_mcontext.arm_r8 = *(regs->GetReg(REG_ARM_R8));
    context.uc_mcontext.arm_r9 = *(regs->GetReg(REG_ARM_R9));
    context.uc_mcontext.arm_r10 = *(regs->GetReg(REG_ARM_R10));
    context.uc_mcontext.arm_fp = *(regs->GetReg(REG_ARM_R11));
    context.uc_mcontext.arm_ip = *(regs->GetReg(REG_ARM_R12));
    context.uc_mcontext.arm_sp = *(regs->GetReg(REG_ARM_R13));
    context.uc_mcontext.arm_lr = *(regs->GetReg(REG_ARM_R14));
    context.uc_mcontext.arm_pc = *(regs->GetReg(REG_ARM_R15));
#else
    for (int i = 0; i < REG_LAST; ++i) {
        context.uc_mcontext.regs[i] = *(regs->GetReg(i));
    }
#endif
    auto unwinder = std::make_shared<Unwinder>();
    ASSERT_TRUE(unwinder->UnwindLocalWithContext(context));
    auto frames = unwinder->GetFrames();
    ASSERT_GT(frames.size(), 1);
    GTEST_LOG_(INFO) << "UnwindLocalWithContextTest001: frames:\n" << Unwinder::GetFramesStr(frames);
    GTEST_LOG_(INFO) << "UnwindLocalWithContextTest001: end.";
}
#endif

static int32_t g_tid = 0;
static std::mutex g_mutex;
__attribute__((noinline)) void ThreadTest002()
{
    printf("ThreadTest002\n");
    g_mutex.lock();
    g_mutex.unlock();
}

__attribute__((noinline)) void ThreadTest001()
{
    g_tid = gettid();
    printf("ThreadTest001: tid: %d\n", g_tid);
    ThreadTest002();
}

/**
 * @tc.name: UnwindLocalWithTidTest001
 * @tc.desc: test unwinder UnwindLocalWithTid interface
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindLocalWithTidTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindLocalWithTidTest001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    g_mutex.lock();
    std::thread unwThread(ThreadTest001);
    sleep(1);
    if (g_tid <= 0) {
        FAIL() << "UnwindLocalWithTidTest001: Failed to create child thread.\n";
    }
#if defined(__aarch64__) || defined(__loongarch_lp64)
    ASSERT_TRUE(unwinder->UnwindLocalWithTid(g_tid));
    auto pcs = unwinder->GetPcs();
    std::vector<DfxFrame> frames;
    unwinder->GetFramesByPcs(frames, pcs);
    ASSERT_GT(frames.size(), 1);
    GTEST_LOG_(INFO) << "UnwindLocalWithTidTest001: frames:\n" << Unwinder::GetFramesStr(frames);
#else
    ASSERT_FALSE(unwinder->UnwindLocalWithTid(g_tid));
#endif
    g_mutex.unlock();
    g_tid = 0;
    if (unwThread.joinable()) {
        unwThread.join();
    }
    GTEST_LOG_(INFO) << "UnwindLocalWithTidTest001: end.";
}

/**
 * @tc.name: UnwindLocalWithTidTest002
 * @tc.desc: test unwinder UnwindLocalWithTid interface
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindLocalWithTidTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindLocalWithTidTest002: start.";
    auto unwinder = std::make_shared<Unwinder>();
    ASSERT_FALSE(unwinder->UnwindLocalWithTid(-1));
    ASSERT_FALSE(unwinder->UnwindLocalWithTid(gettid()));
    UnwindContext context;
    uintptr_t pc, sp;
    ASSERT_FALSE(unwinder->UnwindByFp(&context));
    ASSERT_FALSE(unwinder->FpStep(pc, sp, nullptr));
    ASSERT_FALSE(unwinder->Step(pc, sp, &context));
    std::vector<DfxFrame> frames;
    std::vector<uintptr_t> pcs;
    unwinder->GetFramesByPcs(frames, pcs);
    std::string funcName = "";
    uint64_t funcOffset = 0;
    ASSERT_FALSE(unwinder->GetSymbolByPc(pc, nullptr, funcName, funcOffset));
    ASSERT_FALSE(unwinder->UnwindLocalByOtherTid(-1));
    ASSERT_FALSE(unwinder->UnwindLocalByOtherTid(gettid()));
    GTEST_LOG_(INFO) << "UnwindLocalWithTidTest002: end.";
}

#if defined(__x86_64__)
static _Unwind_Reason_Code TraceFunc(_Unwind_Context *ctx, void *d)
{
    int *depth = (int*)d;
    printf("\t#%d: program counter at %p\n", *depth, reinterpret_cast<void *>(_Unwind_GetIP(ctx)));
    (*depth)++;
    return _URC_NO_REASON;
}

static void PrintUnwindBacktrace()
{
    int depth = 0;
    _Unwind_Backtrace(&TraceFunc, &depth);
}

/**
 * @tc.name: UnwindLocalX86_64Test001
 * @tc.desc: test unwinder UnwindLocal interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindLocalX86_64Test001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindLocalX86_64Test001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    if (unwinder->UnwindLocal()) {
        auto frames = unwinder->GetFrames();
        printf("Unwinder frame size: %zu\n", frames.size());
        auto framesStr = Unwinder::GetFramesStr(frames);
        printf("Unwinder frames:\n%s\n", framesStr.c_str());
        ASSERT_GT(frames.size(), 0);
    }

    PrintUnwindBacktrace();
    GTEST_LOG_(INFO) << "UnwindLocalX86_64Test001: end.";
}

/**
 * @tc.name: UnwindRemoteX86_64Test001
 * @tc.desc: test unwinder UnwindRemote interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwindRemoteX86_64Test001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwindLocalX86_64Test001: start.";
    const pid_t initPid = 1;
    auto unwinder = std::make_shared<Unwinder>(initPid);
    DfxPtrace::Attach(initPid);
    if (unwinder->UnwindRemote(initPid)) {
        auto frames = unwinder->GetFrames();
        printf("Unwinder frame size: %zu\n", frames.size());
        auto framesStr = Unwinder::GetFramesStr(frames);
        printf("Unwinder frames:\n%s\n", framesStr.c_str());
        ASSERT_GT(frames.size(), 0);
    }
    DfxPtrace::Detach(initPid);

    GTEST_LOG_(INFO) << "UnwindRemoteX86_64Test001: end.";
}
#endif

/**
 * @tc.name: GetSymbolByPcTest001
 * @tc.desc: test unwinder GetSymbolByPc interface
 *  in local case
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, GetSymbolByPcTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetSymbolByPcTest001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    unwinder->UnwindLocal();
    auto frames = unwinder->GetFrames();
    uintptr_t pc0 = static_cast<uintptr_t>(frames[0].pc);
    std::string funcName;
    uint64_t funcOffset;
    std::shared_ptr<DfxMaps> maps = std::make_shared<DfxMaps>();
    ASSERT_FALSE(unwinder->GetSymbolByPc(0x00000000, maps, funcName, funcOffset)); // Find map is null
    ASSERT_FALSE(unwinder->GetSymbolByPc(pc0, maps, funcName, funcOffset)); // Get elf is null
    GTEST_LOG_(INFO) << "GetSymbolByPcTest001: end.";
}

/**
 * @tc.name: AccessMemTest001
 * @tc.desc: test unwinder AccessMem interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, AccessMemTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AccessMemTest001: start.";
    auto unwinder = std::make_shared<Unwinder>();
    auto memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    uintptr_t val;
    EXPECT_FALSE(memory->ReadReg(0, &val));
    uintptr_t regs[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa};
    UnwindContext ctx;
    ctx.regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regs, sizeof(regs) / sizeof(regs[0]));
    memory->SetCtx(&ctx);
    EXPECT_FALSE(memory->ReadReg(-1, &val));

    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    uintptr_t addr = reinterpret_cast<uintptr_t>(&values[0]);
    EXPECT_FALSE(unwinder->AccessMem(memory.get(), addr, nullptr));
    GTEST_LOG_(INFO) << "AccessMemTest001: end.";
}

/**
 * @tc.name: AccessMemTest002
 * @tc.desc: test DfxAccessorsLocal AccessMem interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, AccessMemTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AccessMemTest002: start.";
    DfxAccessorsLocal accessors;
    UnwindContext ctx;
    ctx.stackCheck = true;
    ctx.stackBottom = 0xaaaa;
    ctx.stackTop = 0xffff;
#if defined(__aarch64__)
    uintptr_t addr = 0xfffffffffffffff8;
#else
    uintptr_t addr = 0xfffffffc;
#endif
    uintptr_t val;
    auto result = accessors.AccessMem(addr, &val, &ctx);
    ASSERT_EQ(result, UNW_ERROR_INVALID_MEMORY);
    GTEST_LOG_(INFO) << "AccessMemTest002: end.";
}

/**
 * @tc.name: AccessMemTest003
 * @tc.desc: test DfxAccessorsLocal AccessMem interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, AccessMemTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AccessMemTest003: start.";
    DfxAccessorsLocal accessors;
    UnwindContext ctx;
    ctx.stackCheck = true;
    ctx.stackBottom = 0;
    ctx.stackTop = 0;
#if defined(__aarch64__)
    uintptr_t addr = 0xfffffffffffffff0;
#else
    uintptr_t addr = 0xfffffff0;
#endif
    uintptr_t val;
    auto result = accessors.AccessMem(addr, &val, &ctx);
    ASSERT_EQ(result, UNW_ERROR_INVALID_MEMORY);
    GTEST_LOG_(INFO) << "AccessMemTest003: end.";
}

/**
 * @tc.name: UnwinderTest001
 * @tc.desc: test Unwinder::xxxx interface
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest001: start.";
    auto unwinder = std::make_shared<Unwinder>(getpid());
    unwinder->EnableUnwindCache(false);
    unwinder->EnableFpCheckMapExec(false);
    unwinder->EnableJsvmstack(false);
    auto regs = unwinder->GetRegs();
    ASSERT_EQ(regs, nullptr);
    DfxFrame frame;
    unwinder->FillFrame(frame);
    unwinder->AddFrame(frame);
    unwinder->ArkWriteJitCodeToFile(1);
    auto jitCache = unwinder->GetJitCache();
    ASSERT_EQ(jitCache.size(), 0);
    GTEST_LOG_(INFO) << "UnwinderTest001: end.";
}

/**
 * @tc.name: UnwinderTest002
 * @tc.desc: test DfxFrameFormatter GetFrameStr
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest002: start.";
    std::shared_ptr<DfxFrame> frame = nullptr;
    std::string str = DfxFrameFormatter::GetFrameStr(frame);
    ASSERT_EQ(str, "") << "UnwinderTest002: end.";
}

/**
 * @tc.name: UnwinderTest003
 * @tc.desc: test DfxFrameFormatter GetFrameStr
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest003: start.";
    DfxFrame frame;
    frame.isJsFrame = true;
    std::string str = DfxFrameFormatter::GetFrameStr(frame);
    ASSERT_NE(str, "") << "UnwinderTest003: end.";
}

/**
 * @tc.name: UnwinderTest004
 * @tc.desc: test DfxFrameFormatter GetFrameStr
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest004: start.";
    DfxFrame frame;
    frame.isJsFrame = true;
    frame.funcName = "test";
    frame.packageName = "test";
    frame.mapName = "test";
    std::string str = DfxFrameFormatter::GetFrameStr(frame);
    ASSERT_NE(str, "") << "UnwinderTest004: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS

