/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "dfx_define.h"
#define private public
#define protected public
#include "exidx_entry_parser.h"
#undef private
#undef protected
#include "dfx_memory.h"
using namespace testing;
using namespace testing::ext;
#if defined(__arm__)
namespace OHOS {
namespace HiviewDFX {
class ArmExidxTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void ArmExidxTest::SetUpTestCase()
{}

void ArmExidxTest::TearDownTestCase()
{}

void ArmExidxTest::SetUp()
{}

void ArmExidxTest::TearDown()
{}

/**
 * @tc.name: ArmExidxTest001
 * @tc.desc: test ExtractEntryData
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArmExidxTest001: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x1, 0x1};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    // cant unwind
    ASSERT_FALSE(exidx.ExtractEntryData(entryOffset));
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_CANT_UNWIND);
    GTEST_LOG_(INFO) << "ArmExidxTest001: end.";
}
/**
 * @tc.name: ArmExidxTest002
 * @tc.desc: test ExtractEntryData inline compact model
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArmExidxTest002: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    // inline compact model
    uint32_t values[] = {0x7fff2340, 0x80c0c0c0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    ASSERT_TRUE(exidx.ExtractEntryData(entryOffset));
    std::deque<uint8_t> data = exidx.ops_;
    ASSERT_EQ(data.size(), 4U);
    ASSERT_EQ(data[0], 0xc0U);
    ASSERT_EQ(data[1], 0xc0U);
    ASSERT_EQ(data[2], 0xc0U);
    ASSERT_EQ(data[3], 0xb0U);
    GTEST_LOG_(INFO) << "ArmExidxTest002: end.";
}
/**
 * @tc.name: ArmExidxTest003
 * @tc.desc: test ExtractEntryData highest bit is zero, point to .ARM.extab data
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArmExidxTest003: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    //personality 0
    uint32_t values0[] = {0x7fff2340, 0x00001111, 0x80c0c0c0};
    uintptr_t entryOffset = (uintptr_t)(&values0[0]);
    ASSERT_TRUE(exidx.ExtractEntryData(entryOffset));
    std::deque<uint8_t> data = exidx.ops_;
    ASSERT_EQ(data.size(), 4U);
    ASSERT_EQ(data[0], 0xc0U);
    ASSERT_EQ(data[1], 0xc0U);
    ASSERT_EQ(data[2], 0xc0U);
    ASSERT_EQ(data[3], 0xb0U);

    //personality 1
    uint32_t values1[] = {0x7fff2340, 0x00001111, 0x8100c0c0};
    entryOffset = (uintptr_t)(&values1[0]);
    ASSERT_TRUE(exidx.ExtractEntryData(entryOffset));
    data = exidx.ops_;
    ASSERT_EQ(data.size(), 3U);
    ASSERT_EQ(data[0], 0xc0U);
    ASSERT_EQ(data[1], 0xc0U);
    ASSERT_EQ(data[2], 0xb0U);

    //personality 2 MOREWORD 0
    uint32_t values2[] = {0x7fff2340, 0x00001111, 0x8200c0c0};
    entryOffset = (uintptr_t)(&values2[0]);
    ASSERT_TRUE(exidx.ExtractEntryData(entryOffset));
    data = exidx.ops_;
    ASSERT_EQ(data.size(), 3U);
    ASSERT_EQ(data[0], 0xc0U);
    ASSERT_EQ(data[1], 0xc0U);
    ASSERT_EQ(data[2], 0xb0U);

    //personality 2 MOREWORD 1
    uint32_t values2m[] = {0x7fff2340, 0x00001111, 0x8201c0c0, 0xd0e0f0b0};
    entryOffset = (uintptr_t)(&values2m[0]);
    ASSERT_TRUE(exidx.ExtractEntryData(entryOffset));
    data = exidx.ops_;
    ASSERT_EQ(data.size(), 6U);
    ASSERT_EQ(data[0], 0xc0U);
    ASSERT_EQ(data[1], 0xc0U);
    ASSERT_EQ(data[2], 0xd0U);
    ASSERT_EQ(data[3], 0xe0U);
    ASSERT_EQ(data[4], 0xf0U);
    ASSERT_EQ(data[5], 0xb0U);
    GTEST_LOG_(INFO) << "ArmExidxTest003: end.";
}
/**
 * @tc.name: ArmExidxTest004
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest004, TestSize.Level2)
{
    // 00xxxxxx: vsp = vsp + (xxxxxx << 2) + 4
    GTEST_LOG_(INFO) << "ArmExidxTest004: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x810010b0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x44);
    GTEST_LOG_(INFO) << "ArmExidxTest004: end.";
}

/**
 * @tc.name: ArmExidxTest005
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest005, TestSize.Level2)
{
    // 01xxxxxx: vsp = vsp - (xxxxxx << 2) - 4
    GTEST_LOG_(INFO) << "ArmExidxTest005: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x810041b0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, -8);
    GTEST_LOG_(INFO) << "ArmExidxTest005: end.";
}

/**
 * @tc.name: ArmExidxTest006
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest006, TestSize.Level2)
{
    // 10000000 00000000: Refuse to unwind
    GTEST_LOG_(INFO) << "ArmExidxTest006: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x81008000};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    ASSERT_TRUE(exidx.Eval(entryOffset));
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_CANT_UNWIND);
    GTEST_LOG_(INFO) << "ArmExidxTest006: end.";
}

/**
 * @tc.name: ArmExidxTest007
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest007, TestSize.Level2)
{
    // 1000iiii iiiiiiii (i not all 0)
    GTEST_LOG_(INFO) << "ArmExidxTest007: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x81008811};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 12);
    ASSERT_EQ(exidx.rsState_->locs[3].type, REG_LOC_MEM_OFFSET);
    ASSERT_EQ(exidx.rsState_->locs[3].val, -4);
    GTEST_LOG_(INFO) << "ArmExidxTest007: end.";
}

/**
 * @tc.name: ArmExidxTest008
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest008, TestSize.Level2)
{
    // 10011101 || 10011111
    GTEST_LOG_(INFO) << "ArmExidxTest008: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x81009db0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    ASSERT_TRUE(exidx.Eval(entryOffset));
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_RESERVED_VALUE);

    values[2] = 0x81009fb0;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_RESERVED_VALUE);
    GTEST_LOG_(INFO) << "ArmExidxTest008: end.";
}

/**
 * @tc.name: ArmExidxTest009
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArmExidxTest009: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x81009bb0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    // 1001nnnn(nnnn != 13, 15)
    ASSERT_EQ(exidx.rsState_->cfaReg, 11U);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0);
    GTEST_LOG_(INFO) << "ArmExidxTest009: end.";
}

/**
 * @tc.name: ArmExidxTest010
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest010, TestSize.Level2)
{
    //10100nnn Pop r4-r[4+nnn]
    GTEST_LOG_(INFO) << "ArmExidxTest010: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100a7b0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 32);
    ASSERT_EQ(exidx.rsState_->locs[0].type, REG_LOC_MEM_OFFSET);
    ASSERT_EQ(exidx.rsState_->locs[0].val, -20);
    ASSERT_EQ(exidx.rsState_->locs[1].type, REG_LOC_MEM_OFFSET);
    ASSERT_EQ(exidx.rsState_->locs[1].val, -4);
    GTEST_LOG_(INFO) << "ArmExidxTest010: end.";
}

/**
 * @tc.name: ArmExidxTest011
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest011, TestSize.Level2)
{
    // 10101nnn Pop r4-r[4+nnn], r14
    GTEST_LOG_(INFO) << "ArmExidxTest011: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100afb0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 36);
    ASSERT_EQ(exidx.rsState_->locs[0].type, REG_LOC_MEM_OFFSET);
    ASSERT_EQ(exidx.rsState_->locs[0].val, -24);
    ASSERT_EQ(exidx.rsState_->locs[1].type, REG_LOC_MEM_OFFSET);
    ASSERT_EQ(exidx.rsState_->locs[1].val, -8);
    ASSERT_EQ(exidx.rsState_->locs[4].type, REG_LOC_MEM_OFFSET);
    ASSERT_EQ(exidx.rsState_->locs[4].val, -4);
    GTEST_LOG_(INFO) << "ArmExidxTest011: end.";
}

/**
 * @tc.name: ArmExidxTest012
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest012, TestSize.Level2)
{
    // 10110000 Finish
    GTEST_LOG_(INFO) << "ArmExidxTest012: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100b0b0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0);
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_ARM_EXIDX_FINISH);
    GTEST_LOG_(INFO) << "ArmExidxTest012: end.";
}

/**
 * @tc.name: ArmExidxTest013
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest013, TestSize.Level2)
{
    // 10110001 00000000: Spare
    GTEST_LOG_(INFO) << "ArmExidxTest013: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100b100};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0);
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_ARM_EXIDX_SPARE);

    // 10110001 xxxxyyyy spare

    GTEST_LOG_(INFO) << "ArmExidxTest013: end.";
}

/**
 * @tc.name: ArmExidxTest014
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest014, TestSize.Level2)
{
    // 10110001 0000iiii(i not all 0) Pop integer registers under mask{r3, r2,r1,r0}
    GTEST_LOG_(INFO) << "ArmExidxTest014: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100b108};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 4);
    GTEST_LOG_(INFO) << "ArmExidxTest014: end.";
}

/**
 * @tc.name: ArmExidxTest015
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest015, TestSize.Level2)
{
    // 10110010 uleb128 vsp = vsp + 0x204 + (uleb128 << 2)
    GTEST_LOG_(INFO) << "ArmExidxTest015: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100b208};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x224);
    GTEST_LOG_(INFO) << "ArmExidxTest015: end.";
}

/**
 * @tc.name: ArmExidxTest016
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest016, TestSize.Level2)
{
    // 10110011 sssscccc: Pop VFP double precision registers D[ssss]-D[ssss+cccc] saved by FSTMFDX
    GTEST_LOG_(INFO) << "ArmExidxTest016: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100b302};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x1c);
    GTEST_LOG_(INFO) << "ArmExidxTest016: end.";
}


/**
 * @tc.name: ArmExidxTest017
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest017, TestSize.Level2)
{
    // 10111nnn:  VFP double-precision registers D[8]-D[8+nnn] saved by FSTMFDX
    GTEST_LOG_(INFO) << "ArmExidxTest016: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);

    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100b9b0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x14);
    GTEST_LOG_(INFO) << "ArmExidxTest016: end.";
}

/**
 * @tc.name: ArmExidxTest018
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest018, TestSize.Level2)
{
    /* 11000nnn (nnn != 6,7) : Intel Wireless MMX pop wR[10]-wR[10+nnn]
    *  11000110 sssscccc : Intel Wireless MMX pop wR[ssss]-wR[ssss+cccc]
    *  11000111 00000000 : spare
    *  11000111 0000iiii : Intel Wireless MMX pop wCGR register under mask {wCGR3,2,1,0}
    *  11000111 xxxxyyyy (xxxx!=0000): spare
    */
    GTEST_LOG_(INFO) << "ArmExidxTest018: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    // 11000nnn (nnn != 6,7)
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100c1b0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x10);

    // 11000110 sssscccc
    values[2] = 0x8100c602;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x18);

    // 11000111 00000000 : spare
    values[2] = 0x8100c700;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0);
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_ARM_EXIDX_SPARE);

    // 11000111 0000iiii : Intel Wireless MMX pop wCGR register under mask {wCGR3,2,1,0}
    values[2] = 0x8100c70f;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x10);

    //11000111 xxxxyyyy (xxxx!=0000): spare
    values[2] = 0x8100c71f;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0);
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_ARM_EXIDX_SPARE);
    GTEST_LOG_(INFO) << "ArmExidxTest018: end.";
}

/**
 * @tc.name: ArmExidxTest019
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest019, TestSize.Level2)
{
    /**
     * 11001000 sssscccc Pop VFP double precision registers
     *    D[16+ssss]-D[16+ssss+cccc] saved (as if) by VPUSH (see remarks d,e)
     * 11001001 sssscccc Pop VFP double precision registers D[ssss]-D[ssss+cccc] saved (as if) by VPUSH (see remark d)
     * 11001yyy(yyy != 000, 001) Spare
    */
    GTEST_LOG_(INFO) << "ArmExidxTest019: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    // 11001000 sssscccc
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100c801};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x10);

    // 11001001 sssscccc
    values[2] = 0x8100c902;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x18);

    // 11001yyy(yyy != 000, 001) Spare
    values[2] = 0x8100cbb0;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_ARM_EXIDX_SPARE);
    GTEST_LOG_(INFO) << "ArmExidxTest019: end.";
}

/**
 * @tc.name: ArmExidxTest020
 * @tc.desc: test Eval
 * @tc.type: FUNC
 */
HWTEST_F(ArmExidxTest, ArmExidxTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArmExidxTest020: start.";
    std::shared_ptr<DfxMemory> memory = std::make_shared<DfxMemory>(UNWIND_TYPE_LOCAL);
    ExidxEntryParser exidx(memory);
    // 11010nnn Pop VFP double-precision registers D[8]-D[8+nnn] saved (as if) by VPUSH (seeremark d)
    uint32_t values[] = {0x7fff2340, 0x00001111, 0x8100d1b0};
    uintptr_t entryOffset = (uintptr_t)(&values[0]);
    exidx.rsState_ = std::make_shared<RegLocState>();
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.rsState_->cfaRegOffset, 0x10);

    // 11xxxyyy: Spare (xxx != 000, 001, 010)
    values[2] = 0x8100f8b0;
    ASSERT_TRUE(exidx.Eval(entryOffset));
    exidx.FlushInstr();
    ASSERT_EQ(exidx.rsState_->cfaReg, REG_SP);
    ASSERT_EQ(exidx.GetLastErrorCode(), UNW_ERROR_ARM_EXIDX_SPARE);
    GTEST_LOG_(INFO) << "ArmExidxTest020: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
#endif
