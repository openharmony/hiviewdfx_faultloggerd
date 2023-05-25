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

#include "dfx_base_test.h"

#include <ctime>
#include <securec.h>
#include <string>
#include <vector>

#include "dfx_config.h"
#define private public
#include "dfx_dump_request.h"
#undef private
#include "dfx_dump_res.h"
#include "dfx_fault_stack.h"
#include "dfx_elf.h"
#include "dfx_signal.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

void DfxBaseTest::SetUpTestCase(void)
{
}

void DfxBaseTest::TearDownTestCase(void)
{
}

void DfxBaseTest::SetUp(void)
{
}

void DfxBaseTest::TearDown(void)
{
}

static const char DUMPCATCHER_ELF_FILE[] = "/system/bin/dumpcatcher";

/**
 * @tc.name: DfxConfigTest001
 * @tc.desc: test DfxConfig class functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxConfigTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxConfigTest001: start.";
    ASSERT_EQ(DfxConfig::GetConfig().logPersist, false);
    ASSERT_EQ(DfxConfig::GetConfig().displayRegister, true);
    ASSERT_EQ(DfxConfig::GetConfig().displayBacktrace, true);
    ASSERT_EQ(DfxConfig::GetConfig().displayMaps, true);
    ASSERT_EQ(DfxConfig::GetConfig().displayFaultStack, true);
    ASSERT_EQ(DfxConfig::GetConfig().dumpOtherThreads, false);
    ASSERT_EQ(DfxConfig::GetConfig().highAddressStep, 512);
    ASSERT_EQ(DfxConfig::GetConfig().lowAddressStep, 16);
    ASSERT_EQ(DfxConfig::GetConfig().maxFrameNums, 64);
    GTEST_LOG_(INFO) << "DfxConfigTest001: end.";
}

/**
 * @tc.name: DfxElfTest001
 * @tc.desc: test DfxElf class functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxElfTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest001: start.";
    auto dfxElf = DfxElf::Create(DUMPCATCHER_ELF_FILE);
    GTEST_LOG_(INFO) << dfxElf->GetFd();
    GTEST_LOG_(INFO) << dfxElf->GetSize();
    dfxElf->SetLoadBias(0);
    ASSERT_EQ(dfxElf->GetLoadBias(), 0);
    dfxElf->SetHeader(ElfW(Ehdr)());
    ElfW(Ehdr) header = dfxElf->GetHeader();
    struct ElfLoadInfo info;
    info.offset = 0;
    info.vaddr = 1;
    vector<ElfLoadInfo> infos;
    infos.emplace_back(info);
    dfxElf->SetInfos(infos);
    auto infos_bak = dfxElf->GetInfos();
    ASSERT_EQ(infos_bak.at(0).offset, 0);
    ASSERT_EQ(infos_bak.at(0).vaddr, 1);
    GTEST_LOG_(INFO) << "DfxElfTest001: end.";
}

/**
 * @tc.name: DfxElfTest002
 * @tc.desc: test DfxElf FindRealLoadOffset functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxElfTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxElfTest002: start.";
    auto dfxElf = DfxElf::Create(DUMPCATCHER_ELF_FILE);
    GTEST_LOG_(INFO) << dfxElf->FindRealLoadOffset(0);
    GTEST_LOG_(INFO) << dfxElf->FindRealLoadOffset(1);
    GTEST_LOG_(INFO) << "DfxElfTest002: end.";
}

/**
 * @tc.name: DfxRegsTest001
 * @tc.desc: test DfxRegs SetRegsData & GetRegsData functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxRegsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxRegsTest001: start.";
    auto dfxRegs = DfxRegs::Create();
    ASSERT_NE(dfxRegs, nullptr);
    std::vector<uintptr_t> setRegs {};
    for (size_t i = 0; i < 10; i++) { // test 10 regs
        setRegs.push_back(i);
    }
    dfxRegs->SetRegsData(setRegs);
    auto getRegs = dfxRegs->GetRegsData();
    ASSERT_EQ(setRegs, getRegs);
    GTEST_LOG_(INFO) << "DfxRegsTest001: end.";
}

/**
 * @tc.name: DfxDumpResTest001
 * @tc.desc: test DfxDumpRes functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxDumpResTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpResTest001: start.";
    int32_t res = DUMP_ESUCCESS;
    GTEST_LOG_(INFO) << DfxDumpRes::ToString(res);
    GTEST_LOG_(INFO) << "DfxDumpResTest001: end.";
}

/**
 * @tc.name: DfxSignalTest001
 * @tc.desc: test DfxSignal functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxSignalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest001: start.";
    siginfo_t si = {
        .si_signo = SIGSEGV,
        .si_errno = 0,
        .si_code = -1,
        .si_value.sival_int = gettid()
    };
    map<int, string> sigKeys = {
        { SIGILL, string("SIGILL") },
        { SIGTRAP, string("SIGTRAP") },
        { SIGABRT, string("SIGABRT") },
        { SIGBUS, string("SIGBUS") },
        { SIGFPE, string("SIGFPE") },
        { SIGSEGV, string("SIGSEGV") },
        { SIGSTKFLT, string("SIGSTKFLT") },
        { SIGSYS, string("SIGSYS") },
    };
    for (auto sigKey : sigKeys) {
        std::string sigKeyword = "Signal:";
        si.si_signo = sigKey.first;
        sigKeyword += sigKey.second;
        std::string sigStr = PrintSignal(si);
        GTEST_LOG_(INFO) << sigStr;
        ASSERT_TRUE(sigStr.find(sigKeyword) != std::string::npos);
    }
    GTEST_LOG_(INFO) << "DfxSignalTest001: end.";
}
