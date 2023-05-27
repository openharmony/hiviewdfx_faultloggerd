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

#include "dfx_memory_file.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

static const char DUMPCATCHER_ELF_FILE[] = "/system/bin/dumpcatcher";

namespace OHOS {
namespace HiviewDFX {
class DfxMemoryTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

/**
 * @tc.name: DfxMemoryTest001
 * @tc.desc: test DfxMemory class CreateFileMemory
 * @tc.type: FUNC
 */
HWTEST_F(DfxMemoryTest, DfxMemoryTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxMemoryTest001: start.";
    auto memory = DfxMemoryFile::CreateFileMemory(DUMPCATCHER_ELF_FILE, 0);
    if (memory == nullptr) {
        GTEST_LOG_(INFO) << "DfxMemoryTest001: create memory failed.";
        FAIL();
    }
    int tmp;
    bool ret = memory->ReadFully(0, &tmp, sizeof(tmp));
    EXPECT_EQ(true, ret) << "DfxMemoryTest001: failed";
    GTEST_LOG_(INFO) << "DfxMemoryTest001: end.";
}
