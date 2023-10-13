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
#include "elf_imitate.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

#define ELF32_FILE "/data/test/resource/testdata/elf32_test"
#define ELF64_FILE "/data/test/resource/testdata/elf_test"
#define PT_LOAD_OFFSET 0x001000
#define PT_LOAD_OFFSET64 0x0000000000002000
namespace OHOS {
namespace HiviewDFX {
class DfxElfTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

std::vector<std::string> interestedSections = { ".dynsym", ".eh_frame_hdr", ".eh_frame", ".symtab" };
 /**
 * @tc.name: DfxElfTest001
 * @tc.desc: test DfxElf functions with 32 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest001: start.";
    DfxElf elf(ELF32_FILE);
    ASSERT_TRUE(elf.IsValid());
    ElfImitate elfImitate;
    ShdrInfo shdr;
    ShdrInfo shdrImitate;
    elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF32);
    for (size_t i = 0; i < interestedSections.size(); i++) {
        elf.GetSectionInfo(shdr, interestedSections[i]);
        elfImitate.GetSectionInfo(shdrImitate, interestedSections[i]);
        GTEST_LOG_(INFO) << interestedSections[i];
        ASSERT_EQ(shdr.addr, shdrImitate.addr);
        ASSERT_EQ(shdr.offset, shdrImitate.offset);
        ASSERT_EQ(shdr.size, shdrImitate.size);
    }
    ASSERT_EQ(elf.GetArchType(), elfImitate.GetArchType());
    ASSERT_EQ(elf.GetElfSize(), elfImitate.GetElfSize());
    ASSERT_EQ(elf.GetLoadBias(), elfImitate.GetLoadBias());

    auto load = elf.GetPtLoads();
    auto loadImitate = elfImitate.GetPtLoads();
    ASSERT_EQ(load[PT_LOAD_OFFSET].offset, loadImitate[PT_LOAD_OFFSET].offset);
    ASSERT_EQ(load[PT_LOAD_OFFSET].tableSize, loadImitate[PT_LOAD_OFFSET].tableSize);
    ASSERT_EQ(load[PT_LOAD_OFFSET].tableVaddr, loadImitate[PT_LOAD_OFFSET].tableVaddr);

    ASSERT_EQ(elf.GetClassType(), elfImitate.GetClassType());
    ASSERT_EQ(elf.GetLoadBase(0xf78c0000, 0), elfImitate.GetLoadBase(0xf78c0000, 0));
    ASSERT_EQ(elf.GetStartPc(), elfImitate.GetStartPc());
    ASSERT_EQ(elf.GetEndPc(), elfImitate.GetEndPc());
    ASSERT_EQ(elf.GetRelPc(0xf78c00f0, 0xf78c0000, 0), elfImitate.GetRelPc(0xf78c00f0, 0xf78c0000, 0));
    ASSERT_EQ(elf.GetBuildId(), "8e5a30338be326934ff93c998dcd0d22fe345870");
    GTEST_LOG_(INFO) << "DfxElfTest001: end.";
}

/**
 * @tc.name: DfxElfTest002
 * @tc.desc: test DfxElf functions with 64 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest002: start.";
    DfxElf elf(ELF64_FILE);
    ASSERT_TRUE(elf.IsValid());
    ElfImitate elfImitate;
    ShdrInfo shdr;
    ShdrInfo shdrImitate;
    elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF64);
    for (size_t i = 0; i < interestedSections.size(); i++) {
        GTEST_LOG_(INFO) << interestedSections[i];
        elf.GetSectionInfo(shdr, interestedSections[i]);
        elfImitate.GetSectionInfo(shdrImitate, interestedSections[i]);
        ASSERT_EQ(shdr.addr, shdrImitate.addr);
        ASSERT_EQ(shdr.offset, shdrImitate.offset);
        ASSERT_EQ(shdr.size, shdrImitate.size);
    }
    ASSERT_EQ(elf.GetArchType(), elfImitate.GetArchType());
    ASSERT_EQ(elf.GetElfSize(), elfImitate.GetElfSize());
    ASSERT_EQ(elf.GetLoadBias(), elfImitate.GetLoadBias());

    auto load = elf.GetPtLoads();
    auto loadImitate = elfImitate.GetPtLoads();
    ASSERT_EQ(load[PT_LOAD_OFFSET64].offset, loadImitate[PT_LOAD_OFFSET64].offset);
    ASSERT_EQ(load[PT_LOAD_OFFSET64].tableSize, loadImitate[PT_LOAD_OFFSET64].tableSize);
    ASSERT_EQ(load[PT_LOAD_OFFSET64].tableVaddr, loadImitate[PT_LOAD_OFFSET64].tableVaddr);

    ASSERT_EQ(elf.GetClassType(), elfImitate.GetClassType());
    ASSERT_EQ(elf.GetLoadBase(0xf78c0000, 0), elfImitate.GetLoadBase(0xf78c0000, 0));
    ASSERT_EQ(elf.GetStartPc(), elfImitate.GetStartPc());
    ASSERT_EQ(elf.GetEndPc(), elfImitate.GetEndPc());
    ASSERT_EQ(elf.GetRelPc(0xf78c00f0, 0xf78c0000, 0), elfImitate.GetRelPc(0xf78c00f0, 0xf78c0000, 0));
    ASSERT_EQ(elf.GetBuildId(), "24c55dccc5baaaa140da0083207abcb8d523e248");
    GTEST_LOG_(INFO) << "DfxElfTest002: end.";
}

} // namespace HiviewDFX
} // namespace OHOS

