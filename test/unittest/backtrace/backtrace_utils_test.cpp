/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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
#include <directory_ex.h>
#include <fcntl.h>
#include <malloc.h>
#include <string_ex.h>
#include <thread>
#include <unistd.h>

#include "backtrace_local.h"
#include "dfx_test_util.h"
#include "file_util.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxBacktraceUtilsTest"
#define LOG_DOMAIN 0xD002D11

#define TEST_TEMP_FILE "/data/test/testfile"
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

static bool CheckBacktraceContent(const std::string& content, bool fast = false)
{
    std::string existKeyWords[] = { "#04", "backtrace_utils_test", "system" };
    std::string notExistkeyWords[] = {
#if defined(__aarch64__)
        "0000000000000000",
#elif defined(__arm__)
        "00000000",
#endif
        "OHOS::HiviewDFX::GetBacktrace",
        "OHOS::HiviewDFX::PrintBacktrace"
    };

    if (!fast) {
        for (std::string keyWord : existKeyWords) {
            if (!CheckContent(content, keyWord, true)) {
                return false;
            }
        }
    }
    for (std::string keyWord : notExistkeyWords) {
        if (!CheckContent(content, keyWord, false)) {
            return false;
        }
    }
    return true;
}

static bool TestGetBacktraceInterface()
{
    std::string content;
    if (!GetBacktrace(content)) {
        return false;
    }

    GTEST_LOG_(INFO) << content;
    if (content.empty()) {
        return false;
    }

    if (!CheckBacktraceContent(content)) {
        return false;
    }
    return true;
}

static bool TestGetBacktraceFastInterface()
{
#ifdef __aarch64__
    std::string content;
    if (!GetBacktrace(content, true)) {
        return false;
    }

    GTEST_LOG_(INFO) << content;
    if (content.empty()) {
        return false;
    }

    if (!CheckBacktraceContent(content, true)) {
        return false;
    }
#endif
    return true;
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
    int fd = open(TEST_TEMP_FILE, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ(true, PrintBacktrace(fd));
    close(fd);
    std::string content;
    if (!OHOS::HiviewDFX::LoadStringFromFile(TEST_TEMP_FILE, content)) {
        FAIL();
    }
    ASSERT_EQ(CheckBacktraceContent(content), true);
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
    ASSERT_EQ(TestGetBacktraceInterface(), true);
    ASSERT_EQ(TestGetBacktraceFastInterface(), true);
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
        ASSERT_EQ(TestGetBacktraceInterface(), true);
    }
    GTEST_LOG_(INFO) << "BacktraceUtilsTest003: end.";
}

void DoCheckBacktraceInMultiThread()
{
    std::string content;
    ASSERT_TRUE(GetBacktrace(content));
    ASSERT_FALSE(content.empty());
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

/**
 * @tc.name: BacktraceUtilsTest005
 * @tc.desc: test PrintTrace to hilog, stdout and file
 * @tc.type: FUNC
 */
HWTEST_F(BacktraceUtilsTest, BacktraceUtilsTest005, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "BacktraceUtilsTest005: start.";
    ASSERT_EQ(true, PrintTrace(STDIN_FILENO));
    ASSERT_EQ(true, PrintTrace(STDOUT_FILENO));
    ASSERT_EQ(true, PrintTrace(STDERR_FILENO));
    ASSERT_EQ(true, PrintTrace());
    int fd = open(TEST_TEMP_FILE, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ(true, PrintTrace(fd));
    close(fd);
    std::string content;
    if (!OHOS::HiviewDFX::LoadStringFromFile(TEST_TEMP_FILE, content)) {
        FAIL();
    }

    ASSERT_EQ(CheckBacktraceContent(content), true);
    GTEST_LOG_(INFO) << "BacktraceUtilsTest005: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
