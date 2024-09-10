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
#include <sys/stat.h>
#include <ctime>
#include <securec.h>
#include <string>
#include <vector>
#include "dfx_define.h"
#include "dfx_util.h"
#include "dfx_dump_res.h"
#include "dfx_log.h"
#include "string_util.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class CommonTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

static void getLibPathsBySystemBits(vector<std::string> &filePaths)
{
    std::vector<std::string> lib64FilePaths = {
        "/system/lib64/librustc_demangle.z.so",
        "/system/lib64/libstacktrace_rust.dylib.so",
        "/system/lib64/libpanic_handler.dylib.so",
        "/system/lib64/platformsdk/libjson_stack_formatter.z.so",
        "/system/lib64/chipset-pub-sdk/libdfx_dumpcatcher.z.so",
        "/system/lib64/chipset-pub-sdk/libfaultloggerd.z.so",
        "/system/lib64/chipset-pub-sdk/libdfx_signalhandler.z.so",
        "/system/lib64/chipset-pub-sdk/libasync_stack.z.so",
        "/system/lib64/chipset-pub-sdk/libbacktrace_local.so",
        "/system/lib64/chipset-pub-sdk/libunwinder.z.so",
        "/system/lib64/chipset-pub-sdk/libdfx_procinfo.z.so",
        "/system/lib64/chipset-pub-sdk/libcrash_exception.z.so",
        "/system/lib64/module/libfaultlogger_napi.z.so"
    };
    std::vector<std::string> libFilePaths = {
        "/system/lib/librustc_demangle.z.so",
        "/system/lib/libstacktrace_rust.dylib.so",
        "/system/lib/libpanic_handler.dylib.so",
        "/system/lib/platformsdk/libjson_stack_formatter.z.so",
        "/system/lib/chipset-pub-sdk/libdfx_dumpcatcher.z.so",
        "/system/lib/chipset-pub-sdk/libfaultloggerd.z.so",
        "/system/lib/chipset-pub-sdk/libdfx_signalhandler.z.so",
        "/system/lib/chipset-pub-sdk/libasync_stack.z.so",
        "/system/lib/chipset-pub-sdk/libbacktrace_local.so",
        "/system/lib/chipset-pub-sdk/libunwinder.z.so",
        "/system/lib/chipset-pub-sdk/libdfx_procinfo.z.so",
        "/system/lib/chipset-pub-sdk/libcrash_exception.z.so",
        "/system/lib/module/libfaultlogger_napi.z.so"
    };
#ifdef __LP64__
    filePaths.insert(filePaths.end(), lib64FilePaths.begin(), lib64FilePaths.end());
#else
    filePaths.insert(filePaths.end(), libFilePaths.begin(), libFilePaths.end());
#endif
}

namespace {
#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxCommonTest"
#endif

/**
 * @tc.name: DfxUtilTest001
 * @tc.desc: test DfxUtil GetCurrentTimeStr
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest001: start.";
    time_t now = time(nullptr);
    std::string timeStr = GetCurrentTimeStr(static_cast<uint64_t>(now));
    GTEST_LOG_(INFO) << timeStr;
    ASSERT_NE(timeStr, "invalid timestamp\n");
    now += 100000; // 100000 : test time offset
    timeStr = GetCurrentTimeStr(static_cast<uint64_t>(now));
    GTEST_LOG_(INFO) << timeStr;
    ASSERT_NE(timeStr, "invalid timestamp\n");
    GTEST_LOG_(INFO) << "DfxUtilTest001: end.";
}

/**
 * @tc.name: DfxUtilTest002
 * @tc.desc: test DfxUtil TrimAndDupStr
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest002: start.";
    std::string testStr = " abcd  ";
    std::string resStr;
    bool ret = TrimAndDupStr(testStr, resStr);
    ASSERT_EQ(ret, true);
    ASSERT_EQ(resStr, "abcd");
    GTEST_LOG_(INFO) << "DfxUtilTest002: end.";
}

/**
 * @tc.name: DfxDumpResTest001
 * @tc.desc: test DfxDumpRes functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxDumpResTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpResTest001: start.";
    int32_t res = DUMP_ESUCCESS;
    std::string result = DfxDumpRes::ToString(res);
    ASSERT_EQ(result, "0 ( no error )");
    GTEST_LOG_(INFO) << "DfxDumpResTest001: end.";
}

/**
 * @tc.name: DfxLogTest001
 * @tc.desc: test DfxLog functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxLogTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxLogTest001: start.";
    InitDebugFd(STDERR_FILENO);
    EXPECT_FALSE(CheckDebugLevel());
    GTEST_LOG_(INFO) << "DfxLogTest001: end.";
}

/**
 * @tc.name: StringUtilTest001
 * @tc.desc: test StartsWith functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, StringUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "StringUtilTest001: start.";
    std::string start = "[anon:ArkTS Code2024]";
    bool ret = StartsWith(start, "[anon:ArkTS Code");
    EXPECT_TRUE(ret);
    std::string end = "test.hap";
    ret = EndsWith(end, ".hap");
    EXPECT_TRUE(ret);
    GTEST_LOG_(INFO) << "StringUtilTest001: end.";
}

/**
 * @tc.name: ROMBaselineTest001
 * @tc.desc: test the ROM baseline of the faultloggerd component
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, ROMBaselineTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ROMBaselineTest001: start.";
    std::vector<std::string> filePaths = {
        "/system/etc/faultlogger.conf",
        "/system/etc/param/faultloggerd.para",
        "/system/etc/param/faultloggerd.para.dac",
        "/system/etc/init/faultloggerd.cfg",
        "/system/etc/hiview/extract_rule.json",
        "/system/etc/hiview/compose_rule.json",
        "/system/bin/processdump",
        "/system/bin/faultloggerd",
        "/system/bin/dumpcatcher",
        "/system/bin/crashvalidator"
    };
    getLibPathsBySystemBits(filePaths);
    struct stat fileStat;
    long long totalSize = 0;
    constexpr long long sectorSize = 4;
    for (const auto &filePath : filePaths) {
        int ret = stat(filePath.c_str(), &fileStat);
        if (ret != 0) {
            printf("Failed to get file size of %s\n", filePath.c_str());
        } else {
            long long size = fileStat.st_size / 1024;
            size = size + sectorSize - size % sectorSize;
            printf("File size of %s is %lldKB\n", filePath.c_str(), size);
            totalSize += size;
        }
    }
    printf("Total file size is %lldKB\n", totalSize);
    constexpr long long baseline = 1120;
    EXPECT_LT(totalSize, baseline);
    GTEST_LOG_(INFO) << "ROMBaselineTest001: end.";
}
}
} // namespace HiviewDFX
} // namespace OHOS