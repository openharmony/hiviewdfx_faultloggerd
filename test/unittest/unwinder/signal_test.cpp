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
HWTEST_F (DfxSignalTest, DfxSignalTest002, TestSize.Level2)
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
HWTEST_F (DfxSignalTest, DfxSignalTest003, TestSize.Level2)
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
HWTEST_F (DfxSignalTest, DfxSignalTest004, TestSize.Level2)
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
HWTEST_F (DfxSignalTest, DfxSignalTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxSignalTest005: start.";
    int32_t input = 1;
    std::shared_ptr<DfxSignal> signal = std::make_shared<DfxSignal>(input);
    int32_t output = signal->GetSignal();
    EXPECT_EQ(true, output == input) << "DfxSignalTest005 Failed";
    GTEST_LOG_(INFO) << "DfxSignalTest005: end.";
}
}
