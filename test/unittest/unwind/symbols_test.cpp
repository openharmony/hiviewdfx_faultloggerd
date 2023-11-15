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
namespace OHOS {
namespace HiviewDFX {
class DfxSymbolsTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};


/**
 * @tc.name: DfxSymbolsTest001
 * @tc.desc: test DfxSymbols functions with 32 bit ELF
 * @tc.type: FUNC
 */
HWTEST_F(DfxSymbolsTest, DfxSymbolsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSymbolsTest001: start.";
    std::shared_ptr<DfxElf> elf = make_shared<DfxElf>(ELF32_FILE);
    ASSERT_TRUE(elf->IsValid());
    ElfImitate elfImitate;
    elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF32);
    std::vector<DfxSymbol> symbols;
    std::vector<DfxSymbol> symbolsImitate;
    DfxSymbols::ParseSymbols(symbols, elf, ELF32_FILE);
    elfImitate.ParseSymbols(symbolsImitate, ELF32_FILE);
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
    std::string funcName;
    uint64_t funcOffset;
    ASSERT_TRUE(DfxSymbols::GetFuncNameAndOffsetByPc(0x00001786, elf, funcName, funcOffset));
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
    std::shared_ptr<DfxElf> elf = make_shared<DfxElf>(ELF64_FILE);
    ASSERT_TRUE(elf->IsValid());
    ElfImitate elfImitate;
    elfImitate.ParseAllHeaders(ElfImitate::ElfFileType::ELF64);
    std::vector<DfxSymbol> symbols;
    std::vector<DfxSymbol> symbolsImitate;
    DfxSymbols::ParseSymbols(symbols, elf, ELF64_FILE);
    elfImitate.ParseSymbols(symbolsImitate, ELF64_FILE);
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
    std::string funcName;
    uint64_t funcOffset;
    ASSERT_TRUE(DfxSymbols::GetFuncNameAndOffsetByPc(0x00002a08, elf, funcName, funcOffset));

    GTEST_LOG_(INFO) << "DfxSymbolsTest002: end.";
}

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
} // namespace HiviewDFX
} // namespace OHOS

