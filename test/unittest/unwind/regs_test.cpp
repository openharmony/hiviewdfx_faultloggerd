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

#include <gtest/gtest.h>
#include <ctime>
#include <securec.h>
#include <string>
#include <vector>
#include "dfx_ptrace.h"
#include "dfx_regs.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxRegsTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
/**
 * @tc.name: DfxRegsTest001
 * @tc.desc: test DfxRegs SetRegsData & GetRegsData functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxRegsTest, DfxRegsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest001: start.";
    auto dfxRegs = DfxRegs::Create();
    ASSERT_NE(dfxRegs, nullptr);
    std::vector<uintptr_t> setRegs {};
    for (size_t i = 0; i < 10; i++) { // test 10 regs
        setRegs.push_back(i);
    }
    dfxRegs->SetRegsData(setRegs);
    auto getRegs = dfxRegs->GetRegsData();
    ASSERT_EQ(setRegs, getRegs);

    uintptr_t regsData[REG_LAST] = { 0 };
    dfxRegs->SetRegsData(regsData, REG_LAST);
    getRegs = dfxRegs->GetRegsData();
    for (size_t i = 0; i < getRegs.size(); i++) {
        ASSERT_EQ(regsData[i], getRegs[i]);
    }

    GTEST_LOG_(INFO) << "DfxRegsTest001: end.";
}

/**
 * @tc.name: DfxRegsTest002
 * @tc.desc: test DfxRegs GetSpecialRegisterName
 * @tc.type: FUNC
 */
HWTEST_F(DfxRegsTest, DfxRegsTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest002: start.";
    auto dfxRegs = DfxRegs::Create();
    uintptr_t val = 0x00000001;
    dfxRegs->SetPc(val);
    ASSERT_EQ(dfxRegs->GetPc(), val);
    auto name = dfxRegs->GetSpecialRegsName(val);
    ASSERT_EQ(name, "pc");

    val = 0x00000002;
    dfxRegs->SetSp(val);
    ASSERT_EQ(dfxRegs->GetSp(), val);
    ASSERT_EQ(dfxRegs->GetSpecialRegsName(val), "sp");
    ASSERT_EQ((*dfxRegs.get())[REG_SP], val);
    ASSERT_GT(dfxRegs->RegsSize(), 0U);
    uintptr_t *regVal = &val;
    dfxRegs->SetReg(0, regVal);
    ASSERT_EQ((*dfxRegs.get())[0], *regVal);
    uintptr_t fp = 0x00000001;
    uintptr_t lr = 0x00000002;
    uintptr_t sp = 0x00000003;
    uintptr_t pc = 0x00000004;
    dfxRegs->SetSpecialRegs(fp, lr, sp, pc);
    ASSERT_EQ(dfxRegs->GetSpecialRegsName(lr), "lr");
    ASSERT_EQ(dfxRegs->GetSpecialRegsName(fp), "fp");
    ASSERT_EQ(dfxRegs->GetSpecialRegsName(pc), "pc");
    ASSERT_EQ(dfxRegs->GetSpecialRegsName(sp), "sp");
    ASSERT_EQ(dfxRegs->GetSpecialRegsName(0x0), "");
    uintptr_t fpGet;
    uintptr_t lrGet;
    uintptr_t spGet;
    uintptr_t pcGet;
    dfxRegs->GetSpecialRegs(fpGet, lrGet, spGet, pcGet);
    ASSERT_EQ(fp, fpGet);
    ASSERT_EQ(lr, lrGet);
    ASSERT_EQ(sp, spGet);
    ASSERT_EQ(pc, pcGet);
    ASSERT_FALSE(dfxRegs->PrintRegs().empty());
    ASSERT_FALSE(dfxRegs->PrintSpecialRegs().empty());
    GTEST_LOG_(INFO) << "DfxRegsTest002: end.";
}

/**
 * @tc.name: DfxRegsTest003
 * @tc.desc: test DfxRegs CreateFromUcontext
 * @tc.type: FUNC
 */
HWTEST_F(DfxRegsTest, DfxRegsTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest003: start.";

    ucontext_t context;
#if defined(__arm__)
    context.uc_mcontext.arm_r0 = 0;
    context.uc_mcontext.arm_r1 = 1;
    context.uc_mcontext.arm_r2 = 2;
    context.uc_mcontext.arm_r3 = 3;
    context.uc_mcontext.arm_r4 = 4;
    context.uc_mcontext.arm_r5 = 5;
    context.uc_mcontext.arm_r6 = 6;
    context.uc_mcontext.arm_r7 = 7;
    context.uc_mcontext.arm_r8 = 8;
    context.uc_mcontext.arm_r9 = 9;
    context.uc_mcontext.arm_r10 = 10;
    context.uc_mcontext.arm_fp = 11;
    context.uc_mcontext.arm_ip = 12;
    context.uc_mcontext.arm_sp = 13;
    context.uc_mcontext.arm_lr = 14;
    context.uc_mcontext.arm_pc = 15;
#elif defined(__aarch64__)
    for (int i = 0; i < 31; i++) {
        context.uc_mcontext.regs[i] = i;
    }
    context.uc_mcontext.sp = 31;
    context.uc_mcontext.pc = 32;
#elif defined(___x86_64__)
    context.uc_mcontext.gregs[REG_RAX] = 0;
    context.uc_mcontext.gregs[REG_RDX] = 1;
    context.uc_mcontext.gregs[REG_RCX] = 2;
    context.uc_mcontext.gregs[REG_RBX] = 3;
    context.uc_mcontext.gregs[REG_RSI] = 4;
    context.uc_mcontext.gregs[REG_RDI] = 5;
    context.uc_mcontext.gregs[REG_RBP] = 6;
    context.uc_mcontext.gregs[REG_RSP] = 7;
    context.uc_mcontext.gregs[REG_R8] = 8;
    context.uc_mcontext.gregs[REG_R9] = 9;
    context.uc_mcontext.gregs[REG_R10] = 10;
    context.uc_mcontext.gregs[REG_R11] = 11;
    context.uc_mcontext.gregs[REG_R12] = 12;
    context.uc_mcontext.gregs[REG_R13] = 13;
    context.uc_mcontext.gregs[REG_R14] = 14;
    context.uc_mcontext.gregs[REG_R15] = 15;
    context.uc_mcontext.gregs[REG_RIP] = 16;
#endif
    auto dfxRegs = DfxRegs::CreateFromUcontext(context);
    for (size_t i = 0; i < REG_LAST; i++) {
        ASSERT_EQ((*dfxRegs.get())[i], i);
    }
    GTEST_LOG_(INFO) << "DfxRegsTest003: end.";
}

/**
 * @tc.name: DfxRegsTest004
 * @tc.desc: test DfxRegs SetFromQutMiniRegs SetFromFpMiniRegs
 * @tc.type: FUNC
 */
HWTEST_F(DfxRegsTest, DfxRegsTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest004: start.";
#if defined(__arm__)
    uintptr_t pushRegs[QUT_MINI_REGS_SIZE];
    for (size_t i = 0; i < QUT_MINI_REGS_SIZE; ++i) {
        pushRegs[i] = i + 1;
    }
    uintptr_t qutRegs[QUT_MINI_REGS_SIZE] = {REG_ARM_R4, REG_ARM_R7, REG_ARM_R10, REG_ARM_R11,
        REG_SP, REG_PC, REG_LR};
    uintptr_t fpRegs[FP_MINI_REGS_SIZE] = {REG_ARM_R7, REG_FP, REG_SP, REG_PC};
    auto dfxregsArm = std::make_shared<DfxRegsArm>();
    dfxregsArm->SetFromQutMiniRegs(pushRegs, QUT_MINI_REGS_SIZE);
    for (size_t i = 0; i < sizeof(qutRegs) / sizeof(qutRegs[0]); ++i) {
        ASSERT_EQ((*dfxregsArm.get())[qutRegs[i]], pushRegs[i]);
    }
    dfxregsArm->SetFromFpMiniRegs(pushRegs, FP_MINI_REGS_SIZE);
    for (size_t i = 0; i < sizeof(fpRegs) / sizeof(fpRegs[0]); ++i) {
        ASSERT_EQ((*dfxregsArm.get())[fpRegs[i]], pushRegs[i]);
    }

#elif defined(__aarch64__)
    uintptr_t pushRegs[QUT_MINI_REGS_SIZE];
    for (size_t i = 0; i < QUT_MINI_REGS_SIZE; ++i) {
        pushRegs[i] = i;
    }
    uintptr_t qutRegs[QUT_MINI_REGS_SIZE] = {REG_AARCH64_X0, REG_AARCH64_X20, REG_AARCH64_X28,
        REG_FP, REG_SP, REG_AARCH64_PC, REG_LR};
    uintptr_t fpRegs[FP_MINI_REGS_SIZE] = {REG_FP, REG_LR, REG_SP, REG_PC};
    auto dfxregsArm64 = std::make_shared<DfxRegsArm64>();
    dfxregsArm64->SetFromQutMiniRegs(pushRegs, QUT_MINI_REGS_SIZE);
    for (size_t i = 1; i < sizeof(qutRegs) / sizeof(qutRegs[0]); ++i) {
        ASSERT_EQ((*dfxregsArm64.get())[qutRegs[i]], pushRegs[i]);
    }
    dfxregsArm64->SetFromFpMiniRegs(pushRegs, FP_MINI_REGS_SIZE);
    for (size_t i = 0; i < sizeof(fpRegs) / sizeof(fpRegs[0]); ++i) {
        ASSERT_EQ((*dfxregsArm64.get())[fpRegs[i]], pushRegs[i]);
    }
#endif
    GTEST_LOG_(INFO) << "DfxRegsTest004: end.";
}

/**
 * @tc.name: DfxRegsTest005
 * @tc.desc: test DfxRegs CreateFromRegs
 * @tc.type: FUNC
 */
HWTEST_F(DfxRegsTest, DfxRegsTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest005: start.";
#if defined(__arm__)
    uintptr_t regs[QUT_MINI_REGS_SIZE];
    for (size_t i = 0; i < QUT_MINI_REGS_SIZE; ++i) {
        regs[i] = i + 1;
    }
    uintptr_t minimal[QUT_MINI_REGS_SIZE] = {REG_ARM_R4, REG_ARM_R7, REG_ARM_R10, REG_ARM_R11,
        REG_SP, REG_PC, REG_LR};
    uintptr_t framePointer[FP_MINI_REGS_SIZE] = {REG_ARM_R7, REG_FP, REG_SP, REG_PC};
    auto dfxRegs = DfxRegs::CreateFromRegs(UnwindMode::FRAMEPOINTER_UNWIND, regs);
    for (size_t i = 0; i < sizeof(framePointer) / sizeof(framePointer[0]); ++i) {
        ASSERT_EQ((*dfxRegs.get())[framePointer[i]], regs[i]);
    }
    dfxRegs = DfxRegs::CreateFromRegs(UnwindMode::MINIMAL_UNWIND, regs);
    for (size_t i = 0; i < sizeof(minimal) / sizeof(minimal[0]); ++i) {
        ASSERT_EQ((*dfxRegs.get())[minimal[i]], regs[i]);
    }

#elif defined(__aarch64__)
    uintptr_t regs[QUT_MINI_REGS_SIZE];
    for (size_t i = 0; i < QUT_MINI_REGS_SIZE; ++i) {
        regs[i] = i;
    }
    uintptr_t minimal[QUT_MINI_REGS_SIZE] = {REG_AARCH64_X0, REG_AARCH64_X20, REG_AARCH64_X28,
        REG_FP, REG_SP, REG_PC, REG_LR};
    uintptr_t framePointer[FP_MINI_REGS_SIZE] = {REG_FP, REG_LR, REG_SP, REG_PC};
    auto dfxRegs = DfxRegs::CreateFromRegs(UnwindMode::FRAMEPOINTER_UNWIND, regs);
    for (size_t i = 1; i < sizeof(framePointer) / sizeof(framePointer[0]); ++i) {
        ASSERT_EQ((*dfxRegs.get())[framePointer[i]], regs[i]);
    }
    dfxRegs = DfxRegs::CreateFromRegs(UnwindMode::MINIMAL_UNWIND, regs);
    for (size_t i = 0; i < sizeof(minimal) / sizeof(minimal[0]); ++i) {
        ASSERT_EQ((*dfxRegs.get())[minimal[i]], regs[i]);
    }
#endif
    GTEST_LOG_(INFO) << "DfxRegsTest005: end.";
}


/**
 * @tc.name: DfxRegsTest006
 * @tc.desc: test DfxRegs CreateRemoteRegs
 * @tc.type: FUNC
 */
HWTEST_F(DfxRegsTest, DfxRegsTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest006: start.";
    static pid_t pid = getpid();
    pid_t child = fork();
    if (child == 0) {
        DfxPtrace::Attach(pid);
        auto dfxRegs = DfxRegs::CreateRemoteRegs(pid);
        constexpr size_t maxIdx = 100;
        ASSERT_NE(dfxRegs->GetReg(0), nullptr);
        ASSERT_EQ(dfxRegs->GetReg(maxIdx), nullptr);
        uintptr_t value = 0;
        dfxRegs->SetReg(maxIdx, &value);
        uintptr_t fp = 0x80;
        dfxRegs->SetFp(fp);
        ASSERT_EQ(dfxRegs->GetFp(), fp);
        DfxPtrace::Detach(pid);
        _exit(0);
    }
    int status;
    int ret = wait(&status);
    ASSERT_EQ(status, 0);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "DfxRegsTest006: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS
