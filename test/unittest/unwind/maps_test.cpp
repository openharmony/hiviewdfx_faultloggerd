/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "dfx_elf.h"
#include "dfx_maps.h"
#include <gtest/gtest.h>
#include <memory>
#include <sys/types.h>

using namespace OHOS::HiviewDFX;
using namespace std;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
namespace {
static const string INVALID_MAP_ITEM = "/system/lib64/init/libinit_context111111.z.so";

#ifdef __arm__
static const string MAPS_FILE = "/data/test/resource/testdata/testmaps_32";
static const string TEST_MAP_TARGET = "/system/lib/init/libinit_context.z.so";
#else
static const string MAPS_FILE = "/data/test/resource/testdata/testmaps_64";
static const string TEST_MAP_TARGET = "/system/lib64/init/libinit_context.z.so";
#endif
}

class MapsTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() { maps_ = DfxMaps::Create(MAPS_FILE); }
    void TearDown() {}

public:
    shared_ptr<DfxMaps> maps_;
};

namespace {

/**
 * @tc.name: DfxMaps::FindMapByAddrTest001
 * @tc.desc: test exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByAddrTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByAddrTest001: start.";
    auto map = make_shared<DfxMap>();
#ifdef __arm__
    uintptr_t testAddr = 0xf6d80000;
#else
    uintptr_t testAddr = 0x7f8b8f3001;
#endif
    EXPECT_EQ(true, maps_->FindMapByAddr(map, testAddr));
    GTEST_LOG_(INFO) << "FindMapByAddrTest001: end.";
}

/**
 * @tc.name: DfxMaps::FindMapByAddrTest002
 * @tc.desc: test not exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByAddrTest002, TestSize.Level2)
{
    auto map = make_shared<DfxMap>();
    EXPECT_EQ(false, maps_->FindMapByAddr(map, 0xffffffff)); // 0xffffffff : invalid address
    GTEST_LOG_(INFO) << "FindMapByAddrTest002: end.";
}

/**
 * @tc.name: DfxMaps::FindMapByFileInfoTest001
 * @tc.desc: test name exist and offset exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByFileInfoTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest001: start.";
    auto map = make_shared<DfxMap>();
    EXPECT_EQ(true, maps_->FindMapByFileInfo(map, TEST_MAP_TARGET, 0));
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest001: end.";
}

/**
 * @tc.name: DfxMaps::FindMapByFileInfoTest002
 * @tc.desc: test name not exist and offset exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByFileInfoTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest002: start.";
    auto map = make_shared<DfxMap>();
    EXPECT_EQ(false, maps_->FindMapByFileInfo(map, INVALID_MAP_ITEM, 0));
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest002: end.";
}

/**
 * @tc.name: DfxMaps::FindMapByFileInfoTest003
 * @tc.desc: test name exist and offset not exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByFileInfoTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest003: start.";
    auto map = make_shared<DfxMap>();
    EXPECT_EQ(false, maps_->FindMapByFileInfo(map, TEST_MAP_TARGET, 0xffffffff)); // 0xffffffff : invalid offset
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest003: end.";
}

/**
 * @tc.name: DfxMaps::FindMapByFileInfoTest004
 * @tc.desc: test name not exist and offset not exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByFileInfoTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest004: start.";
    auto map = make_shared<DfxMap>();
    EXPECT_EQ(false, maps_->FindMapByFileInfo(map, INVALID_MAP_ITEM, 0xffffffff)); // 0xffffffff : invalid offset
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest004: end.";
}

/**
 * @tc.name: DfxMaps::FindMapsByNameTest001
 * @tc.desc: test exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapsByNameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapsByNameTest001: start.";
    auto mapsV = vector<shared_ptr<DfxMap>>();
    EXPECT_EQ(true, maps_->FindMapsByName(mapsV, TEST_MAP_TARGET));
    GTEST_LOG_(INFO) << "FindMapsByNameTest001: end.";
}

/**
 * @tc.name: DfxMaps::FindMapsByNameTest002
 * @tc.desc: test not exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapsByNameTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapsByNameTest002: start.";
    auto mapsV = vector<shared_ptr<DfxMap>>();
    EXPECT_EQ(false, maps_->FindMapsByName(mapsV, INVALID_MAP_ITEM));
    GTEST_LOG_(INFO) << "FindMapsByNameTest002: end.";
}

/**
 * @tc.name: DfxMap::GetRelPcTest002
 * @tc.desc: test getRelPc no elf
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, GetRelPcTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetRelPcTest002: start.";
#ifdef __arm__
    const string invalidMap =
        "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context.z.so111";
    uint64_t pc = 0xf6d83001;
    const uint64_t invalidOffset = 0x1001;
#else
    const string invalidMap =
        "7f0ab40000-7f0ab41000 r--p 00000000 b3:07 1882 /system/lib64/init/libinit_context11111.z.so";
    uint64_t pc = 0x7f0ab40016;
    const uint64_t invalidOffset = 0x16;
#endif
    shared_ptr<DfxMap> map = DfxMap::Create(invalidMap, sizeof(invalidMap));
    EXPECT_EQ(true, ((map->GetElf() == nullptr) && (map->GetRelPc(pc) == invalidOffset)));
    GTEST_LOG_(INFO) << "GetRelPcTest002: end.";
}

/**
 * @tc.name: DfxMap::ToStringTest001
 * @tc.desc: test getRelPc
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, ToStringTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ToStringTest001: start.";
#ifdef __arm__
    const string testMap = "f6d83000-f6d84000 r--p 00001000 b3:07 1892 /system/lib/init/libinit_context.z.so";
#else
    const string testMap = "7f0ab40000-7f0ab41000 r--p 00000000 b3:07 1882 /system/lib64/init/libinit_context1.z.so";
#endif
    shared_ptr<DfxMap> map = DfxMap::Create(testMap, sizeof(testMap));
    GTEST_LOG_(INFO) << map->ToString();
    EXPECT_EQ(true, sizeof(map->ToString()) != 0);
    GTEST_LOG_(INFO) << "ToStringTest001: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS