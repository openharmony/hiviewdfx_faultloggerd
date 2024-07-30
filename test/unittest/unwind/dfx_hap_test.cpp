/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#define private public
#include "dfx_hap.h"
#undef private

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxHapTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: DfxHapTest001
 * @tc.desc: test DfxHap functions ParseHapInfo exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxHapTest, DfxHapTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxHapTest001: start.";
    DfxHap dfxHap;
    pid_t pid = 1;
    uint64_t pc = 1;
    uintptr_t methodid = 1;
    auto map = std::make_shared<DfxMap>();
    JsFunction jsFunction;

    bool res = dfxHap.ParseHapInfo(pid, pc, methodid, map, nullptr);
    ASSERT_EQ(res, false);
    res = dfxHap.ParseHapInfo(pid, pc, methodid, map, &jsFunction);
    ASSERT_EQ(res, false);
    map->name = "test.hap";
    res = dfxHap.ParseHapInfo(pid, pc, methodid, map, &jsFunction);
    ASSERT_EQ(res, false);
    GTEST_LOG_(INFO) << "DfxHapTest001: end.";
}

/**
 * @tc.name: DfxHapTest002
 * @tc.desc: test DfxHap ParseHapMemData exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxHapTest, DfxHapTest002, TestSize.Level2)
{
    DfxHap dfxHap;
    GTEST_LOG_(INFO) << "DfxHapTest002: start.";
    pid_t pid = 1;
    auto map = std::make_shared<DfxMap>();
    map->begin = 10001; // 10001 : simulate the begin value of DfxMap
    map->end = 10101; // 10101 : simulate the end value of DfxMap
    auto res = dfxHap.ParseHapMemData(pid, nullptr);
    ASSERT_EQ(res, false);
    res = dfxHap.ParseHapMemData(pid, map);
    ASSERT_EQ(res, false);
    size_t abcDataSize = map->end - map->begin;
    dfxHap.abcDataPtr_ = std::make_unique<uint8_t[]>(abcDataSize);
    res = dfxHap.ParseHapMemData(pid, map);
    ASSERT_EQ(res, true);
    GTEST_LOG_(INFO) << "DfxHapTest002: end.";
}

/**
 * @tc.name: DfxHapTest003
 * @tc.desc: test DfxHap ParseHapFileData exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxHapTest, DfxHapTest003, TestSize.Level2)
{
    DfxHap dfxHap;
    GTEST_LOG_(INFO) << "DfxHapTest003: start.";
    std::string name = "";
    auto res = dfxHap.ParseHapFileData(name);
    ASSERT_EQ(res, false);
    name = "test";
    res = dfxHap.ParseHapFileData(name);
    ASSERT_EQ(res, false);
    dfxHap.abcDataPtr_ = std::make_unique<uint8_t[]>(1);
    res = dfxHap.ParseHapFileData(name);
    ASSERT_EQ(res, true);

    GTEST_LOG_(INFO) << "DfxHapTest003: end.";
}

/**
 * @tc.name: DfxHapTest004
 * @tc.desc: test DfxHap ParseHapMemInfo exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxHapTest, DfxHapTest004, TestSize.Level2)
{
    DfxHap dfxHap;
    GTEST_LOG_(INFO) << "DfxHapTest004: start.";
    pid_t pid = 1;
    uint64_t pc = 1;
    uintptr_t methodid = 1;
    auto map = std::make_shared<DfxMap>();
    JsFunction jsFunction;
    auto res = dfxHap.ParseHapMemInfo(pid, pc, methodid, map, nullptr);
    ASSERT_EQ(res, false);
    res = dfxHap.ParseHapMemInfo(pid, pc, methodid, map, &jsFunction);
    ASSERT_EQ(res, false);
    GTEST_LOG_(INFO) << "DfxHapTest004: end.";
}


} // namespace HiviewDFX
} // namespace OHOS