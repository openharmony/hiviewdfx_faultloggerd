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
#include "dfx_regs_define.h"

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
} // namespace HiviewDFX
} // namespace OHOS

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
    dfxRegs->pc_ = val;
    auto name = dfxRegs->GetSpecialRegisterName(val);
    ASSERT_EQ(name, "pc");
    GTEST_LOG_(INFO) << "DfxRegsTest002: end.";
}
