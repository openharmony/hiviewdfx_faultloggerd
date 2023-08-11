/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <sys/utsname.h>
#include <vector>

#include "arch_util.h"
#include "unwind_define.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ArchUtilTest : public testing::Test {
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
 * @tc.name: ArchUtilTest001
 * @tc.desc: test ArchUtil functions
 * @tc.type: FUNC
 */
HWTEST_F(ArchUtilTest, ArchUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ArchUtilTest001: start.";
    utsname systemName;
#if defined(__arm__)
    if ((uname(&systemName)) != 0) {
        ASSERT_EQ(GetArchFromUname(systemName.machine), ArchType::ARCH_ARM);
    }
    ASSERT_EQ(GetCurrentArch(), ArchType::ARCH_ARM);
#elif defined(__aarch64__)
    if ((uname(&systemName)) != 0) {
        ASSERT_EQ(GetArchFromUname(systemName.machine), ArchType::ARCH_ARM64);
    }
    ASSERT_EQ(GetCurrentArch(), ArchType::ARCH_ARM64);
#elif defined(__i386__)
    if ((uname(&systemName)) != 0) {
        ASSERT_EQ(GetArchFromUname(systemName.machine), ArchType::ARCH_X86);
    }
    ASSERT_EQ(GetCurrentArch(), ArchType::ARCH_X86);
#elif defined(__x86_64__)
    if ((uname(&systemName)) != 0) {
        ASSERT_EQ(GetArchFromUname(systemName.machine), ArchType::ARCH_X86_64);
    }
    ASSER_EQ(GetCurrentArch(), ArchType::ARCH_X86_64);
#else
    if ((uname(&systemName)) != 0) {
        ASSERT_EQ(GetArchFromUname(systemName.machine), ArchType::ARCH_UNKNOWN);
    }
    ASSER_EQ(GetCurrentArch(), ArchType::ARCH_UNKNOWN);
#endif
    ASSERT_EQ(GetArchName(ArchType::ARCH_X86), "X86_32");
    ASSERT_EQ(GetArchName(ArchType::ARCH_X86_64), "X86_64");
    ASSERT_EQ(GetArchName(ArchType::ARCH_ARM), "ARM");
    ASSERT_EQ(GetArchName(ArchType::ARCH_ARM64), "ARM64");
    ASSERT_EQ(GetArchName(ArchType::ARCH_UNKNOWN), "Unsupport");
    GTEST_LOG_(INFO) << "ArchUtilTest001: end.";
}

}
