/*
 * Copyright (c) 2022-2025 Huawei Device Co., Ltd.
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
#include "elf_factory.h"
#include "unwinder_config.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

#define ELF32_FILE "/data/test/resource/testdata/elf32_test"
#define ELF64_FILE "/data/test/resource/testdata/elf_test"
#define DUMPCATCHER_ELF_FILE "/system/bin/dumpcatcher"
#define PT_LOAD_OFFSET 0x001000
#define PT_LOAD_OFFSET64 0x0000000000002000

namespace OHOS {
namespace HiviewDFX {
class DfxElfTest : public testing::Test {};

std::vector<std::string> interestedSections = { ".dynsym", ".eh_frame_hdr", ".eh_frame", ".symtab" };
 /**
 * @tc.name: DfxElfTest001
 * @tc.desc: test DfxElf functions with 32 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest001: start.";
    RegularElfFactory factory(ELF32_FILE);
    auto elf = factory.Create();
    ASSERT_TRUE(elf != nullptr);
    ElfImitate elfImitate;
    ShdrInfo shdr;
    ShdrInfo shdrImitate;
    bool ret = elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF32);
    ASSERT_TRUE(ret);
    for (size_t i = 0; i < interestedSections.size(); i++) {
        elf->GetSectionInfo(shdr, interestedSections[i]);
        elfImitate.GetSectionInfo(shdrImitate, interestedSections[i]);
        GTEST_LOG_(INFO) << interestedSections[i];
        ASSERT_EQ(shdr.addr, shdrImitate.addr);
        ASSERT_EQ(shdr.offset, shdrImitate.offset);
        ASSERT_EQ(shdr.size, shdrImitate.size);
        std::vector<uint8_t> sectionBuffer(shdr.size);
        ASSERT_TRUE(elf->GetSectionData(sectionBuffer.data(), shdr.size, interestedSections[i]));
    }
    ASSERT_EQ(elf->GetArchType(), elfImitate.GetArchType());
    ASSERT_EQ(elf->GetElfSize(), elfImitate.GetElfSize());
    ASSERT_EQ(elf->GetLoadBias(), elfImitate.GetLoadBias());

    auto load = elf->GetPtLoads();
    auto loadImitate = elfImitate.GetPtLoads();
    ASSERT_EQ(load[PT_LOAD_OFFSET].offset, loadImitate[PT_LOAD_OFFSET].offset);
    ASSERT_EQ(load[PT_LOAD_OFFSET].tableSize, loadImitate[PT_LOAD_OFFSET].tableSize);
    ASSERT_EQ(load[PT_LOAD_OFFSET].tableVaddr, loadImitate[PT_LOAD_OFFSET].tableVaddr);

    ASSERT_EQ(elf->GetClassType(), elfImitate.GetClassType());
    ASSERT_EQ(elf->GetLoadBase(0xf78c0000, 0), elfImitate.GetLoadBase(0xf78c0000, 0));
    ASSERT_EQ(elf->GetStartPc(), elfImitate.GetStartPc());
    ASSERT_EQ(elf->GetEndPc(), elfImitate.GetEndPc());
    ASSERT_EQ(elf->GetRelPc(0xf78c00f0, 0xf78c0000, 0), elfImitate.GetRelPc(0xf78c00f0, 0xf78c0000, 0));
    ASSERT_EQ(elf->GetBuildId(), "8e5a30338be326934ff93c998dcd0d22fe345870");
    EXPECT_NE(elf->GetGlobalPointer(), 0);
    EXPECT_FALSE(elf->GetFuncSymbols().empty());
    EXPECT_GT(elf->GetMmapSize(), 0);
    GTEST_LOG_(INFO) << elf->GetElfName();
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
    RegularElfFactory factory(ELF64_FILE);
    auto elf = factory.Create();
    ASSERT_TRUE(elf != nullptr);
    ElfImitate elfImitate;
    ShdrInfo shdr;
    ShdrInfo shdrImitate;
    bool ret = elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF64);
    ASSERT_TRUE(ret);
    for (size_t i = 0; i < interestedSections.size(); i++) {
        GTEST_LOG_(INFO) << interestedSections[i];
        elf->GetSectionInfo(shdr, interestedSections[i]);
        elfImitate.GetSectionInfo(shdrImitate, interestedSections[i]);
        ASSERT_EQ(shdr.addr, shdrImitate.addr);
        ASSERT_EQ(shdr.offset, shdrImitate.offset);
        ASSERT_EQ(shdr.size, shdrImitate.size);
        std::vector<uint8_t> sectionBuffer(shdr.size);
        ASSERT_TRUE(elf->GetSectionData(sectionBuffer.data(), shdr.size, interestedSections[i]));
        ASSERT_FALSE(elf->GetSectionData(sectionBuffer.data(), shdr.size, "invalid section name"));
    }
    ASSERT_EQ(elf->GetArchType(), elfImitate.GetArchType());
    ASSERT_EQ(elf->GetElfSize(), elfImitate.GetElfSize());
    ASSERT_EQ(elf->GetLoadBias(), elfImitate.GetLoadBias());

    auto load = elf->GetPtLoads();
    auto loadImitate = elfImitate.GetPtLoads();
    ASSERT_EQ(load[PT_LOAD_OFFSET64].offset, loadImitate[PT_LOAD_OFFSET64].offset);
    ASSERT_EQ(load[PT_LOAD_OFFSET64].tableSize, loadImitate[PT_LOAD_OFFSET64].tableSize);
    ASSERT_EQ(load[PT_LOAD_OFFSET64].tableVaddr, loadImitate[PT_LOAD_OFFSET64].tableVaddr);

    ASSERT_EQ(elf->GetClassType(), elfImitate.GetClassType());
    ASSERT_EQ(elf->GetLoadBase(0xf78c0000, 0), elfImitate.GetLoadBase(0xf78c0000, 0));
    ASSERT_EQ(elf->GetStartPc(), elfImitate.GetStartPc());
    ASSERT_EQ(elf->GetEndPc(), elfImitate.GetEndPc());
    ASSERT_EQ(elf->GetRelPc(0xf78c00f0, 0xf78c0000, 0), elfImitate.GetRelPc(0xf78c00f0, 0xf78c0000, 0));
    ASSERT_EQ(elf->GetBuildId(), "24c55dccc5baaaa140da0083207abcb8d523e248");
    EXPECT_NE(elf->GetGlobalPointer(), 0);
    EXPECT_FALSE(elf->GetFuncSymbols().empty());
    EXPECT_GT(elf->GetMmapSize(), 0);
    GTEST_LOG_(INFO) << elf->GetElfName();
    GTEST_LOG_(INFO) << "DfxElfTest002: end.";
}

/**
 * @tc.name: DfxElfTestAdltStrTabSection
 * @tc.desc: test DfxElf adlt.strtab section
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTestAdltStrTabSection, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTestAdltStrTabSection: start.";
    std::string filePath = "/data/test/resource/testdata/libadlt_test";
    RegularElfFactory factory(filePath);
    auto elf = factory.Create();
    ASSERT_TRUE(elf != nullptr);
    GTEST_LOG_(INFO) << elf->GetElfName();
    ASSERT_TRUE(elf->IsValid());
    ASSERT_TRUE(elf->IsAdlt());
 
    // check .adlt.strtab section
    std::string strTab = elf->GetAdltStrtab();
    EXPECT_EQ(strTab.size(), 0x67);
    ShdrInfo shdr;
    std::string secName = ".adlt.strtab";
    GTEST_LOG_(INFO) << secName;
    ASSERT_TRUE(elf->GetSectionInfo(shdr, secName));
    EXPECT_EQ(shdr.size, strTab.size());
 
    GTEST_LOG_(INFO) << "DfxElfTestAdltStrTabSection: end.";
}

/**
 * @tc.name: DfxElfTestAdltMapSection
 * @tc.desc: test DfxElf adlt.map section
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTestAdltMapSection, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTestAdltMapSection: start.";
    std::string filePath = "/data/test/resource/testdata/libadlt_test";
    RegularElfFactory factory(filePath);
    auto elf = factory.Create();
    ASSERT_TRUE(elf != nullptr);
    GTEST_LOG_(INFO) << elf->GetElfName();
    ASSERT_TRUE(elf->IsValid());
    ASSERT_TRUE(elf->IsAdlt());

    // check .adlt.map section
    std::vector<AdltMapInfo> adltMap = elf->GetAdltMap();
    std::string secName = ".adlt.map";
    GTEST_LOG_(INFO) << secName;
    ShdrInfo shdr;
    ASSERT_TRUE(elf->GetSectionInfo(shdr, secName));
    EXPECT_GT(shdr.size, 0);
    EXPECT_EQ(shdr.size, adltMap.size() * sizeof(AdltMapInfo));
    EXPECT_EQ(shdr.size % sizeof(AdltMapInfo), 0);
    std::vector<uint8_t> buf(shdr.size);
    ASSERT_TRUE(elf->GetSectionData(buf.data(), shdr.size, secName));
    EXPECT_EQ(shdr.size, buf.size());

    GTEST_LOG_(INFO) << "DfxElfTestAdltMapSection: end.";
}

/**
 * @tc.name: DfxElfTestAdltGetOriginSoFunc
 * @tc.desc: test DfxElf Get Origin So Func
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTestAdltGetOriginSoFunc, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTestAdltGetOriginSoFunc: start.";
    std::string filePath = "/data/test/resource/testdata/libadlt_test";
    RegularElfFactory factory(filePath);
    auto elf = factory.Create();
    ASSERT_TRUE(elf != nullptr);
    GTEST_LOG_(INFO) << elf->GetElfName();
    ASSERT_TRUE(elf->IsValid());
 
    // get .adlt.map section
    std::string secName = ".adlt.map";
    ShdrInfo shdr;
    ASSERT_TRUE(elf->GetSectionInfo(shdr, secName));
    EXPECT_GT(shdr.size, 0);
    std::vector<uint8_t> buf(shdr.size);
    ASSERT_TRUE(elf->GetSectionData(buf.data(), shdr.size, secName));
    EXPECT_EQ(shdr.size, buf.size());
 
    // check func: GetAdltOriginSoNameByRelPc
    // legal relPc
    AdltMapInfo* mapInfo = reinterpret_cast<AdltMapInfo*>(buf.data());
    uint32_t relPc = mapInfo[0].pcBegin;
    std::string originSoName = elf->GetAdltOriginSoNameByRelPc(relPc);
    EXPECT_GT(originSoName.size(), 0);
    // illegal relPc
    relPc = mapInfo[0].pcEnd;
    originSoName = elf->GetAdltOriginSoNameByRelPc(relPc);
    ASSERT_TRUE(originSoName.empty());
 
    GTEST_LOG_(INFO) << "DfxElfTestAdltGetOriginSoFunc: end.";
}

/**
 * @tc.name: DfxElfTest003
 * @tc.desc: test DfxElf functions with using error
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest003: start.";
    DfxElf elf(nullptr);
    ASSERT_FALSE(elf.IsValid());
    ASSERT_EQ(elf.GetClassType(), ELFCLASSNONE);
    ASSERT_EQ(elf.GetElfSize(), 0);
    ASSERT_TRUE(elf.GetBuildId().empty());
    EXPECT_EQ(elf.GetGlobalPointer(), 0);
    ShdrInfo shdrInfo;
    EXPECT_FALSE(elf.GetSectionInfo(shdrInfo, ""));
    EXPECT_EQ(elf.GetMmapSize(), 0);
    GTEST_LOG_(INFO) << "DfxElfTest003: end.";
}

#if !is_emulator
/**
 * @tc.name: DfxElfTest004
 * @tc.desc: test DfxElf class functions with minidebugInfo
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest004: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    RegularElfFactory factory(DUMPCATCHER_ELF_FILE);
    auto elf = factory.Create();
    ASSERT_TRUE(elf->IsValid());
    auto symbols = elf->GetFuncSymbols();
    GTEST_LOG_(INFO) << "DfxElfTest004: symbols1 size:" << symbols.size();

    UnwinderConfig::SetEnableMiniDebugInfo(false);
    auto regularElf = factory.Create();
    ASSERT_TRUE(regularElf->IsValid());
    auto regularSymbols = regularElf->GetFuncSymbols();
    GTEST_LOG_(INFO) << "DfxElfTest004: symbols2 size:" << regularSymbols.size();
    ASSERT_GE(regularSymbols.size(), 0);
    ASSERT_GE(symbols.size(), regularSymbols.size());
    GTEST_LOG_(INFO) << "DfxElfTest004: end.";
}
#endif

/**
 * @tc.name: DfxElfTest005
 * @tc.desc: test GetBuildId function when input empty elf file path
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest005, TestSize.Level2)
{
    auto elfFile = std::make_shared<DfxElf>(nullptr);
    ASSERT_NE(elfFile, nullptr);
    EXPECT_STREQ(elfFile->GetBuildId().c_str(), "");
}

/**
 * @tc.name: DfxElfTest006
 * @tc.desc: test SetBaseOffset function and GetBaseOffset function
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest006, TestSize.Level2)
{
    auto elf = std::make_shared<DfxElf>(nullptr);
    ASSERT_EQ(elf->GetBaseOffset(), 0);
    elf->SetBaseOffset(1);
    ASSERT_EQ(elf->GetBaseOffset(), 1);
}

/**
 * @tc.name: DfxElfTest007
 * @tc.desc: test DfxMmap abnormal case
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest007, TestSize.Level2)
{
    auto mMap = std::make_shared<DfxMmap>();
    ASSERT_FALSE(mMap->Init(-1, 0U, 0));
    uintptr_t invalidAddr = 0;
    ASSERT_EQ(mMap->Read(invalidAddr, nullptr, 0U, true), 0);
}

/**
 * @tc.name: DfxElfTest008
 * @tc.desc: test scenario function
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest008, TestSize.Level2)
{
    auto elfParse = std::unique_ptr<ElfParser>(std::make_unique<ElfParser32>(nullptr));
    std::string str = "";
    elfParse->GetSectionNameByIndex(str, 0);
    elfParse->sectionNames_ = "test";
    const size_t num = 10;
    elfParse->GetSectionNameByIndex(str, num);
    unsigned char buf[num] = {0};
    elfParse->GetSectionData(buf, num, str);
    std::string buildId = elfParse->ToReadableBuildId(str);
    elfParse->GetElfName();
    uint64_t addr = 0;
    ElfSymbol elfSymbol;
    elfParse->GetFuncSymbolByAddr(addr, elfSymbol);
    ASSERT_EQ(buildId, "");
    // illegal size
    elfParse->GetAdltMapSectionInfo(0, 13);
}

/**
 * @tc.name: DfxElfTest009
 * @tc.desc: test scenario function
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest009, TestSize.Level2)
{
    auto elf = std::make_shared<DfxElf>(nullptr);
    elf->GetEndVaddr();
    std::string buildId = "test";
    elf->SetBuildId(buildId);
    const size_t num = 10;
    unsigned char buf[num] = {0};
    elf->GetSectionData(buf, num, "");
    struct UnwindTableInfo uti;
    ShdrInfo shdr;
    bool cur = elf->FillUnwindTableByExidx(shdr, 0, nullptr);
    ASSERT_FALSE(cur);
    cur = elf->FillUnwindTableByExidx(shdr, 0, &uti);
    ASSERT_TRUE(cur);
}

/**
 * @tc.name: DfxElfTest010
 * @tc.desc: test scenario function
 * @tc.type: FUNC
 */
HWTEST_F(DfxElfTest, DfxElfTest010, TestSize.Level2)
{
    auto elf = std::make_shared<DfxElf>(nullptr);
    struct dl_phdr_info info;
    int cur = elf->DlPhdrCb(nullptr, 0, nullptr);
    ASSERT_EQ(cur, -1);
    cur = elf->DlPhdrCb(&info, 0, nullptr);
    ASSERT_EQ(cur, -1);
    elf->GetMmapPtr();
    bool isvalid = elf->IsValidElf(nullptr, 0);
    ASSERT_FALSE(isvalid);
}
} // namespace HiviewDFX
} // namespace OHOS

