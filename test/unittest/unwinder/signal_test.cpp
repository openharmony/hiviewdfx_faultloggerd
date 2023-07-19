/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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
#include <map>
#include <memory>
#include <string>

#include "dfx_signal.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxSignalTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: DfxSignalTest001
 * @tc.desc: test DfxSignal functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxSignalTest, DfxSignalTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest001: start.";
    siginfo_t si = {
        .si_signo = SIGSEGV,
        .si_errno = 0,
        .si_code = -1,
        .si_value.sival_int = static_cast<int>(gettid())
    };
    std::map<int, std::string> sigKey = {
        { SIGILL, string("SIGILL") },
        { SIGTRAP, string("SIGTRAP") },
        { SIGABRT, string("SIGABRT") },
        { SIGBUS, string("SIGBUS") },
        { SIGFPE, string("SIGFPE") },
        { SIGSEGV, string("SIGSEGV") },
        { SIGSTKFLT, string("SIGSTKFLT") },
        { SIGSYS, string("SIGSYS") },
    };
    for (auto sigKey : sigKey) {
        std::string sigKeyword = "Signal:";
        si.si_signo = sigKey.first;
        sigKeyword += sigKey.second;
        std::string sigStr = DfxSignal::PrintSignal(si);
        GTEST_LOG_(INFO) << sigStr;
        ASSERT_TRUE(sigStr.find(sigKeyword) != std::string::npos);
    }
    GTEST_LOG_(INFO) << "DfxSignalTest001: end.";
}

/**
 * @tc.name: DfxSignalTest002
 * @tc.desc: test if signal info is available
 * @tc.type: FUNC
 */
HWTEST_F(DfxSignalTest, DfxSignalTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest002: start.";
    int32_t input = 1;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsAvailable();
    EXPECT_EQ(true, ret != true) << "DfxSignalTest002 Failed";
    GTEST_LOG_(INFO) << "DfxSignalTest002: end.";
}

/**
 * @tc.name: DfxSignalTest003
 * @tc.desc: test if addr is available
 * @tc.type: FUNC
 */
HWTEST_F(DfxSignalTest, DfxSignalTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest003: start.";
    int32_t input = -100;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsAddrAvailable();
    EXPECT_EQ(true, ret != true) << "DfxSignalTest003 Failed";
    GTEST_LOG_(INFO) << "DfxSignalTest003: end.";
}

/**
 * @tc.name: DfxSignalTest004
 * @tc.desc: test if pid is available
 * @tc.type: FUNC
 */
HWTEST_F(DfxSignalTest, DfxSignalTest004, TestSize.Level2)
{
    int32_t input = 100;
    GTEST_LOG_(INFO) << "DfxSignalTest004: start.";
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    bool ret = signal->IsPidAvailable();
    EXPECT_EQ(true, ret != true) << "DfxSignalTest004 Failed";
    GTEST_LOG_(INFO) << "DfxSignalTest004: end.";
}

/**
 * @tc.name: DfxSignalTest005
 * @tc.desc: test if GetSignal
 * @tc.type: FUNC
 */
HWTEST_F(DfxSignalTest, DfxSignalTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest005: start.";
    int32_t input = 1;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    int32_t output = signal->GetSignal();
    EXPECT_EQ(true, output == input) << "DfxSignalTest005 Failed";
    GTEST_LOG_(INFO) << "DfxSignalTest005: end.";
}

std::map<int32_t, std::string> sigKeys = {
    { SIGILL, std::string("SIGILL") },
    { SIGTRAP, std::string("SIGTRAP") },
    { SIGBUS, std::string("SIGBUS") },
    { SIGFPE, std::string("SIGFPE") },
    { SIGSEGV, std::string("SIGSEGV") },
};
std::map<int, std::string> segvCode = {
    { SEGV_MAPERR, string("SEGV_MAPERR") },
    { SEGV_ACCERR, string("SEGV_ACCERR") },
    { SI_USER, string("SI_USER") },
};
std::map<int, std::string> trapCode = {
    { TRAP_BRKPT, string("TRAP_BRKPT") },
    { TRAP_TRACE, string("TRAP_TRACE") },
    { TRAP_BRANCH, string("TRAP_BRANCH") },
    { TRAP_HWBKPT, string("TRAP_HWBKPT") },
    { SI_USER, string("SI_USER") },
};
std::map<int, std::string> illCode = {
    { ILL_ILLOPC, string("ILL_ILLOPC") },
    { ILL_ILLOPN, string("ILL_ILLOPN") },
    { ILL_ILLADR, string("ILL_ILLADR") },
    { ILL_ILLTRP, string("ILL_ILLTRP") },
    { ILL_PRVOPC, string("ILL_PRVOPC") },
    { ILL_PRVREG, string("ILL_PRVREG") },
    { ILL_COPROC, string("ILL_COPROC") },
    { ILL_BADSTK, string("ILL_BADSTK") },
    { SI_USER, string("SI_USER") },
};
std::map<int, std::string> fpeCode = {
    { FPE_INTDIV, string("FPE_INTDIV") },
    { FPE_INTOVF, string("FPE_INTOVF") },
    { FPE_FLTDIV, string("FPE_FLTDIV") },
    { FPE_FLTOVF, string("FPE_FLTOVF") },
    { FPE_FLTUND, string("FPE_FLTUND") },
    { FPE_FLTRES, string("FPE_FLTRES") },
    { FPE_FLTINV, string("FPE_FLTINV") },
    { FPE_FLTSUB, string("FPE_FLTSUB") },
    { SI_USER, string("SI_USER") },
};
std::map<int, std::string> busCode = {
    { BUS_ADRALN, string("BUS_ADRALN") },
    { BUS_ADRERR, string("BUS_ADRERR") },
    { BUS_OBJERR, string("BUS_OBJERR") },
    { BUS_MCEERR_AR, string("BUS_MCEERR_AR") },
    { BUS_MCEERR_AO, string("BUS_MCEERR_AO") },
    { SI_USER, string("SI_USER") },
};
/**
 * @tc.name: DfxSignalTest006
 * @tc.desc: test DfxSignal functions
 * @tc.type: FUNC
 */
HWTEST_F(DfxSignalTest, DfxSignalTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest006: start.";
    siginfo_t si = {
        .si_signo = SIGSEGV,
        .si_errno = 0,
        .si_code = -1,
        .si_value.sival_int = static_cast<int>(gettid())
    };
    for (auto sigKey : sigKeys) {
        si.si_signo = sigKey.first;
        std::map<int, std::string> codeMap;
        switch (si.si_signo) {
            case SIGILL:
                codeMap = illCode;
                break;
            case SIGTRAP:
                codeMap = trapCode;
                break;
            case SIGBUS:
                codeMap = busCode;
                break;
            case SIGFPE:
                codeMap = fpeCode;
                break;
            case SIGSEGV:
                codeMap = segvCode;
                break;
            default:
                break;
        }
        for (auto& code : codeMap) {
            std::string sigKeyword = "Signal:";
            sigKeyword += sigKey.second;
            si.si_code = code.first;
            sigKeyword = sigKeyword + "(" + code.second + ")";
            std::string sigStr = DfxSignal::PrintSignal(si);
            GTEST_LOG_(INFO) << sigStr;
            ASSERT_TRUE(sigStr.find(sigKeyword) != std::string::npos);
        }
    }
    GTEST_LOG_(INFO) << "DfxSignalTest006: end.";
}
}
