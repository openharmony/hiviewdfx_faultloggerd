/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>
#include <ctime>
#include <string>
#include <vector>
#include "dfx_elf.h"
#include "dfx_xz_utils.h"
#include "unwinder_config.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

#define DUMPCATCHER_ELF_FILE "/system/bin/dumpcatcher"

namespace OHOS {
namespace HiviewDFX {

class DfxXzUtilTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDonw() {}
};

namespace {
/**
 * @tc.name: DfxXzUtilTest001
 * @tc.desc: test XzUtil functions
 * @tc.type: FUNC
 */
#if !is_emulator
HWTEST_F(DfxXzUtilTest, DfxXzUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxXzUtilTest001: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    DfxElf elf(DUMPCATCHER_ELF_FILE);
    ASSERT_TRUE(elf.IsValid());
    auto minidebugInfo = elf.GetMiniDebugInfo();
    ASSERT_TRUE(minidebugInfo != nullptr);
    uint8_t *addr = minidebugInfo->offset + const_cast<uint8_t*>(elf.GetMmapPtr());
    std::shared_ptr<std::vector<uint8_t>> decompressedElf = std::make_shared<std::vector<uint8_t>>();;
    ASSERT_TRUE(XzDecompress(addr, minidebugInfo->size, decompressedElf));
    ASSERT_TRUE(decompressedElf->size() > minidebugInfo->size);
    GTEST_LOG_(INFO) << "DfxXzUtilTest001: end.";
}
#endif
}
} // namespace HiviewDFX
} // namespace OHOS