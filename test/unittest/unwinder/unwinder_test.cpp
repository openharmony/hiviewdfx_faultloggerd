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
#include <hilog/log.h>
#include <malloc.h>
#include <libunwind_i-ohos.h>
#include <libunwind.h>
#include <securec.h>

#include "dfx_config.h"
#include "elapsed_time.h"
#include "dwarf_unwinder.h"
#include "fp_unwinder.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "UnwinderTest"
#define LOG_DOMAIN 0xD002D11

class UnwinderTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void UnwinderTest::SetUpTestCase()
{
}

void UnwinderTest::TearDownTestCase()
{
}

void UnwinderTest::SetUp()
{
}

void UnwinderTest::TearDown()
{
}

/**
 * @tc.name: DfxConfigTest001
 * @tc.desc: test DfxConfig class functions
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, DfxConfigTest001, TestSize.Level2)
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
 * @tc.name: UnwinderTest000
 * @tc.desc: test dwarf unwinder UnwindWithContext
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest000, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest000: start.";
    ElapsedTime counter;
    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);
    pid_t child = fork();
    if (child == 0) {
        unw_addr_space_t as;
        unw_init_local_address_space(&as);
        if (as == nullptr) {
            FAIL() << "Failed to init address space.";
            return;
        }

        auto symbol = std::make_shared<DfxSymbols>();
        ElapsedTime counter2;
        DwarfUnwinder unwinder;
        ASSERT_EQ(true, unwinder.UnwindWithContext(as, context, symbol, 0));
        GTEST_LOG_(INFO) << "ChildProcessElapse:" << counter2.Elapsed();
        const auto& frames = unwinder.GetFrames();
        ASSERT_GT(frames.size(), 0);
        unw_destroy_local_address_space(as);
        _exit(0);
    }
    GTEST_LOG_(INFO) << "CurrentThreadElapse:" << counter.Elapsed();

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderTest000: end.";
}

/**
 * @tc.name: UnwinderTest001
 * @tc.desc: test fp unwinder UnwindWithContext
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest001: start.";
#ifdef __aarch64__
    ElapsedTime counter;
    unw_context_t context;
    (void)memset_s(&context, sizeof(unw_context_t), 0, sizeof(unw_context_t));
    unw_getcontext(&context);
    pid_t child = fork();
    if (child == 0) {
        ElapsedTime counter2;
        FpUnwinder unwinder;
        ASSERT_EQ(true, unwinder.UnwindWithContext(context, 0));
        GTEST_LOG_(INFO) << "ChildProcessElapse:" << counter2.Elapsed();
        const auto& frames = unwinder.GetFrames();
        ASSERT_GT(frames.size(), 0);
        _exit(0);
    }
    GTEST_LOG_(INFO) << "CurrentThreadElapse:" << counter.Elapsed();

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
#endif
    GTEST_LOG_(INFO) << "UnwinderTest001: end.";
}

/**
 * @tc.name: UnwinderTest002
 * @tc.desc: test fp unwinder Unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest002: start.";
#ifdef __aarch64__
    ElapsedTime counter;
    pid_t child = fork();
    if (child == 0) {
        ElapsedTime counter2;
        FpUnwinder unwinder;
        ASSERT_EQ(true, unwinder.Unwind(0));
        GTEST_LOG_(INFO) << "ChildProcessElapse:" << counter2.Elapsed();
        const auto& frames = unwinder.GetFrames();
        ASSERT_GT(frames.size(), 0);
        _exit(0);
    }
    GTEST_LOG_(INFO) << "CurrentThreadElapse:" << counter.Elapsed();

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
#endif
    GTEST_LOG_(INFO) << "UnwinderTest002: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
