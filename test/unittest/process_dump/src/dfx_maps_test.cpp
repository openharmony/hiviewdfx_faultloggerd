/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files is process dump map module unittest. */

#include "dfx_maps_test.h"

#include <memory>
#include <sys/types.h>
#include "dfx_maps.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

void DfxMapsTest::SetUpTestCase(void)
{
}

void DfxMapsTest::TearDownTestCase(void)
{
}

void DfxMapsTest::SetUp(void)
{
}

void DfxMapsTest::TearDown(void)
{
}

/**
 * @tc.name: DfxMapsRequestTest001
 * @tc.desc: test get begin
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest001: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = 1;
    uint64_t output = 0;
    dfxmap->SetMapBegin(input);
    output = dfxmap->GetMapBegin();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest001 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest001: end.";
}

/**
 * @tc.name: DfxMapsRequestTest002
 * @tc.desc: test get begin
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest002: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = 165;
    uint64_t output = 0;
    dfxmap->SetMapBegin(input);
    output = dfxmap->GetMapBegin();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest002 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest002: end.";
}

/**
 * @tc.name: DfxMapsRequestTest003
 * @tc.desc: test get begin
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest003: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = -1;
    uint64_t output = 0;
    dfxmap->SetMapBegin(input);
    output = dfxmap->GetMapBegin();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest003 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest003: end.";
}

/**
 * @tc.name: DfxMapsRequestTest004
 * @tc.desc: test get begin
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest004: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = 1;
    uint64_t output = 0;
    dfxmap->SetMapEnd(input);
    output = dfxmap->GetMapEnd();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest004 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest004: end.";
}

/**
 * @tc.name: DfxMapsRequestTest005
 * @tc.desc: test get end
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest006: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = 165;
    uint64_t output = 0;
    dfxmap->SetMapEnd(input);
    output = dfxmap->GetMapEnd();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest005 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest005: end.";
}

/**
 * @tc.name: DfxMapsRequestTest006
 * @tc.desc: test get end
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest006: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = -1;
    uint64_t output = 0;
    dfxmap->SetMapEnd(input);
    output = dfxmap->GetMapEnd();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest006 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest006: end.";
}

/**
 * @tc.name: DfxMapsRequestTest007
 * @tc.desc: test get offect
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest007: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = 1;
    uint64_t output = 0;
    dfxmap->SetMapOffset(input);
    output = dfxmap->GetMapOffset();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest007 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest007: end.";
}

/**
 * @tc.name: DfxMapsRequestTest008
 * @tc.desc: test get offect
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest008: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = 165;
    uint64_t output = 0;
    dfxmap->SetMapOffset(input);
    output = dfxmap->GetMapOffset();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest008 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest008: end.";
}

/**
 * @tc.name: DfxMapsRequestTest009
 * @tc.desc: test get offect
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest009: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    uint64_t input = -1;
    uint64_t output = 0;
    dfxmap->SetMapOffset(input);
    output = dfxmap->GetMapOffset();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest009 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest009: end.";
}

/**
 * @tc.name: DfxMapsRequestTest010
 * @tc.desc: test get perms
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest015: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    std::string input = "11111";
    std::string output = "0000";
    dfxmap->SetMapPerms(input, sizeof(input));
    output = dfxmap->GetMapPerms();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest010 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest010: end.";
}

/**
 * @tc.name: DfxMapsRequestTest010
 * @tc.desc: test get perms
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest011: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    std::string input = "24861";
    std::string output = "245154";
    dfxmap->SetMapPerms(input, sizeof(input));
    output = dfxmap->GetMapPerms();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest011 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest011: end.";
}

/**
 * @tc.name: DfxMapsRequestTest012
 * @tc.desc: test get perms
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest012: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    std::string input = "";
    std::string output = "1111";
    dfxmap->SetMapPerms(input, sizeof(input));
    output = dfxmap->GetMapPerms();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest012 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest012: end.";
}

/**
 * @tc.name: DfxMapsRequestTest013
 * @tc.desc: test get path
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest013: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    std::string input = "1";
    std::string output = "2";
    dfxmap->SetMapPath(input);
    output = dfxmap->GetMapPath();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest013 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest013: end.";
}

/**
 * @tc.name: DfxMapsRequestTest014
 * @tc.desc: test get path
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest014: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    std::string input = "165";
    std::string output = "156";
    dfxmap->SetMapPath(input);
    output = dfxmap->GetMapPath();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest014 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest014: end.";
}

/**
 * @tc.name: DfxMapsRequestTest015
 * @tc.desc: test get path
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest015: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    std::string input = "";
    std::string output = "  ";
    dfxmap->SetMapPath(input);
    output = dfxmap->GetMapPath();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest015 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest015: end.";
}

/**
 * @tc.name: DfxMapsRequestTest016
 * @tc.desc: test get image
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest016: start.";
    std::shared_ptr<DfxElfMap> dfxmap = std::make_shared<DfxElfMap>();
    std::shared_ptr<DfxElf> input;
    std::shared_ptr<DfxElf> output;
    dfxmap->SetMapImage(input);
    output = dfxmap->GetMapImage();
    EXPECT_EQ(true, input == output) << "DfxMapsRequestTest016 Failed";
    GTEST_LOG_(INFO) << "DfxMapsRequestTest016: end.";
}

/**
 * @tc.name: DfxMapsRequestTest017
 * @tc.desc: test find map by path
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest017: start.";
    std::shared_ptr<DfxElfMaps> dfxmap = std::make_shared<DfxElfMaps>();
    const std::string path = "/data";
    std::vector<std::shared_ptr<DfxElfMap>> maps;
    bool flag = dfxmap->FindMapByPath(path, maps);
    EXPECT_EQ(false, flag);
    GTEST_LOG_(INFO) << "DfxMapsRequestTest017: end.";
}

/**
 * @tc.name: DfxMapsRequestTest018
 * @tc.desc: test find map by addr
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest018: start.";
    std::shared_ptr<DfxElfMaps> dfxmap = std::make_shared<DfxElfMaps>();
    std::shared_ptr<DfxElfMap> map = std::make_shared<DfxElfMap>();
    uintptr_t address = 1;
    bool flag = dfxmap->FindMapByAddr(address, map);
    EXPECT_EQ(false, flag);
    GTEST_LOG_(INFO) << "DfxMapsRequestTest018: end.";
}

/**
 * @tc.name: DfxMapsRequestTest019
 * @tc.desc: test find map by addr
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest019: start.";
    std::shared_ptr<DfxElfMaps> dfxmap = std::make_shared<DfxElfMaps>();
    std::shared_ptr<DfxElfMap> map = std::make_shared<DfxElfMap>();
    uintptr_t address = 100;
    bool flag = dfxmap->FindMapByAddr(address, map);
    EXPECT_EQ(false, flag);
    GTEST_LOG_(INFO) << "DfxMapsRequestTest019: end.";
}

/**
 * @tc.name: DfxMapsRequestTest020
 * @tc.desc: test find map by addr
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsRequestTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsRequestTest020: start.";
    std::shared_ptr<DfxElfMaps> dfxmap = std::make_shared<DfxElfMaps>();
    std::shared_ptr<DfxElfMap> map = std::make_shared<DfxElfMap>();
    uintptr_t address = -1;
    bool flag = dfxmap->FindMapByAddr(address, map);
    EXPECT_EQ(false, flag);
    GTEST_LOG_(INFO) << "DfxMapsRequestTest020: end.";
}
