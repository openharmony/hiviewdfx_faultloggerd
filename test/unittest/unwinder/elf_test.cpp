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
#include <ctime>
#include <securec.h>
#include <string>
#include <vector>

#include "dfx_elf.h"
#include "dfx_memory_file.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

static const char DUMPCATCHER_ELF_FILE[] = "/system/bin/dumpcatcher";

namespace OHOS {
namespace HiviewDFX {
class DfxElfTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

/**
 * @tc.name: DfxElfTest001
 * @tc.desc: test DfxElf class functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest001: start.";
    auto memory = DfxMemoryFile::CreateFileMemory(DUMPCATCHER_ELF_FILE, 0);
    if (memory == nullptr) {
        FAIL();
    }
    auto elf = std::make_shared<DfxElf>(memory);
    EXPECT_EQ(true, elf != nullptr) << "DfxElfTest001: failed";
    if (!elf->Init() || !elf->IsValid()) {
        GTEST_LOG_(INFO) << "DfxElfTest001: not a valid elf file.";
        FAIL();
    }
    std::string buildIdHex = elf->GetBuildID();
    if (!buildIdHex.empty()) {
        printf("Build ID hex: ");
        for (size_t i = 0; i < buildIdHex.size(); ++i) {
            printf("%02hhx", buildIdHex[i]);
        }
        printf("\n");
        std::string buildId = DfxElf::GetReadableBuildID(buildIdHex);
        printf("Build ID: %s\n", buildId.c_str());
    }
    GTEST_LOG_(INFO) << "DfxElfTest001: end.";
}
