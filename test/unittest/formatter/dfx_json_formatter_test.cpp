/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include "dfx_json_formatter.h"

using namespace testing::ext;
using namespace OHOS::HiviewDFX;

namespace OHOS {
namespace HiviewDFX {

class DfxJsonFormatterTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

#ifdef __aarch64__
// Test data
const char* MOCK_KERNEL_VALID_STACK = R"(Thread info:
 name=main, tid=12926, state=BLOCKED, sctime=651.8, tcb_cref=502c50, pid=12926, ppid=635
[actv=0]
Stack backtrace (pname=/system/bin/main, pid=12926):
[0000000000173b8c]<???+0x0/0x0> (/system/lib/ld-musl-aarch64.so.1)
[000000000002fab0]<???+0x0/0x0> (/system/lib64/chipset-sdk-sp/libeventhandler.z.so)
[00000000000cbb44]<???+0x0/0x0> (/system/lib64/platformsdk/libappkit_native.z.so)
[0000000000006c34]<???+0x0/0x0> (/system/lib64/appspawn/appspawn/libappspawn_ace.z.so)
[00000000000b03ec]<???+0x0/0x0> (/system/lib/ld-musl-aarch64.so.1)
[base actv dumped]
state=S, utime=17, stime=21, priority=-14, nice=-10, clk=100
Thread info:
 name=OS_IPC_0_13096, tid=13096, state=BLOCKED, sctime=651.8, tcb_cref=502c50, pid=12926, ppid=635
[actv=0]
Stack backtrace (pname=/system/bin/main, pid=12926):
[0000000000193b2c]<???+0x0/0x0> (/system/lib/ld-musl-aarch64.so.1)
[000000000000f02c]<???+0x0/0x0> (/system/lib64/platformsdk/libipc_common.z.so)
[0000000000071c70]<???+0x0/0x0> (/system/lib64/platformsdk/libipc_single.z.so)
[00000000001daaa0]<???+0x0/0x0> (/system/lib/ld-musl-aarch64.so.1)
[base actv dumped]
state=S, utime=1, stime=1, priority=0, nice=-20, clk=100
)";

const char* MOCK_KERNEL_INVALID_STACK = R"(Thread info:
 name=main, tid=12926, state=BLOCKED, sctime=651.8, tcb_cref=502c50, pid=12926, ppid=635
[actv=0]
Stack backtrace (pname=/system/bin/main, pid=12926):
[0000000000173b8c]<???+0x0/0x0> (/system/lib/ld-musl-aarch64.so.1)
[base actv dumped]
state=S, utime=17, stime=21, priority=-14, nice=-10, clk=100
Thread info:
 name=OS_IPC_0_13096, tid=13096, state=BLOCKED, sctime=651.8, tcb_cref=502c50, pid=12926, ppid=635
[actv=0]
Stack backtrace (pname=/system/bin/main, pid=12926):
[0000000000193b2c]<???+0x0/0x0> (/system/lib/ld-musl-aarch64.so.1)
[000000000000f02c]<???+0x0/0x0> (/system/lib64/platformsdk/libipc_common.z.so)
[0000000000071c70]<???+0x0/0x0> (/system/lib64/platformsdk/libipc_single.z.so)
[00000000001daaa0]<???+0x0/0x0> (/system/lib/ld-musl-aarch64.so.1)
[base actv dumped]
state=S, utime=1, stime=1, priority=0, nice=-20, clk=100
)";

const char* MOCK_USER_STACK = R"(Tid:12926, Name:main
state=S, utime=15, stime=17, priority=-14, nice=-10, clk=100
#00 pc 0000000000173b8c /system/lib/ld-musl-aarch64.so.1(81437328e14b7fba0bac6c92e21735b9)
#01 pc 000000000002fab0 /system/lib64/chipset-sdk-sp/libeventhandler.z.so(e420922c45b941e4aeddead6353246b9)
#02 pc 00000000000cbb44 /system/lib64/platformsdk/libappkit_native.z.so(c50895963cbac01443583716dccf7066)
#03 pc 0000000000006c34 /system/lib64/appspawn/appspawn/libappspawn_ace.z.so(316394d6f43f0b11785ee721703ad3f1)
#04 pc 00000000000b03ec /system/lib/ld-musl-aarch64.so.1(81437328e14b7fba0bac6c92e21735b9)
)";

/**
 * @tc.name: FormatKernelStackWithUserStackMarker
 * @tc.desc: Verify Priority 1 path (user stack extraction and replacement)
 * @tc.type: FUNC
 */
HWTEST_F(DfxJsonFormatterTest, FormatKernelStackWithUserStackMarker, TestSize.Level0)
{
    std::string kernelStack = std::string(MOCK_KERNEL_VALID_STACK) +
        "\nMain thread user stack (unsymbolized):\n" + MOCK_USER_STACK;
    std::string formattedStack;
    bool ret = DfxJsonFormatter::FormatKernelStack(kernelStack, formattedStack, false, false, "");
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << formattedStack;
    EXPECT_TRUE(formattedStack.find("libeventhandler.z.so") != std::string::npos);
    EXPECT_TRUE(formattedStack.find("81437328e14b7fba0bac6c92e21735b9") != std::string::npos);
    EXPECT_TRUE(formattedStack.find("Main thread user stack") == std::string::npos);
}

/**
 * @tc.name: FormatKernelStackKernelMoreThan3Layers
 * @tc.desc: Verify Priority 2 path (preserve kernel stack)
 * @tc.type: FUNC
 */
HWTEST_F(DfxJsonFormatterTest, FormatKernelStackKernelMoreThan3Layers, TestSize.Level1)
{
    std::string kernelStack = std::string(MOCK_KERNEL_VALID_STACK);
    std::string formattedStack;
    bool ret = DfxJsonFormatter::FormatKernelStack(kernelStack, formattedStack, false, false, "");
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << formattedStack;
    EXPECT_TRUE(formattedStack.find("libeventhandler.z.so") != std::string::npos);
    EXPECT_TRUE(formattedStack.find("81437328e14b7fba0bac6c92e21735b9") == std::string::npos);
}

/**
 * @tc.name: FormatKernelStackWithFallback
 * @tc.desc: Verify Priority 3 path (use fallback)
 * @tc.type: FUNC
 */
HWTEST_F(DfxJsonFormatterTest, FormatKernelStackWithFallback, TestSize.Level0)
{
    std::string kernelStack = std::string(MOCK_KERNEL_INVALID_STACK);
    std::string fallbackStack = std::string(MOCK_USER_STACK);
    std::string formattedStack;
    bool ret = DfxJsonFormatter::FormatKernelStack(kernelStack, formattedStack, false, false, "", fallbackStack);
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << formattedStack;
    EXPECT_TRUE(formattedStack.find("libeventhandler.z.so") != std::string::npos);
}

/**
 * @tc.name: FormatKernelStackBackwardCompatibility
 * @tc.desc: Verify existing calls work without fallback parameter
 * @tc.type: FUNC
 */
HWTEST_F(DfxJsonFormatterTest, FormatKernelStackBackwardCompatibility, TestSize.Level1)
{
    std::string kernelStack = std::string(MOCK_KERNEL_VALID_STACK) +
        "\nMain thread user stack (unsymbolized):\n" + MOCK_USER_STACK;
    std::string formattedStack;
    bool ret = DfxJsonFormatter::FormatKernelStack(kernelStack, formattedStack, true, false, "");
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << formattedStack;
    EXPECT_TRUE(formattedStack.find("libeventhandler.z.so") != std::string::npos);
    EXPECT_TRUE(formattedStack.find("81437328e14b7fba0bac6c92e21735b9") != std::string::npos);
    EXPECT_TRUE(formattedStack.find("Main thread user stack") == std::string::npos);
}
#endif
} // namespace HiviewDFX
} // namespace OHOS
