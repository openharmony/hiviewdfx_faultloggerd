/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <thread>
#include <unistd.h>
#include <malloc.h>
#include <securec.h>
#include "dfx_instr_statistic.h"
#include "dfx_ptrace.h"
#include "dfx_regs_get.h"
#include "procinfo.h"
#include "unwinder.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxInstrStatisticTest"
#define LOG_DOMAIN 0xD002D11

class InstrStatisticTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: InstrStatisticTest001
 * @tc.desc: test InstrStatistic interface
 * @tc.type: FUNC
 */
HWTEST_F(InstrStatisticTest, InstrStatisticTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "InstrStatisticTest001: start.";
    static pid_t pid = getpid();
    static std::string elfName;
    ReadProcessName(pid, elfName);
    pid_t child = fork();
    if (child == 0) {
        GTEST_LOG_(INFO) << "elfName: " << elfName;
        DfxInstrStatistic::GetInstance().SetCurrentStatLib(elfName);
        GTEST_LOG_(INFO) << "pid: " << pid << ", ppid:" << getppid();
        auto unwinder = std::make_shared<Unwinder>(pid);
        bool unwRet = DfxPtrace::Attach(pid);
        EXPECT_EQ(true, unwRet) << "InstrStatisticTest001: Attach:" << unwRet;
        auto regs = DfxRegs::CreateRemoteRegs(pid);
        unwinder->SetRegs(regs);
        UnwindContext context;
        context.pid = pid;
        context.regs = regs;
        unwRet = unwinder->Unwind(&context);
        EXPECT_EQ(true, unwRet) << "InstrStatisticTest001: Unwind:" << unwRet;
        auto frames = unwinder->GetFrames();
        ASSERT_GT(frames.size(), 1);
        GTEST_LOG_(INFO) << "frames:\n" << Unwinder::GetFramesStr(frames);
        DfxPtrace::Detach(pid);
        std::vector<std::pair<uint32_t, uint32_t>> result;
        DfxInstrStatistic::GetInstance().DumpInstrStatResult(result);
        ASSERT_GT(result.size(), 0);
        for (size_t i = 0; i < result.size(); ++i) {
            GTEST_LOG_(INFO) << result[i].first << result[i].second;
        }
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    ASSERT_EQ(status, 0);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "InstrStatisticTest001: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS