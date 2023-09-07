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
    pid_t pid = 1;
    auto dfxMaps = DfxMaps::Create(pid);
    std::vector<std::shared_ptr<DfxMap>> maps;
    std::string elfName = "libc++.so";
    ASSERT_TRUE(dfxMaps->FindMapsByName(maps, elfName));
    auto elfBaseMap = maps[0];
    for (const auto& map : maps) {
        ASSERT_EQ(map->elfBaseMap->name, elfBaseMap->name);
    }

    GTEST_LOG_(INFO) << "DfxMapsTest001: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS
