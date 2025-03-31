/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include <cstdio>
#include <dlfcn.h>
#include <cstdint>

#include "dfx_maps.h"
#include "dfx_symbols.h"
#include "dfx_test_util.h"
#include "elf_factory_selector.h"
#include "unwinder_config.h"

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace {
    constexpr const char* const DUMPCATCHER_ELF_FILE = "/system/bin/dumpcatcher";
}
class ElfFactoryTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: ElfFactoryTest001
 * @tc.desc: test VdsoElfFactory normal case
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest001: start.";
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
        return;
    }
    pid_t pid = GetProcessPid(FOUNDATION_NAME);
    auto maps = DfxMaps::Create(pid);
    std::vector<std::shared_ptr<DfxMap>> shmmMaps;
    ASSERT_TRUE(maps->FindMapsByName("[shmm]", shmmMaps));
    std::shared_ptr<DfxMap> shmmMap = nullptr;
    for (auto map : shmmMaps) {
        if (map->IsMapExec()) {
            shmmMap = map;
            break;
        }
    }
    ASSERT_TRUE(shmmMap != nullptr);
    VdsoElfFactory factory(shmmMap->begin, shmmMap->end - shmmMap->begin, pid);
    auto shmmElf = factory.Create();
    ASSERT_TRUE(shmmElf != nullptr);
    std::vector<DfxSymbol> shmmSyms;
    DfxSymbols::ParseSymbols(shmmSyms, shmmElf, "");
    GTEST_LOG_(INFO) << "shmm symbols size" << shmmSyms.size();
    ASSERT_GT(shmmSyms.size(), 0);
    for (const auto& sym : shmmSyms) {
        GTEST_LOG_(INFO) << sym.ToDebugString();
    }
    GTEST_LOG_(INFO) << "ElfFactoryTest001 : end.";
}

/**
 * @tc.name: ElfFactoryTest002
 * @tc.desc: test VdsoElfFactory using not vdso exec map case
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest002: start.";
    pid_t pid = GetProcessPid(FOUNDATION_NAME);
    auto dfxMaps = DfxMaps::Create(pid);
    std::vector<std::shared_ptr<DfxMap>> maps;
    ASSERT_TRUE(dfxMaps->FindMapsByName("libunwinder.z.so", maps));
    for (const auto& map : maps) {
        if (map->IsMapExec()) {
            VdsoElfFactory factory(map->begin, map->end - map->begin, pid);
            auto elf = factory.Create();
            ASSERT_TRUE(elf == nullptr);
            break;
        }
    }
    GTEST_LOG_(INFO) << "ElfFactoryTest002 : end.";
}

/**
 * @tc.name: ElfFactoryTest003
 * @tc.desc: test VdsoElfFactory using abnormal param
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest003: start.";
    uint64_t begin = 0;
    size_t size = 0;
    pid_t pid = 0;
    VdsoElfFactory factory(begin, size, pid);
    auto elf = factory.Create();
    ASSERT_TRUE(elf == nullptr);
    GTEST_LOG_(INFO) << "ElfFactoryTest003 : end.";
}

#ifndef __x86_64__
/**
 * @tc.name: ElfFactoryTest004
 * @tc.desc: test MiniDebugInfoFactory normal case
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest004: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    RegularElfFactory regularElfFactory(DUMPCATCHER_ELF_FILE);
    auto elf = regularElfFactory.Create();
    ASSERT_TRUE(elf->IsValid());
    auto gnuDebugDataHdr = elf->GetGnuDebugDataHdr();
    ASSERT_TRUE(gnuDebugDataHdr.size != 0);
    MiniDebugInfoFactory miniDebugInfoFactory(gnuDebugDataHdr);
    elf = miniDebugInfoFactory.Create();
    ASSERT_TRUE(elf->IsValid());
    GTEST_LOG_(INFO) << "ElfFactoryTest004: end.";
}

/**
 * @tc.name: ElfFactoryTest005
 * @tc.desc: test MiniDebugInfoFactory normal case with test parsing symbol
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest005: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    RegularElfFactory regularElfFactory(DUMPCATCHER_ELF_FILE);
    auto elf = regularElfFactory.Create();
    ASSERT_TRUE(elf->IsValid());
    auto gnuDebugDataHdr = elf->GetGnuDebugDataHdr();
    ASSERT_TRUE(gnuDebugDataHdr.size != 0);
    MiniDebugInfoFactory miniDebugInfoFactory(gnuDebugDataHdr);
    elf = miniDebugInfoFactory.Create();
    ASSERT_TRUE(elf->IsValid());
    std::vector<DfxSymbol> symbols;
    DfxSymbols::ParseSymbols(symbols, elf, "");
    GTEST_LOG_(INFO) << "symbols size" << symbols.size();
    ASSERT_GT(symbols.size(), 0);
    for (const auto& symbol : symbols) {
        GTEST_LOG_(INFO) << symbol.ToDebugString();
    }
    GTEST_LOG_(INFO) << "ElfFactoryTest005: end.";
}

/**
 * @tc.name: ElfFactoryTest006
 * @tc.desc: test MiniDebugInfoFactory disable minidebuginfo case
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest006: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(false);
    RegularElfFactory regularElfFactory(DUMPCATCHER_ELF_FILE);
    auto elf = regularElfFactory.Create();
    ASSERT_TRUE(elf->IsValid());
    auto gnuDebugDataHdr = elf->GetGnuDebugDataHdr();
    ASSERT_TRUE(gnuDebugDataHdr.size != 0);
    MiniDebugInfoFactory miniDebugInfoFactory(gnuDebugDataHdr);
    elf = miniDebugInfoFactory.Create();
    ASSERT_TRUE(elf == nullptr);
    GTEST_LOG_(INFO) << "ElfFactoryTest006: end.";
}

/**
 * @tc.name: ElfFactoryTest007
 * @tc.desc: test MiniDebugInfoFactory using abnormal param
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest007: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    GnuDebugDataHdr hdr;
    MiniDebugInfoFactory miniDebugInfoFactory(hdr);
    auto elf = miniDebugInfoFactory.Create();
    ASSERT_TRUE(elf == nullptr);
    GTEST_LOG_(INFO) << "ElfFactoryTest007: end.";
}

/**
 * @tc.name: ElfFactoryTest008
 * @tc.desc: test MiniDebugInfoFactory using abnormal param
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest008: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(false);
    GnuDebugDataHdr hdr;
    MiniDebugInfoFactory miniDebugInfoFactory(hdr);
    auto elf = miniDebugInfoFactory.Create();
    ASSERT_TRUE(elf == nullptr);
    GTEST_LOG_(INFO) << "ElfFactoryTest008: end.";
}
#endif

/**
 * @tc.name: ElfFactoryTest009
 * @tc.desc: test ElfFactoryTest with 64 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest009: start.";
    const char* elf64File = "/data/test/resource/testdata/elf_test";
    RegularElfFactory factory(elf64File);
    auto elf = factory.Create();
    ASSERT_TRUE(elf->IsValid());
    GTEST_LOG_(INFO) << "ElfFactoryTest009: end.";
}

/**
 * @tc.name: ElfFactoryTest010
 * @tc.desc: ElfFactoryTest with 32 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest010: start.";
    const char* elf32File = "/data/test/resource/testdata/elf32_test";
    RegularElfFactory factory(elf32File);
    auto elf = factory.Create();
    ASSERT_TRUE(elf->IsValid());
    GTEST_LOG_(INFO) << "ElfFactoryTest010: end.";
}

/**
 * @tc.name: ElfFactoryTest011
 * @tc.desc: test DfxElf functions with empty file path
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest002: start.";
    std::string filePath  = "";
    RegularElfFactory factory(filePath);
    auto elf = factory.Create();
    ASSERT_FALSE(elf->IsValid());
    GTEST_LOG_(INFO) << "ElfFactoryTest011: end.";
}

/**
 * @tc.name: ElfFactoryTest012
 * @tc.desc: test DfxElf functions with invalid filePath
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest012: start.";
    std::string filePath  = "[anno:stack:2345]";
    RegularElfFactory factory(filePath);
    auto elf = factory.Create();
    ASSERT_FALSE(elf->IsValid());
    GTEST_LOG_(INFO) << "ElfFactoryTest012: end.";
}

/**
 * @tc.name: ElfFactoryTest013
 * @tc.desc: ElfFactorySelector in normal case
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest013: start.";
    pid_t pid = GetProcessPid(FOUNDATION_NAME);
    auto dfxMaps = DfxMaps::Create(pid);
    auto maps = dfxMaps->GetMaps();
    for (const auto& map : maps) {
        auto elfFactory = ElfFactorySelector::Select(*(map.get()), pid);
        auto elf = elfFactory->Create();
        if (map->IsVdsoMap()) {
            ASSERT_TRUE(elf->IsValid());
            GTEST_LOG_(INFO) << elf->GetElfName();
            ASSERT_TRUE(elf->GetElfName().find("shmm") != std::string::npos ||
                elf->GetElfName().find("vdso") != std::string::npos);
        } else if (DfxMaps::IsLegalMapItem(map->name) && map->IsMapExec()) {
            ASSERT_TRUE(elf->IsValid()) << map->name;
        }
    }
    GTEST_LOG_(INFO) << "ElfFactoryTest013: end.";
}

/**
 * @tc.name: ElfFactoryTest014
 * @tc.desc: ElfFactorySelector using abnormal param
 * @tc.type: FUNC
 */
HWTEST_F(ElfFactoryTest, ElfFactoryTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElfFactoryTest014: start.";
    DfxMap map;
    auto elfFactory = ElfFactorySelector::Select(map);
    auto elf = elfFactory->Create();
    ASSERT_FALSE(elf->IsValid());
    GTEST_LOG_(INFO) << "ElfFactoryTest014: end.";
}
}
}