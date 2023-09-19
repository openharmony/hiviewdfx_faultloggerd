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

    auto memory = std::make_shared<DfxMemoryCpy>();

    uintptr_t addr = (uintptr_t)(&values[0]);
    uintptr_t value;
    bool ret = memory->ReadUptr(addr, &value, false);
    EXPECT_EQ(true, ret) << "DfxMemoryTest002: ret:" << ret;
    printf("addr: %llx, value: %llx \n", static_cast<uint64_t>(addr), static_cast<uint64_t>(value));

    uint64_t tmp;
    memory->Read(addr, &tmp, sizeof(uint8_t), false);
    printf("addr: %llx, u8: %llx \n", static_cast<uint64_t>(addr), tmp);
    ASSERT_EQ(tmp, 0x01);

    memory->Read(addr, &tmp, sizeof(uint16_t), false);
    printf("addr: %llx, u16: %llx \n", static_cast<uint64_t>(addr), tmp);
    ASSERT_EQ(tmp, 0x0201);

    memory->Read(addr, &tmp, sizeof(uint32_t), false);
    printf("addr: %llx, u32: %llx \n", static_cast<uint64_t>(addr), tmp);
    ASSERT_EQ(tmp, 0x04030201);

    memory->Read(addr, &tmp, sizeof(uint64_t), false);
    printf("addr: %llx, u64: %llx \n", static_cast<uint64_t>(addr), tmp);
    ASSERT_EQ(tmp, 0x0807060504030201);

    GTEST_LOG_(INFO) << "DfxMemoryTest002: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS
