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
    dfxRegs->SetRegsData(regsData);
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
 * @tc.desc: test DfxRegs SetFromFpMiniRegs
 * @tc.type: FUNC
 */
HWTEST_F(DfxRegsTest, DfxRegsTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest004: start.";
    uintptr_t regs[4] = {1, 2, 3, 4};
#if defined(__arm__)
    auto dfxregsArm = std::make_shared<DfxRegsArm>();
    dfxregsArm->SetFromFpMiniRegs(regs);
    ASSERT_EQ((*dfxregsArm.get())[7], regs[0]);
    ASSERT_EQ((*dfxregsArm.get())[11], regs[1]);
    ASSERT_EQ((*dfxregsArm.get())[13], regs[2]);
    ASSERT_EQ((*dfxregsArm.get())[15], regs[3]);

#elif defined(__aarch64__)
    auto dfxregsArm64 = std::make_shared<DfxRegsArm64>();
    dfxregsArm64->SetFromFpMiniRegs(regs);
    ASSERT_EQ((*dfxregsArm64.get())[29], regs[0]);
    ASSERT_EQ((*dfxregsArm64.get())[30], regs[1]);
    ASSERT_EQ((*dfxregsArm64.get())[31], regs[2]);
    ASSERT_EQ((*dfxregsArm64.get())[32], regs[3]);
#endif
    GTEST_LOG_(INFO) << "DfxRegsTest004: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS
