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
#include "dfx_util.h"

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
    DfxConfig::GetInstance().ReadConfig();
    ASSERT_EQ(DfxConfig::GetInstance().GetLogPersist(), false);
    DfxConfig::GetInstance().SetLogPersist(true);
    ASSERT_EQ(DfxConfig::GetInstance().GetLogPersist(), true);

    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayRegister(), true);
    DfxConfig::GetInstance().SetDisplayRegister(false);
    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayRegister(), false);

    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayBacktrace(), true);
    DfxConfig::GetInstance().SetDisplayBacktrace(false);
    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayBacktrace(), false);

    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayMaps(), true);
    DfxConfig::GetInstance().SetDisplayMaps(false);
    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayMaps(), false);

    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayFaultStack(), true);
    DfxConfig::GetInstance().SetDisplayFaultStack(false);
    ASSERT_EQ(DfxConfig::GetInstance().GetDisplayFaultStack(), false);

    ASSERT_EQ(DfxConfig::GetInstance().GetDumpOtherThreads(), false);
    DfxConfig::GetInstance().SetDumpOtherThreads(true);
    ASSERT_EQ(DfxConfig::GetInstance().GetDumpOtherThreads(), true);

    ASSERT_EQ(DfxConfig::GetInstance().GetFaultStackHighAddressStep(), 512);
    ASSERT_EQ(DfxConfig::GetInstance().GetFaultStackLowAddressStep(), 16);
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
    dfxElf->SetName("dumpcatcher");
    ASSERT_EQ(dfxElf->GetName(), "dumpcatcher");
    dfxElf->SetPath(DUMPCATCHER_ELF_FILE);
    ASSERT_EQ(dfxElf->GetPath(), DUMPCATCHER_ELF_FILE);
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
 * @tc.name: DfxDumpResquestTest001
 * @tc.desc: test DfxDumpResquest functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxDumpResquestTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpResquestTest001: start.";
    auto request = make_shared<ProcessDumpRequest>();
    request->SetRecycleTid(-1);
    ASSERT_EQ(request->GetRecycleTid(), -1);
    struct TraceInfo traceinfo;
    request->traceInfo = traceinfo;
    auto info = request->GetTraceInfo();
    char threadname[] = "testThread";
    char processname[] = "testProcess";
    char fatalmsg[] = "test fatal message";
    if (strcpy_s(request->threadName_, NAME_LEN, threadname) != EOK) {
        GTEST_LOG_(ERROR) << "strcpy_s failed.";
    }
    if (strcpy_s(request->processName_, NAME_LEN, processname) != EOK) {
        GTEST_LOG_(ERROR) << "strcpy_s failed.";
    }
    if (strcpy_s(request->lastFatalMessage_, NAME_LEN, fatalmsg) != EOK) {
        GTEST_LOG_(ERROR) << "strcpy_s failed.";
    }
    GTEST_LOG_(INFO) << "threadName_ = " << request->GetThreadNameString();
    GTEST_LOG_(INFO) << "processName_ = " << request->GetProcessNameString();
    GTEST_LOG_(INFO) << "lastFatalMessage_ = " << request->GetLastFatalMessage();
    GTEST_LOG_(INFO) << "DfxDumpResquestTest001: end.";
}

/**
 * @tc.name: DfxDumpResTest001
 * @tc.desc: test DfxDumpRes functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxDumpResTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpResTest001: start.";
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_ESUCCESS);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EREADREQUEST);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EGETPPID);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EATTACH);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EGETFD);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_ENOMEM);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EBADREG);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EREADONLYREG);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_ESTOPUNWIND);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EINVALIDIP);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EBADFRAME);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EINVAL);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_EBADVERSION);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(ProcessDumpRes::DUMP_ENOINFO);
    ASSERT_EQ(DfxDumpRes::GetInstance().GetRes(), ProcessDumpRes::DUMP_ENOINFO);
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
    DfxDumpRes::GetInstance().SetRes(-1); // -1 : invalid status
    GTEST_LOG_(INFO) << DfxDumpRes::GetInstance().ToString();
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
    GTEST_LOG_(INFO) << PrintSignal(si);
    si.si_signo = SIGABRT;
    GTEST_LOG_(INFO) << PrintSignal(si);
    si.si_signo = SIGBUS;
    GTEST_LOG_(INFO) << PrintSignal(si);
    si.si_signo = SIGILL;
    GTEST_LOG_(INFO) << PrintSignal(si);
    si.si_signo = SIGTRAP;
    GTEST_LOG_(INFO) << PrintSignal(si);
    si.si_signo = SIGFPE;
    GTEST_LOG_(INFO) << PrintSignal(si);
    si.si_signo = SIGSTKFLT;
    GTEST_LOG_(INFO) << PrintSignal(si);
    si.si_signo = SIGSYS;
    GTEST_LOG_(INFO) << PrintSignal(si);
    GTEST_LOG_(INFO) << "DfxSignalTest001: end.";
}

/**
 * @tc.name: DfxUtilTest001
 * @tc.desc: test DfxUtil functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest001: start.";
    time_t now = time(nullptr);
    GTEST_LOG_(INFO) << GetCurrentTimeStr(static_cast<uint64_t>(now));
    now += 100000; // 100000 : test time offset
    GTEST_LOG_(INFO) << GetCurrentTimeStr(static_cast<uint64_t>(now));
    GTEST_LOG_(INFO) << "DfxUtilTest001: end.";
}

/**
 * @tc.name: DfxUtilTest002
 * @tc.desc: test DfxUtil functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxBaseTest, DfxUtilTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest002: start.";
    pid_t pid = getppid();
    ASSERT_EQ(pid, GetRealTargetPid());
    GTEST_LOG_(INFO) << "DfxUtilTest002: end.";
}
