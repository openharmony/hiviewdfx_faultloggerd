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

#define ELF32_FILE "/data/processdump"
#define ELF64_FILE "/data/dumpcatcher"

namespace OHOS {
namespace HiviewDFX {
class DfxElfTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: DfxElfTest001
 * @tc.desc: test DfxElf functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest001: start.";
    //64
    DfxElf elf(ELF64_FILE);
    bool valid = elf.IsValid();
    printf("valid: %d\n", valid);
    ASSERT_TRUE(valid);
    ShdrInfo shdr;
    bool ret = elf.GetSectionInfo(shdr, ".eh_frame");
    if (ret) {
        printf(".eh_frame %llx %llx %llx\n", shdr.addr, shdr.offset, shdr.size);
    }

    ret = elf.GetSectionInfo(shdr, ".eh_frame_hdr");
    if (ret) {
        printf(".eh_frame_hdr %llx %llx %llx\n", shdr.addr, shdr.offset, shdr.size);
    }

    ret = elf.GetSectionInfo(shdr, ".dynsym");
    if (ret) {
        printf(".dynsym %llx %llx %llx\n", shdr.addr, shdr.offset, shdr.size);
    }

    ret = elf.GetSectionInfo(shdr, ".symtab");
    if (ret) {
        printf(".symtab %llx %llx %llx\n", shdr.addr, shdr.offset, shdr.size);
    }
    GTEST_LOG_(INFO) << elf.GetBuildId();
    //32
    DfxElf elf32(ELF32_FILE);
    valid = elf32.IsValid();
    printf("valid: %d\n", valid);
    ASSERT_TRUE(valid);
    ret = elf32.GetSectionInfo(shdr, ".dynsym");
    if (ret) {
        printf(".dynsym %llx %llx %llx\n", shdr.addr, shdr.offset, shdr.size);
    }
    ret = elf32.GetSectionInfo(shdr, ".ARM.extab");
    if (ret) {
        printf(".ARM.extab %llx %llx %llx\n", shdr.addr, shdr.offset, shdr.size);
    }
    GTEST_LOG_(INFO) << elf32.GetBuildId();
    GTEST_LOG_(INFO) << "DfxElfTest001: end.";
}

/**
 * @tc.name: DfxElfTest002
 * @tc.desc: test DfxElf functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest002: start.";
    //64
    DfxElf elf(ELF64_FILE);
    bool valid = elf.IsValid();
    ASSERT_TRUE(valid);
    std::vector<ElfSymbol> elfSymbols = elf.GetElfSymbols();
    ASSERT_TRUE(elfSymbols.size() > 0);
    printf("Value   Size    Type    Bind    Vis     Ndx     Name\n");
    for (auto sym : elfSymbols) {
        printf("%llx %llx %d %d %d %s\n", sym.value, sym.size, sym.info, sym.other, sym.shndx, sym.nameStr.c_str());
    }
    printf("\n");

    //32
    DfxElf elf32(ELF32_FILE);
    valid = elf32.IsValid();
    ASSERT_TRUE(valid);
    elfSymbols.clear();
    elfSymbols = elf32.GetElfSymbols();
    ASSERT_TRUE(elfSymbols.size() > 0);
    printf("Value   Size    Type    Bind    Vis     Ndx     Name\n");
    for (auto sym : elfSymbols) {
        printf("%llx %llx %d %d %d %s\n", sym.value, sym.size, sym.info, sym.other, sym.shndx, sym.nameStr.c_str());
    }
    printf("\n");
    GTEST_LOG_(INFO) << "DfxElfTest002: end.";
}
} // namespace HiviewDFX
} // namespace OHOS

