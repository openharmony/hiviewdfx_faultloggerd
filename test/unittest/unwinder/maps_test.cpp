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
} // namespace HiviewDFX
} // namespace OHOS

/**
 * @tc.name: DfxMapsTest001
 * @tc.desc: test map Create
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsTest001: start.";
    char mapBuf[] = "7658d38000-7658d40000 rw-p 00000000 00:00 0 /system/lib/libdfx_dumpcatcher.z.so";
    std::shared_ptr<DfxElfMap> map = DfxElfMap::Create(mapBuf, sizeof(mapBuf));
    EXPECT_EQ(true, map != nullptr);
    bool ret = map->IsValidPath();
    EXPECT_EQ(true, ret);
    GTEST_LOG_(INFO) << map->PrintMap();
    GTEST_LOG_(INFO) << "DfxMapsTest001: end.";
}

/**
 * @tc.name: DfxMapsTest002
 * @tc.desc: test maps Create
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsTest002: start.";
    std::shared_ptr<DfxElfMaps> elfMaps = DfxElfMaps::CreateFromLocal();
    EXPECT_EQ(true, elfMaps != nullptr);
    auto maps = elfMaps->GetMaps();
    EXPECT_EQ(true, maps.size() > 0);
    GTEST_LOG_(INFO) << "DfxMapsTest002: end.";
}

/**
 * @tc.name: DfxMapsTest003
 * @tc.desc: test find map by addr
 * @tc.type: FUNC
 */
HWTEST_F (DfxMapsTest, DfxMapsTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMapsTest003: start.";
    std::shared_ptr<DfxElfMaps> maps = DfxElfMaps::CreateFromLocal();
    std::shared_ptr<DfxElfMap> map = std::make_shared<DfxElfMap>();
    uintptr_t address = -1;
    bool flag = maps->FindMapByAddr(address, map);
    EXPECT_EQ(false, flag);
    address = 1;
    flag = maps->FindMapByAddr(address, map);
    EXPECT_EQ(false, flag);
    GTEST_LOG_(INFO) << "DfxMapsTest003: end.";
}
