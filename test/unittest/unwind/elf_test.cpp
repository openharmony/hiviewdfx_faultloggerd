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
#include <iostream>
#include "dfx_elf.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxElfTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

namespace {
/**
 * @tc.name: DfxElfTest001
 * @tc.desc: test DfxElf functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest001: start.";
    //64
    DfxElf elf("/data/dumpcatcher");
    ASSERT_TRUE(elf.IsValid());
    ElfShdr shdr;
    elf.FindSection(shdr, ".eh_frame");
    printf(".eh_frame %x %x %llx\n",shdr.name, shdr.type, shdr.offset);
    elf.FindSection(shdr, ".eh_frame_hdr");
    printf(".eh_frame_hdr %x %x %llx\n",shdr.name, shdr.type, shdr.offset);
    elf.FindSection(shdr, ".dynsym");
    printf(".dynsym %x %x %llx\n",shdr.name, shdr.type, shdr.offset);
    elf.FindSection(shdr, ".symtab");
    printf(".symtab %x %x %llx\n",shdr.name, shdr.type, shdr.offset);
    GTEST_LOG_(INFO) << elf.GetReadableBuildId();
    //32
    DfxElf elf32("/data/processdump");
    ASSERT_TRUE(elf32.IsValid());
    elf32.FindSection(shdr, ".dynsym");
    printf(".dynsym %x %x %llx\n",shdr.name, shdr.type, shdr.offset);
    elf32.FindSection(shdr, ".ARM.extab");
    printf(".ARM.extab %x %x %llx\n",shdr.name, shdr.type, shdr.offset);
    GTEST_LOG_(INFO) << elf.GetReadableBuildId();
    GTEST_LOG_(INFO) << "DfxElfTest001: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS

