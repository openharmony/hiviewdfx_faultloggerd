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
#include <filesystem>
#include "elf_factory.h"
#include "elf_imitate.h"
#include "unwinder_config.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

#define ELF32_FILE "/data/test/resource/testdata/elf32_test"
#define ELF64_FILE "/data/test/resource/testdata/elf_test"
#define DUMPCATCHER_ELF_FILE "/system/bin/dumpcatcher"
namespace OHOS {
namespace HiviewDFX {
class DfxSymbolsTest : public testing::Test {};


/**
 * @tc.name: DfxSymbolsTest001
 * @tc.desc: test DfxSymbols functions with 32 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxSymbolsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSymbolsTest001: start.";
    RegularElfFactory factory(ELF32_FILE);
    auto elf = factory.Create();
    ASSERT_TRUE(elf->IsValid());
    ElfImitate elfImitate;
    elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF32);
    std::string funcName;
    uint64_t funcOffset;
    ASSERT_TRUE(DfxSymbols::GetFuncNameAndOffsetByPc(0x00001786, elf, funcName, funcOffset));
    GTEST_LOG_(INFO) << funcName << " " << funcOffset;
    std::vector<DfxSymbol> symbols;
    std::vector<DfxSymbol> symbolsImitate;
    ASSERT_TRUE(DfxSymbols::ParseSymbols(symbols, elf, ELF32_FILE));
    ASSERT_TRUE(elfImitate.ParseSymbols(symbolsImitate, ELF32_FILE));
    ASSERT_EQ(symbols.size(), symbolsImitate.size());
    for (size_t i = 0; i < symbolsImitate.size(); ++i) {
        symbols[i].fileVaddr_ = symbolsImitate[i].fileVaddr_;
        symbols[i].funcVaddr_ = symbolsImitate[i].funcVaddr_;
        symbols[i].name_ = symbolsImitate[i].name_;
        symbols[i].demangle_ = symbolsImitate[i].demangle_;
        symbols[i].module_ = symbolsImitate[i].module_;
    }

    DfxSymbols::AddSymbolsByPlt(symbols, elf, ELF32_FILE);
    elfImitate.AddSymbolsByPlt(symbolsImitate, ELF32_FILE);
    ASSERT_EQ(symbols.size(), symbolsImitate.size());
    for (size_t i = 0; i < symbolsImitate.size(); ++i) {
        symbols[i].fileVaddr_ = symbolsImitate[i].fileVaddr_;
        symbols[i].funcVaddr_ = symbolsImitate[i].funcVaddr_;
        symbols[i].name_ = symbolsImitate[i].name_;
        symbols[i].demangle_ = symbolsImitate[i].demangle_;
        symbols[i].module_ = symbolsImitate[i].module_;
    }
    ASSERT_FALSE(DfxSymbols::ParseSymbols(symbols, nullptr, ELF32_FILE));
    elf->SetBaseOffset(0x1000);
    ASSERT_TRUE(DfxSymbols::ParseSymbols(symbols, elf, ELF32_FILE));
    ASSERT_FALSE(DfxSymbols::AddSymbolsByPlt(symbols, nullptr, ELF32_FILE));
    GTEST_LOG_(INFO) << symbols[0].module_;
    GTEST_LOG_(INFO) << "DfxSymbolsTest001: end.";
}

/**
 * @tc.name: DfxSymbolsTest002
 * @tc.desc: test DfxSymbols functions with 64 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxSymbolsTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSymbolsTest002: start.";
    RegularElfFactory factory(ELF64_FILE);
    auto elf = factory.Create();
    ASSERT_TRUE(elf->IsValid());
    ElfImitate elfImitate;
    elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF64);
    std::string funcName;
    uint64_t funcOffset;
    ASSERT_TRUE(DfxSymbols::GetFuncNameAndOffsetByPc(0x00002a08, elf, funcName, funcOffset));
    GTEST_LOG_(INFO) << funcName << " " << funcOffset;
    std::vector<DfxSymbol> symbols;
    std::vector<DfxSymbol> symbolsImitate;
    ASSERT_TRUE(DfxSymbols::ParseSymbols(symbols, elf, ELF64_FILE));
    ASSERT_TRUE(elfImitate.ParseSymbols(symbolsImitate, ELF64_FILE));
    ASSERT_EQ(symbols.size(), symbolsImitate.size());
    for (size_t i = 0; i < symbolsImitate.size(); ++i) {
        symbols[i].fileVaddr_ = symbolsImitate[i].fileVaddr_;
        symbols[i].funcVaddr_ = symbolsImitate[i].funcVaddr_;
        symbols[i].name_ = symbolsImitate[i].name_;
        symbols[i].demangle_ = symbolsImitate[i].demangle_;
        symbols[i].module_ = symbolsImitate[i].module_;
    }

    DfxSymbols::AddSymbolsByPlt(symbols, elf, ELF64_FILE);
    elfImitate.AddSymbolsByPlt(symbolsImitate, ELF64_FILE);
    ASSERT_EQ(symbols.size(), symbolsImitate.size());
    for (size_t i = 0; i < symbolsImitate.size(); ++i) {
        symbols[i].fileVaddr_ = symbolsImitate[i].fileVaddr_;
        symbols[i].funcVaddr_ = symbolsImitate[i].funcVaddr_;
        symbols[i].name_ = symbolsImitate[i].name_;
        symbols[i].demangle_ = symbolsImitate[i].demangle_;
        symbols[i].module_ = symbolsImitate[i].module_;
    }
    ASSERT_FALSE(DfxSymbols::ParseSymbols(symbols, nullptr, ELF64_FILE));
    elf->SetBaseOffset(0x1000);
    ASSERT_TRUE(DfxSymbols::ParseSymbols(symbols, elf, ELF64_FILE));
    ASSERT_FALSE(DfxSymbols::AddSymbolsByPlt(symbols, nullptr, ELF64_FILE));
    GTEST_LOG_(INFO) << symbols[0].module_;
    GTEST_LOG_(INFO) << "DfxSymbolsTest002: end.";
}

#if defined(ENABLE_MINIDEBUGINFO)
/**
 * @tc.name: DfxSymbolsTest003
 * @tc.desc: test DfxSymbols functions with minidebuginfo elf
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxSymbolsTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSymbolsTest003: start.";
    UnwinderConfig::SetEnableMiniDebugInfo(true);
    std::vector<DfxSymbol> dfxSymbols;
    RegularElfFactory factory(DUMPCATCHER_ELF_FILE);
    auto elf = factory.Create();
    ASSERT_TRUE(elf->IsValid());
    DfxSymbols::ParseSymbols(dfxSymbols, elf, DUMPCATCHER_ELF_FILE);
    GTEST_LOG_(INFO) << "DfxSymbolsTest003: symbols size:" << dfxSymbols.size();
    ASSERT_GE(dfxSymbols.size(), 0);
    for (auto dfxSymbol : dfxSymbols) {
        GTEST_LOG_(INFO) << "DfxSymbolsTest003: dfxSymbol.demangle_: "<< dfxSymbol.demangle_;
    }
    GTEST_LOG_(INFO) << "DfxSymbolsTest003: end.";
}
#endif

/**
 * @tc.name: DfxDemangleTest001
 * @tc.desc: test DfxSymbols demangle functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxDemangleTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDemangleTest001: start.";
    EXPECT_EQ("", DfxSymbols::Demangle(""));
    EXPECT_EQ("a", DfxSymbols::Demangle("a"));
    EXPECT_EQ("_", DfxSymbols::Demangle("_"));
    EXPECT_EQ("ab", DfxSymbols::Demangle("ab"));
    EXPECT_EQ("abc", DfxSymbols::Demangle("abc"));
    EXPECT_EQ("_R", DfxSymbols::Demangle("_R"));
    EXPECT_EQ("_Z", DfxSymbols::Demangle("_Z"));
    GTEST_LOG_(INFO) << "DfxDemangleTest001: end.";
}

/**
 * @tc.name: DfxDemangleTest002
 * @tc.desc: test DfxSymbols demangle functions with cxx
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxDemangleTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDemangleTest002: start.";
    EXPECT_EQ("fake(bool)", DfxSymbols::Demangle("_Z4fakeb"));
    EXPECT_EQ("demangle(int)", DfxSymbols::Demangle("_Z8demanglei"));
    GTEST_LOG_(INFO) << "DfxDemangleTest002: end.";
}

/**
 * @tc.name: DfxDemangleTest003
 * @tc.desc: test DfxSymbols demangle functions with rust
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxDemangleTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDemangleTest003: start.";
    EXPECT_EQ("std::rt::lang_start_internal",
        DfxSymbols::Demangle("_RNvNtCs2WRBrrl1bb1_3std2rt19lang_start_internal"));
    EXPECT_EQ("profcollectd::main", DfxSymbols::Demangle("_RNvCs4VPobU5SDH_12profcollectd4main"));
    GTEST_LOG_(INFO) << "DfxDemangleTest003: end.";
}

/**
 * @tc.name: DfxDemangleTest004
 * @tc.desc: test DfxSymbols demangle functions with cangjie
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxDemangleTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDemangleTest004: start.";
    std::filesystem::path runtimePath("/system/lib64/chipset-sdk/libcangjie-demangle.so");
    if (std::filesystem::exists(runtimePath)) {
        EXPECT_EQ("std.time.initLocalDefault()",
            DfxSymbols::Demangle("_CN8std.time16initLocalDefaultHv"));
        EXPECT_EQ("std.core.Error::init()", DfxSymbols::Demangle("_CN8std.core5Error6<init>Hv"));
    } else {
        EXPECT_EQ("_CN8std.time16initLocalDefaultHv",
            DfxSymbols::Demangle("_CN8std.time16initLocalDefaultHv"));
        EXPECT_EQ("_CN8std.core5Error6<init>Hv",
            DfxSymbols::Demangle("_CN8std.core5Error6<init>Hv"));
    }
    GTEST_LOG_(INFO) << "DfxDemangleTest004: end.";
}

/**
 * @tc.name: DfxDemangleTest005
 * @tc.desc: test DfxSymbols demangle functions with cangjie
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxDemangleTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDemangleTest005: start.";
    CompressHapElfFactory compress1("", nullptr);
    std::shared_ptr<DfxElf> res = compress1.Create();
    EXPECT_EQ(res, nullptr);
    auto prevMap = std::shared_ptr<DfxMap>();
    CompressHapElfFactory compress2("", prevMap);
    res = compress2.Create();
    EXPECT_EQ(res, nullptr);
    CompressHapElfFactory compress3("/proc/", prevMap);
    res = compress3.Create();
    EXPECT_EQ(res, nullptr);
    CompressHapElfFactory compress4("/proc/123/test.hap", prevMap);
    res = compress4.Create();
    EXPECT_EQ(res, nullptr);
    GTEST_LOG_(INFO) << "DfxDemangleTest005: end.";
}
} // namespace HiviewDFX
} // namespace OHOS

