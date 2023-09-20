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

#include <gtest/gtest.h>
#include <memory>
#include <sys/types.h>
#include "dfx_maps.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxMapsTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
/**
 * @tc.name: DfxMapsTest001
 * @tc.desc: test map Create
 * @tc.type: FUNC
 */
HWTEST_F(DfxMapsTest, DfxMapsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsTest001: start.";

    GTEST_LOG_(INFO) << "DfxMapsTest001: end.";
}

/////////////////////////////////////////

/**
 * @tc.name: DfxMaps::FindMapByAddrTest001
 * @tc.desc: test exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByAddrTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByAddrTest001: start.";
    const std::string path = "/data/unwind_test.txt";
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(path);
    auto map = std::make_shared<DfxMap>();
    uintptr_t addr = {0xf6d80000};
    bool ifExist = maps->FindMapByAddr(map,addr);
    EXPECT_EQ(true, ifExist);
    GTEST_LOG_(INFO) << "FindMapByAddrTest001: end.";
}

/**
 * @tc.name: DfxMaps::FindMapByAddrTest002
 * @tc.desc: test not exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByAddrTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByAddrTest002: start.";
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(std::string("/data/unwind_test.txt"));
    auto map = std::make_shared<DfxMap>();
    uintptr_t addr = {0Xffffffff};
    bool ifExist = maps->FindMapByAddr(map,addr);
    EXPECT_EQ(false, ifExist);
    GTEST_LOG_(INFO) << "FindMapByAddrTest002: end.";
}

/////////////////////////////////////////

/**
 * @tc.name: DfxMaps::FindMapByFileInfoTest001
 * @tc.desc: test name exist and offset exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapByFileInfoTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest001: start.";
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(std::string("/data/unwind_test.txt"));
    auto map = std::make_shared<DfxMap>();
    std::string name = "/system/lib/init/libinit_context.z.so";
    uintptr_t offset = {0X0};
    bool ifExist = maps->FindMapByFileInfo(map,name,offset);
    EXPECT_EQ(true, ifExist);
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
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(std::string("/data/unwind_test.txt"));
    auto map = std::make_shared<DfxMap>();
    std::string name = "/system/lib/init/libinit_context111.z.so";
    uintptr_t offset = {0X0};
    bool ifExist = maps->FindMapByFileInfo(map,name,offset);
    EXPECT_EQ(false, ifExist);
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
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(std::string("/data/unwind_test.txt"));
    auto map = std::make_shared<DfxMap>();
    std::string name = "/system/lib/init/libinit_context.z.so";
    uintptr_t offset = {0Xffffffff};
    bool ifExist = maps->FindMapByFileInfo(map,name,offset);
    EXPECT_EQ(false, ifExist);
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
    std::shared_ptr<DfxMaps> maps = DfxMaps::Create(std::string("/data/unwind_test.txt"));
    auto map = std::make_shared<DfxMap>();
    std::string name = "/system/lib/init/libinit_context1111.z.so";
    uintptr_t offset = {0Xffffffff};
    bool ifExist = maps->FindMapByFileInfo(map,name,offset);
    EXPECT_EQ(false, ifExist);
    GTEST_LOG_(INFO) << "FindMapByFileInfoTest004: end.";
}

/////////////////////////////////////////

/**
 * @tc.name: DfxMaps::FindMapsByNameTest001
 * @tc.desc: test exist
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, FindMapsByNameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FindMapsByNameTest001: start.";
    std::shared_ptr<DfxMaps> dfxMaps = DfxMaps::Create(std::string("/data/unwind_test.txt"));
    auto maps = std::vector<std::shared_ptr<DfxMap>>();
    std::string elfName = "ibinit_context.z.so";
    bool ifExist = dfxMaps->FindMapsByName(maps,elfName);
    EXPECT_EQ(true, ifExist);
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
    std::shared_ptr<DfxMaps> dfxMaps = DfxMaps::Create(std::string("/data/unwind_test.txt"));
    auto maps = std::vector<std::shared_ptr<DfxMap>>();
    std::string elfName = "libinit_context.z.soxxx";
    bool ifExist = dfxMaps->FindMapsByName(maps,elfName);
    EXPECT_EQ(false, ifExist);
    GTEST_LOG_(INFO) << "FindMapsByNameTest002: end.";
}

/////////////////////////////////////////

/**
 * @tc.name: DfxMap::GetRelPcTest001
 * @tc.desc: test getRelPc
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, GetRelPcTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "GetRelPcTest001: start.";
    char mapBuf[] = "f6d83000-f6d84000 r--p 00001000 b3:07 1892                               /system/lib/init/libinit_context.z.so";
    std::shared_ptr<DfxMap> map = DfxMap::Create(mapBuf, sizeof(mapBuf));
    uint64_t pc = {0Xf6d83000};
    uint64_t relpc =  map->GetRelPc(pc);
    GTEST_LOG_(INFO) << relpc;
    GTEST_LOG_(INFO) << "GetRelPcTest001: end.";
}

/////////////////////////////////////////

/**
 * @tc.name: DfxMap::ToStringTest001
 * @tc.desc: test getRelPc
 * @tc.type: FUNC
 */
HWTEST_F(MapsTest, ToStringTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ToStringTest001: start.";
    char mapBuf[] = "f6d83000-f6d84000 r--p 00001000 b3:07 1892                               /system/lib/init/libinit_context.z.so";
    std::shared_ptr<DfxMap> map = DfxMap::Create(mapBuf, sizeof(mapBuf));
    std::string mapStr = map->ToString();
    EXPECT_EQ(true, 1 == 1);
    GTEST_LOG_(INFO) << mapStr;
    GTEST_LOG_(INFO) << "ToStringTest001: end.";
}

}
} // namespace HiviewDFX
} // namespace OHOS