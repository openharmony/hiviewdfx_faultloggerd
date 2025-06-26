/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "dfx_coredump_service.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxCoreDumpTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {}
    void SetUp() {};
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: DfxCoreDumpTest001
 * @tc.desc: test coredump set pid, tid, vmpid
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest001, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "DfxCoreDumpTest001: start.";
    pid_t vmPid = 666; // 666
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid);
    coreDumpService.SetVmPid(vmPid);
    auto coreDumpThread = coreDumpService.GetCoreDumpThread();
    ASSERT_EQ(coreDumpThread.targetPid, pid);
    ASSERT_EQ(coreDumpThread.targetTid, tid);
    ASSERT_EQ(coreDumpThread.vmPid, vmPid);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest001: end.";
#endif
}

/**
 * @tc.name: DfxCoreDumpTest002
 * @tc.desc: test coredump
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest002, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "DfxCoreDumpTest002: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid);
    bool ret = coreDumpService.StartCoreDump();
    ASSERT_EQ(ret, false);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest002: end.";
#endif
}

/**
 * @tc.name: DfxCoreDumpTest003
 * @tc.desc: test coredump
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest003, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "DfxCoreDumpTest003: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid);
    auto bundleName = coreDumpService.GetBundleNameItem();
    ASSERT_TRUE(bundleName.empty());
    GTEST_LOG_(INFO) << "DfxCoreDumpTest003: end.";
#endif
}

/**
 * @tc.name: DfxCoreDumpTest004
 * @tc.desc: test coredump
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest004, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "DfxCoreDumpTest004: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid);
    auto fd = coreDumpService.CreateFileForCoreDump();
    ASSERT_EQ(fd, -1);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest004: end.";
#endif
}

/**
 * @tc.name: DfxCoreDumpTest005
 * @tc.desc: test coredump
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest005, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "DfxCoreDumpTest005: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid);
    bool ret = coreDumpService.CreateFile();
    ASSERT_TRUE(!ret);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest005: end.";
#endif
}

/**
 * @tc.name: DfxCoreDumpTest006
 * @tc.desc: test coredump
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest006, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "DfxCoreDumpTest006: start.";
    auto pid = getpid();
    auto tid = gettid();
    CoreDumpService coreDumpService = CoreDumpService(pid, tid);
    std::string line = "5a0eb08000-5a0eb09000 r-xp 00007000 00:00 0            /system/lib/test.z.so";
    DumpMemoryRegions region;
    coreDumpService.ObtainDumpRegion(line, region);
    ASSERT_EQ(region.memorySizeHex, 0x1000);
    GTEST_LOG_(INFO) << "DfxCoreDumpTest006: end.";
#endif
}

/**
 * @tc.name: DfxCoreDumpTest007
 * @tc.desc: test coredump when isHwasanHap_ is true
 * @tc.type: FUNC
 */
HWTEST_F(DfxCoreDumpTest, DfxCoreDumpTest007, TestSize.Level2)
{
#if defined(__aarch64__)
    GTEST_LOG_(INFO) << "DfxCoreDumpTest007: start.";
    CoreDumpService coreDumpService = CoreDumpService(getpid(), gettid());
    coreDumpService.isHwasanHap_ = true;
    ASSERT_FALSE(coreDumpService.IsDoCoredump());
    GTEST_LOG_(INFO) << "DfxCoreDumpTest007: end.";
#endif
}
}
