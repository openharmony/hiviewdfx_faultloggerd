/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "dfx_define.h"
#include "dfx_param.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ParamTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: DfxParamTest001
 * @tc.desc: test DfxParam class functions
 * @tc.type: FUNC
 */
HWTEST_F(ParamTest, DfxParamTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxParamTest001: start.";
    ASSERT_EQ(DfxParam::EnableMixstack(), true);
    GTEST_LOG_(INFO) << "DfxParamTest001: end.";
}
} // namespace HiviewDFX
} // namespace OHOS