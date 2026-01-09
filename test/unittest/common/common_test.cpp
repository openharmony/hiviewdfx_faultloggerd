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
#include "dfx_test_util.h"
#include "elapsed_time.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class CommonTest : public testing::Test {};

static void getLibPathsBySystemBits(vector<std::string> &filePaths)
{
    std::vector<std::string> lib64FilePaths = {
        "/system/lib64/libstacktrace_rust.dylib.so",
        "/system/lib64/libpanic_handler.dylib.so",
        "/system/lib64/platformsdk/libdfx_signalhandler.z.so",
        "/system/lib64/platformsdk/libjson_stack_formatter.z.so",
        "/system/lib64/chipset-sdk/libcrash_exception.z.so",
        "/system/lib64/chipset-sdk-sp/librustc_demangle.z.so",
        "/system/lib64/chipset-sdk-sp/libasync_stack.z.so",
        "/system/lib64/chipset-sdk-sp/libdfx_dumpcatcher.z.so",
        "/system/lib64/chipset-sdk-sp/libfaultloggerd.z.so",
        "/system/lib64/chipset-sdk-sp/libbacktrace_local.so",
        "/system/lib64/chipset-sdk-sp/libunwinder.z.so",
        "/system/lib64/chipset-sdk-sp/libdfx_procinfo.z.so",
        "/system/lib64/module/libfaultlogger_napi.z.so"
    };
    std::vector<std::string> libFilePaths = {
        "/system/lib/libstacktrace_rust.dylib.so",
        "/system/lib/libpanic_handler.dylib.so",
        "/system/lib/platformsdk/libdfx_signalhandler.z.so",
        "/system/lib/platformsdk/libjson_stack_formatter.z.so",
        "/system/lib/chipset-sdk/libcrash_exception.z.so",
        "/system/lib/chipset-sdk-sp/librustc_demangle.z.so",
        "/system/lib/chipset-sdk-sp/libasync_stack.z.so",
        "/system/lib/chipset-sdk-sp/libdfx_dumpcatcher.z.so",
        "/system/lib/chipset-sdk-sp/libfaultloggerd.z.so",
        "/system/lib/chipset-sdk-sp/libbacktrace_local.so",
        "/system/lib/chipset-sdk-sp/libunwinder.z.so",
        "/system/lib/chipset-sdk-sp/libdfx_procinfo.z.so",
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
    std::string resStr;
    ASSERT_FALSE(TrimAndDupStr(nullptr, 0, resStr));
    constexpr auto testEmptyStr = "  ";
    ASSERT_FALSE(TrimAndDupStr(testEmptyStr, 0, resStr));
    ASSERT_FALSE(TrimAndDupStr(testEmptyStr, resStr));
    constexpr auto testStr = " abcd  ";
    ASSERT_TRUE(TrimAndDupStr(testStr, resStr));
    ASSERT_EQ(resStr, "abcd");
    GTEST_LOG_(INFO) << "DfxUtilTest002: end.";
}

#if is_ohos && !is_mingw
/**
 * @tc.name: DfxUtilTest003
 * @tc.desc: test DfxUtil ReadProcMemByPid
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest003: start.";
    size_t size = 4096 * 2048;
    char *p = static_cast<char *>(malloc(size));
    ASSERT_EQ(memset_s(p, size, '0', size), 0);
    *(p + size - 2) = 'O';
    *(p + size - 1) = 'K';
    std::vector<char> data(size);
    ASSERT_EQ(ReadProcMemByPid(getpid(), reinterpret_cast<uintptr_t>(p), data.data(), size), size);
    ASSERT_EQ(data[size - 2], 'O') << data[size - 2];
    ASSERT_EQ(data[size - 1], 'K') << data[size - 1];
    free(p);
    GTEST_LOG_(INFO) << "DfxUtilTest003: end.";
}
#endif

/**
 * @tc.name: DfxUtilTest004
 * @tc.desc: test DfxUtil CheckAndExit
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest004: start.";
    int pid = fork();
    if (pid == 0) {
        EXPECT_EQ(pid, 0);
        CheckAndExit(HasFailure());
    } else if (pid < 0) {
        GTEST_LOG_(INFO) << "DfxUtilTest004 fail. ";
    } else {
        int status;
        wait(&status);
        ASSERT_EQ(status, 0);
    }
    GTEST_LOG_(INFO) << "DfxUtilTest004: end.";
}

/**
 * @tc.name: DfxUtilTest005
 * @tc.desc: test DfxUtil CheckAndExit
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest005: start.";
    int pid = fork();
    if (pid == 0) {
        EXPECT_NE(pid, 0);
        CheckAndExit(HasFailure());
    } else if (pid < 0) {
        GTEST_LOG_(INFO) << "DfxUtilTest005 fail. ";
    } else {
        int status;
        wait(&status);
        ASSERT_NE(status, 0);
    }
    GTEST_LOG_(INFO) << "DfxUtilTest005: end.";
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
 * @tc.name: ElapsedTimeTest001
 * @tc.desc: test ElapsedTime class
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, ElapsedTimeTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ElapsedTimeTest001: start.";

    ElapsedTime t1;
    ElapsedTime t2("t2 timeout", 1);
    ElapsedTime t3("t3 timeout", 10);
    usleep(1000);
    t1.Reset();
    EXPECT_GT(t1.Elapsed(), 0);
    GTEST_LOG_(INFO) << "ElapsedTimeTest001: end.";
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
        "/system/bin/dumpcatcher"
    };
    getLibPathsBySystemBits(filePaths);
    struct stat fileStat;
    long long totalSize = 0;
    constexpr long long sectorSize = 4;
    for (const auto &filePath : filePaths) {
        int ret = stat(filePath.c_str(), &fileStat);
        if (ret != 0) {
            EXPECT_EQ(ret, 0) << "Failed to get file size of " << filePath;
        } else {
            long long size = fileStat.st_size / 1024;
            size = size + sectorSize - size % sectorSize;
            GTEST_LOG_(INFO) << "File size of " << filePath << " is " << size << "KB";
            totalSize += size;
        }
    }
    printf("Total file size is %lldKB\n", totalSize);
    constexpr long long baseline = 1435;
    // There is a 5% threshold for the baseline value
    EXPECT_LT(totalSize, static_cast<long long>(baseline * 1.05));
    GTEST_LOG_(INFO) << "ROMBaselineTest001: end.";
}

}
} // namespace HiviewDFX
} // namespace OHOS
