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

#include "dfx_memory.h"
#include "dfx_regs.h"
#include "dfx_regs_get.h"
#include "dwarf_define.h"
#include "stack_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxMemoryTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
/**
 * @tc.name: DfxMemoryTest001
 * @tc.desc: test DfxMemory class ReadReg
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest001: start.";
    uintptr_t regs[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa};
    UnwindLocalContext ctx;
    ctx.regs = DfxRegs::CreateFromRegs(UnwindMode::DWARF_UNWIND, regs);
    auto acc = std::make_shared<DfxAccessorsLocal>();
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t value;
    bool ret = memory->ReadReg(0, &value);
    EXPECT_EQ(true, ret) << "DfxMemoryTest001: ret" << ret;
    EXPECT_EQ((uintptr_t)0x1, value) << "DfxMemoryTest001: value" << value;
    GTEST_LOG_(INFO) << "DfxMemoryTest001: end.";
}

/**
 * @tc.name: DfxMemoryTest002
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest002: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};

    uintptr_t addr = (uintptr_t)(&values[0]);
    uintptr_t value;
    bool ret = DfxMemoryCpy::GetInstance().ReadUptr(addr, &value, false);
    EXPECT_EQ(true, ret) << "DfxMemoryTest002: ret:" << ret;


    uint64_t tmp;
    DfxMemoryCpy::GetInstance().Read(addr, &tmp, sizeof(uint8_t), false);
    ASSERT_EQ(tmp, 0x01);

    DfxMemoryCpy::GetInstance().Read(addr, &tmp, sizeof(uint16_t), false);
    ASSERT_EQ(tmp, 0x0201);

    DfxMemoryCpy::GetInstance().Read(addr, &tmp, sizeof(uint32_t), false);
    ASSERT_EQ(tmp, 0x04030201);

    DfxMemoryCpy::GetInstance().Read(addr, &tmp, sizeof(uint64_t), false);
    ASSERT_EQ(tmp, 0x0807060504030201);

    GTEST_LOG_(INFO) << "DfxMemoryTest002: end.";
}

/**
 * @tc.name: DfxMemoryTest003
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest003: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    UnwindLocalContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_EQ(GetSelfStackRange(ctx.stackBottom, ctx.stackTop), 0);
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t addr = (uintptr_t)(&values[0]);
    uintptr_t value;
    ASSERT_TRUE(memory->ReadUptr(addr, &value, false));
#if defined(__arm__)
    ASSERT_EQ(value, 0x04030201);
#elif defined(__aarch64__)
    ASSERT_EQ(value, 0x0807060504030201);
#endif

    uint64_t tmp;
    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint8_t), false));
    ASSERT_EQ(tmp, 0x01);

    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint16_t), false));
    ASSERT_EQ(tmp, 0x0201);

    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint32_t), false));
    ASSERT_EQ(tmp, 0x04030201);

    ASSERT_TRUE(memory->Read(addr, &tmp, sizeof(uint64_t), false));
    ASSERT_EQ(tmp, 0x0807060504030201);

    GTEST_LOG_(INFO) << "DfxMemoryTest003: end.";
}

/**
 * @tc.name: DfxMemoryTest004
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest004: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    UnwindLocalContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_EQ(GetSelfStackRange(ctx.stackBottom, ctx.stackTop), 0);
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t addr = (uintptr_t)(&values[0]);
    uint8_t tmp8;
    ASSERT_TRUE(memory->ReadU8(addr, &tmp8, false));
    ASSERT_EQ(tmp8, 0x01);
    uint16_t tmp16;
    ASSERT_TRUE(memory->ReadU16(addr, &tmp16, false));
    ASSERT_EQ(tmp16, 0x0201);
    uint32_t tmp32;
    ASSERT_TRUE(memory->ReadU32(addr, &tmp32, false));
    ASSERT_EQ(tmp32, 0x04030201);
    uint64_t tmp64;
    ASSERT_TRUE(memory->ReadU64(addr, &tmp64, false));
    ASSERT_EQ(tmp64, 0x0807060504030201);
    GTEST_LOG_(INFO) << "DfxMemoryTest004: end.";
}

/**
 * @tc.name: DfxMemoryTest005
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest005: start.";
    uint8_t values[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};
    UnwindLocalContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_EQ(GetSelfStackRange(ctx.stackBottom, ctx.stackTop), 0);
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    uintptr_t addr = (uintptr_t)(&values[0]);
    uintptr_t valuePrel32;
    ASSERT_TRUE(memory->ReadPrel31(addr, &valuePrel32));
    ASSERT_EQ(valuePrel32, 0x04030201 + addr);
    char testStr[] = "Test ReadString Func";
    std::string resultStr;
    uintptr_t addrStr = (uintptr_t)(&testStr[0]);
    ASSERT_TRUE(memory->ReadString(addrStr, &resultStr, sizeof(testStr)/sizeof(char), false));
    ASSERT_EQ(testStr, resultStr);
    ASSERT_EQ(memory->ReadUleb128(addr), 1U);
    ASSERT_EQ(memory->ReadSleb128(addr), 2);
    GTEST_LOG_(INFO) << "DfxMemoryTest005: end.";
}

/**
 * @tc.name: DfxMemoryTest006
 * @tc.desc: test DfxMemory class Read
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest006: start.";
    UnwindLocalContext ctx;
    auto acc = std::make_shared<DfxAccessorsLocal>();
    ASSERT_EQ(GetSelfStackRange(ctx.stackBottom, ctx.stackTop), 0);
    auto memory = std::make_shared<DfxMemory>(acc);
    memory->SetCtx(&ctx);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_absptr), sizeof(uintptr_t));
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata1), 1);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata2), 2);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata4), 4);
    ASSERT_EQ(memory->GetEncodedSize(DW_EH_PE_sdata8), 8);
    GTEST_LOG_(INFO) << "DfxMemoryTest006: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS
