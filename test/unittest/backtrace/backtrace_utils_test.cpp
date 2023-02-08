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
#include <fcntl.h>
#include <thread>
#include <unistd.h>

#include <directory_ex.h>
#include <file_ex.h>
#include <hilog/log.h>
#include <string_ex.h>
#include <malloc.h>

#include "backtrace_utils.h"
#include "test_utils.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "BacktraceTest"
#define LOG_DOMAIN 0xD002D11
namespace {
/*
expected output log should be like this(aarch64):
    Backtrace: #01 pc 000000000000d2f8 /data/test/backtrace_utils_test
    Backtrace: #02 pc 000000000000d164 /data/test/backtrace_utils_test
    Backtrace: #03 pc 000000000000c86c /data/test/backtrace_utils_test
    Backtrace: #04 pc 0000000000013f88 /data/test/backtrace_utils_test
    Backtrace: #05 pc 00000000000148a4 /data/test/backtrace_utils_test
    Backtrace: #06 pc 0000000000015140 /data/test/backtrace_utils_test
    Backtrace: #07 pc 00000000000242d8 /data/test/backtrace_utils_test
    Backtrace: #08 pc 0000000000023bd8 /data/test/backtrace_utils_test
    Backtrace: #09 pc 000000000000df68 /data/test/backtrace_utils_test
    Backtrace: #10 pc 00000000000dcf74 /system/lib/ld-musl-aarch64.so.1
    Backtrace: #11 pc 000000000000c614 /data/test/backtrace_utils_test
*/
static void CheckResourceUsage(uint32_t fdCount, uint32_t mapsCount, uint64_t memCount)
{
    // check memory/fd/maps
    auto curFdCount = GetSelfFdCount();
    constexpr uint32_t extraVal = 10;
    ASSERT_LE(curFdCount, fdCount + extraVal);
    GTEST_LOG_(INFO) << "AfterTest Fd New:" << std::to_string(curFdCount);
    GTEST_LOG_(INFO) << "Fd Old:" << std::to_string(fdCount) << "\n";

    auto curMapsCount = GetSelfMapsCount();
    ASSERT_LE(curMapsCount, mapsCount + extraVal);
    GTEST_LOG_(INFO) << "AfterTest Maps New:" << std::to_string(curMapsCount);
    GTEST_LOG_(INFO) << "Maps Old:" << std::to_string(mapsCount) << "\n";

    auto curMemSize = GetSelfMemoryCount();
    constexpr double ratio = 1.5;
    ASSERT_LE(curMemSize, (memCount * ratio));
    GTEST_LOG_(INFO) << "AfterTest Memory New(KB):" << std::to_string(curMemSize);
    GTEST_LOG_(INFO) << "Memory Old(KB):" << std::to_string(memCount) << "\n";
}
}
class BacktraceUtilsTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    uint32_t fdCount;
    uint32_t mapsCount;
    uint64_t memCount;

    static uint32_t fdCountTotal;
    static uint32_t mapsCountTotal;
    static uint64_t memCountTotal;
};

uint32_t BacktraceUtilsTest::fdCountTotal = 0;
uint32_t BacktraceUtilsTest::mapsCountTotal = 0;
uint64_t BacktraceUtilsTest::memCountTotal = 0;

void BacktraceUtilsTest::SetUpTestCase()
{
    HILOG_INFO(LOG_CORE, "DfxSetUpTestCase");
    // get memory/fd/maps
    BacktraceUtilsTest::fdCountTotal = GetSelfFdCount();
    BacktraceUtilsTest::mapsCountTotal = GetSelfMapsCount();
    BacktraceUtilsTest::memCountTotal = GetSelfMemoryCount();
}

void BacktraceUtilsTest::TearDownTestCase()
{
    // check memory/fd/maps
    CheckResourceUsage(fdCountTotal, mapsCountTotal, memCountTotal);
}

void BacktraceUtilsTest::SetUp()
{
    // get memory/fd/maps
    fdCount = GetSelfFdCount();
    mapsCount = GetSelfMapsCount();
    memCount = GetSelfMemoryCount();
}

void BacktraceUtilsTest::TearDown()
{
#ifdef USE_JEMALLOC_DFX_INTF
    mallopt(M_FLUSH_THREAD_CACHE, 0);
#endif
    // check memory/fd/maps
    CheckResourceUsage(fdCount, mapsCount, memCount);
}

static void CheckBacktraceContent(const std::string& content)
{
    CheckContent(content, "#09", true);
    CheckContent(content, "backtrace_utils_test", true);
    CheckContent(content, "system", true);
#if defined(__aarch64__)
    CheckContent(content, "0000000000000000", false);
#elif defined(__arm__)
    CheckContent(content, "00000000", false);
#endif
}

static void TestGetBacktraceInterface()
{
    std::string content;
    if (!GetBacktrace(content)) {
        FAIL();
    }

    GTEST_LOG_(INFO) << content;

    if (content.empty()) {
        FAIL();
    }

    CheckBacktraceContent(content);
}

/**
 * @tc.name: BacktraceUtilsTest000
 * @tc.desc: collect resource usage without perform any test
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceUtilsTest, BacktraceUtilsTest000, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceUtilsTest001: start.";
    // Do Nothing and collect resource usage
    GTEST_LOG_(INFO) << "BacktraceUtilsTest001: end.";
}

/**
 * @tc.name: BacktraceUtilsTest001
 * @tc.desc: test log backtrace to hilog, stdout and file
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceUtilsTest, BacktraceUtilsTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceUtilsTest001: start.";
    ASSERT_EQ(true, PrintBacktrace(STDIN_FILENO));
    ASSERT_EQ(true, PrintBacktrace(STDOUT_FILENO));
    ASSERT_EQ(true, PrintBacktrace(STDERR_FILENO));
    ASSERT_EQ(true, PrintBacktrace());
    int fd = open("/data/test/testfile", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        FAIL();
        return;
    }
    ASSERT_EQ(true, PrintBacktrace(fd));
    close(fd);
    std::string content;
    if (!OHOS::LoadStringFromFile("/data/test/testfile", content)) {
        FAIL();
    }

    CheckBacktraceContent(content);
    GTEST_LOG_(INFO) << "BacktraceUtilsTest001: end.";
}

/**
 * @tc.name: BacktraceUtilsTest002
 * @tc.desc: test get backtrace
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceUtilsTest, BacktraceUtilsTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceUtilsTest002: start.";
    TestGetBacktraceInterface();
    GTEST_LOG_(INFO) << "BacktraceUtilsTest002: end.";
}

/**
 * @tc.name: BacktraceUtilsTest003
 * @tc.desc: test get backtrace 100 times
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceUtilsTest, BacktraceUtilsTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceUtilsTest003: start.";
    int32_t loopCount = 100;
    for (int32_t i = 0; i < loopCount; i++) {
        TestGetBacktraceInterface();
    }
    GTEST_LOG_(INFO) << "BacktraceUtilsTest003: end.";
}

void DoCheckBacktraceInMultiThread()
{
    std::string content;
    if (!GetBacktrace(content)) {
        FAIL();
    }

    if (content.empty()) {
        FAIL();
    }
}

/**
 * @tc.name: BacktraceUtilsTest004
 * @tc.desc: test get backtrace in multi-thread situation
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceUtilsTest, BacktraceUtilsTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BacktraceUtilsTest004: start.";
    constexpr int32_t threadCount = 50;
    std::vector<std::thread> threads(threadCount);
    for (auto it = std::begin(threads); it != std::end(threads); ++it) {
        *it = std::thread(DoCheckBacktraceInMultiThread);
    }

    for (auto&& thread : threads) {
        thread.join();
    }
    GTEST_LOG_(INFO) << "BacktraceUtilsTest004: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
