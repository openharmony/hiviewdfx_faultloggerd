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
#include "dfx_map.h"

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
HWTEST_F(DfxHapTest, DfxHapTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DfxHapTest001: start.";
    DfxHap dfxHap;
    pid_t pid = 1;
    uint64_t pc = 1;
    auto map = std::make_shared<DfxMap>();
    JsFunction jsFunction;

    bool res = dfxHap.ParseHapInfo(pid, pc, map, nullptr);
    ASSERT_EQ(res, false);
    res = dfxHap.ParseHapInfo(pid, pc, map, &jsFunction);
    ASSERT_EQ(res, false);
    map->name = "test.hap";
    res = dfxHap.ParseHapInfo(pid, pc, map, &jsFunction);
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
    auto map = std::make_shared<DfxMap>();
    JsFunction jsFunction;
    auto res = dfxHap.ParseHapMemInfo(pid, pc, map, nullptr);
    ASSERT_EQ(res, false);
    res = dfxHap.ParseHapMemInfo(pid, pc, map, &jsFunction);
    ASSERT_EQ(res, false);
    GTEST_LOG_(INFO) << "DfxHapTest004: end.";
}

/**
 * @tc.name: DfxHapTest005
 * @tc.desc: test DfxHap ParseHapFileInfo exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxHapTest, DfxHapTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxHapTest005: start.";
    DfxHap dfxHap;
    std::shared_ptr<DfxMap> map = nullptr;
    dfxHap.ParseHapFileInfo(0, map, nullptr);
    JsFunction jsFunction;
    dfxHap.ParseHapFileInfo(0, map, &jsFunction);
    auto map1 = std::make_shared<DfxMap>();
    bool ret = dfxHap.ParseHapFileInfo(0, map1, &jsFunction);
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "DfxHapTest005: end.";
}

/**
 * @tc.name: DfxHapTest006
 * @tc.desc: test DfxHap ParseOfflineJsSymbol exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxHapTest, DfxHapTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxHapTest006: start.";
    DfxHap dfxHap;
    pid_t pid = 1;
    uint64_t pc = 1;
    auto map = std::make_shared<DfxMap>();
    JsFunction jsFunction;

    bool res = dfxHap.ParseHapInfo(pid, pc, map, nullptr, true);
    ASSERT_EQ(res, false);
    res = dfxHap.ParseHapInfo(pid, pc, map, &jsFunction, true);
    ASSERT_EQ(res, false);
    map->name = "test.hap";
    res = dfxHap.ParseHapInfo(pid, pc, map, &jsFunction, true);
    ASSERT_EQ(res, false);
    GTEST_LOG_(INFO) << "DfxHapTest006: end.";
}

/**
 * @tc.name: DfxHapTest007
 * @tc.desc: test DfxHap ParseOfflineJsSymbolForArkHap exception
 * @tc.type: FUNC
 */
HWTEST_F(DfxHapTest, DfxHapTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxHapTest007: start.";
    DfxHap dfxHap;
    std::string mapName = "";
    bool ret = dfxHap.ParseHapMemInfoForOffline(mapName, 0, nullptr);
    ASSERT_EQ(ret, false);
    mapName = "test.abc";
    ret = dfxHap.ParseHapMemInfoForOffline(mapName, 0, nullptr);
    ASSERT_EQ(ret, false);
    JsFunction jsFunction;
    ret = dfxHap.ParseHapMemInfoForOffline(mapName, 0, &jsFunction);
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "DfxHapTest007: end.";
}
} // namespace HiviewDFX
} // namespace OHOS