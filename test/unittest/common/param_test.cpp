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
#include "dfx_trace.h"
#include "dfx_util.h"

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
    ASSERT_EQ(DfxParam::IsEnableMixstack(), true);
    GTEST_LOG_(INFO) << "DfxParamTest001: end.";
}

/**
 * @tc.name: TraceTest001
 * @tc.desc: test FormatTraceName functions
 * @tc.type: FUNC
 */
HWTEST_F(ParamTest, TraceTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "TraceTest001: start.";
    char *name = nullptr;
    FormatTraceName(name, 0, nullptr);
    ASSERT_EQ(name, nullptr);
    const size_t size = 2;
    FormatTraceName(name, size, nullptr);
    ASSERT_EQ(name, nullptr);
    GTEST_LOG_(INFO) << "TraceTest001: end.";
}

/**
 * @tc.name: DfxutilTest001
 * @tc.desc: test TrimAndDupStr functions
 * @tc.type: FUNC
 */
HWTEST_F(ParamTest, DfxutilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxutilTest001: start.";
    std::string filePath = "";
    std::vector<const std::string> validPaths;
    bool ret = VerifyFilePath(filePath, validPaths);
    ASSERT_EQ(ret, true);
    GTEST_LOG_(INFO) << "DfxutilTest001: end.";
}
} // namespace HiviewDFX
} // namespace OHOS