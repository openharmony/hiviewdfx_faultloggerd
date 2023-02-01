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
#include <string>
#include <cstdlib>

#include "dfx_define.h"
#include "rustc_demangle.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
static const int BUF_LEN = 100;

class RustcDemangleTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void RustcDemangleTest::SetUpTestCase()
{}

void RustcDemangleTest::TearDownTestCase()
{}

void RustcDemangleTest::SetUp()
{}

void RustcDemangleTest::TearDown()
{}

/**
 * @tc.name: RustcDemangleTest001
 * @tc.desc: test RustcDemangle ok
 * @tc.type: FUNC
 */
HWTEST_F(RustcDemangleTest, RustcDemangleTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RustcDemangleTest001: start.";
    char buf[BUF_LEN] = "_RNvCs7FufEY4pgE0_21panic_handler_example4main\0";
    char *res = (char *)std::malloc(sizeof(char) * BUF_LEN);
    res = rustc_demangle(buf);
    int ret = -1;
    if (res != nullptr) {
        GTEST_LOG_(INFO) << "RustcDemangleTest001: res=" << std::string(res);
        ret = strcmp(res, "panic_handler_example::main");
        std::free(res);
    }
    EXPECT_EQ(true, ret == 0) << "RustcDemangleTest001 Failed";
    GTEST_LOG_(INFO) << "RustcDemangleTest001: end.";
}

/**
 * @tc.name: RustcDemangleTest002
 * @tc.desc: test RustcDemangle not rust symbol
 * @tc.type: FUNC
 */
HWTEST_F(RustcDemangleTest, RustcDemangleTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RustcDemangleTest002: start.";
    char buf[BUF_LEN] = "la la la\0";
    char *res = (char *)std::malloc(sizeof(char) * BUF_LEN);
    res = rustc_demangle(buf);
    EXPECT_EQ(true, res == nullptr) << "RustcDemangleTest002 Failed";
    if (res != nullptr) {
        free(res);
    }
    GTEST_LOG_(INFO) << "RustcDemangleTest002: end.";
}

/**
 * @tc.name: RustcDemangleTest003
 * @tc.desc: test RustcDemangle str null
 * @tc.type: FUNC
 */
HWTEST_F(RustcDemangleTest, RustcDemangleTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RustcDemangleTest003: start.";
    char buf[BUF_LEN] = "\0";
    char *res = (char *)malloc(sizeof(char) * BUF_LEN);
    res = rustc_demangle(buf);
    EXPECT_EQ(true, res == nullptr) << "RustcDemangleTest003 Failed";
    if (res != nullptr) {
        std::free(res);
    }
    GTEST_LOG_(INFO) << "RustcDemangleTest003: end.";
}

/**
 * @tc.name: RustcDemangleTest004
 * @tc.desc: test RustcDemangle invalidid utf8
 * @tc.type: FUNC
 */
HWTEST_F(RustcDemangleTest, RustcDemangleTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "RustcDemangleTest004: start.";
    char buf[BUF_LEN] = "\xe7\x8a\xb6\xe6\x80\x81\0";
    char *res = (char *)std::malloc(sizeof(char) * BUF_LEN);
    res = rustc_demangle(buf);
    EXPECT_EQ(true, res == nullptr) << "RustcDemangleTest004 Failed";
    if (res != nullptr) {
        std::free(res);
    }
    GTEST_LOG_(INFO) << "RustcDemangleTest004: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
