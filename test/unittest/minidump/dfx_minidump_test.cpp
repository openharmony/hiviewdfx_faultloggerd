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
#include <string>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sys/wait.h>

#include "dfx_signal_handler.h"
#include "dfx_test_util.h"
#include "dfx_socket_request.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxMiniDumpTest : public testing::Test {
public:
    static void SetUpTestCase()
    {
        system("setenforce 0");
    }

    static void TearDownTestCase()
    {
        system("setenforce 1");
    }
};

#if defined(__aarch64__)
/**
 * @tc.name: MinidumpSetConfig001
 * @tc.desc: enable minidump
 * @tc.type: FUNC
 */
HWTEST_F(DfxMiniDumpTest, MinidumpSetConfig001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MinidumpSetConfig001: start.";

    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "fork failed";
        FAIL();
    }
    if (pid == 0) {
        EXPECT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 1), REQUEST_SUCCESS);
        abort();
    } else {
        auto file = WaitCreateCrashFile("minidump", pid);
        EXPECT_FALSE(file.empty()) << "minidump file should be created when enabled";

        int status = 0;
        waitpid(pid, &status, 0);
    }
    GTEST_LOG_(INFO) << "MinidumpSetConfig001: end.";
}
#endif

/**
 * @tc.name: MinidumpSetConfig002
 * @tc.desc: disable minidump
 * @tc.type: FUNC
 */
HWTEST_F(DfxMiniDumpTest, MinidumpSetConfig002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MinidumpSetConfig002: start.";

    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "fork failed";
        FAIL();
    }
    if (pid == 0) {
        EXPECT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 0), REQUEST_SUCCESS);
        abort();
    } else {
        auto file = WaitCreateCrashFile("minidump", pid, 1);
        EXPECT_TRUE(file.empty()) << "minidump file should not be created when disabled";
        int status = 0;
        waitpid(pid, &status, 0);
    }

    GTEST_LOG_(INFO) << "MinidumpSetConfig002: end.";
}

/**
 * @tc.name: MinidumpSetConfig003
 * @tc.desc: test SET_MINIDUMP disabled, cppcrash log should not contain minidump config
 * @tc.type: FUNC
 */
HWTEST_F(DfxMiniDumpTest, MinidumpSetConfig003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MinidumpSetConfig003: start.";

    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "fork failed";
        FAIL();
    }
    if (pid == 0) {
        EXPECT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 0), REQUEST_SUCCESS);
        abort();
    } else {
        auto file = WaitCreateCrashFile("cppcrash", pid);
        EXPECT_FALSE(file.empty()) << "cppcrash file should be created";

        std::ifstream ifs(file);
        std::string fileContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        int status = 0;
        waitpid(pid, &status, 0);
    }

    GTEST_LOG_(INFO) << "MinidumpSetConfig003: end.";
}

/**
 * @tc.name: MinidumpSetConfig004
 * @tc.desc: test DFX_SetCrashLogConfig return value for SET_MINIDUMP
 * @tc.type: FUNC
 */
HWTEST_F(DfxMiniDumpTest, MinidumpSetConfig004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MinidumpSetConfig004: start.";

    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "fork failed";
        FAIL();
    }
    if (pid == 0) {
        ASSERT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 1), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 0), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 1), 0);
        _exit(0);
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
        ASSERT_TRUE(WIFEXITED(status));
        ASSERT_EQ(WEXITSTATUS(status), 0);
    }

    GTEST_LOG_(INFO) << "MinidumpSetConfig004: end.";
}

/**
 * @tc.name: MinidumpSetConfig005
 * @tc.desc: test enable then disable minidump, minidump file should not be created
 * @tc.type: FUNC
 */
HWTEST_F(DfxMiniDumpTest, MinidumpSetConfig005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MinidumpSetConfig005: start.";

    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "fork failed";
        FAIL();
    }
    if (pid == 0) {
        EXPECT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 1), REQUEST_SUCCESS);
        DFX_SetCrashLogConfig(SET_MINIDUMP, 0);
        abort();
    } else {
        auto file = WaitCreateCrashFile("minidump", pid, 1);
        EXPECT_TRUE(file.empty()) << "minidump file should not be created when disabled after enabling";
        int status = 0;
        waitpid(pid, &status, 0);
    }
    GTEST_LOG_(INFO) << "MinidumpSetConfig005: end.";
}

/**
 * @tc.name: MinidumpSetConfig006
 * @tc.desc: test combined crash log configs with minidump enabled
 * @tc.type: FUNC
 */
HWTEST_F(DfxMiniDumpTest, MinidumpSetConfig006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "MinidumpSetConfig006: start.";

    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "fork failed";
        FAIL();
    }
    if (pid == 0) {
        EXPECT_EQ(DFX_SetCrashLogConfig(SET_MINIDUMP, 1), REQUEST_SUCCESS);
        abort();
    } else {
        auto file = WaitCreateCrashFile("cppcrash", pid);
        ASSERT_FALSE(file.empty()) << "cppcrash file should be created";

        std::ifstream ifs(file);
        std::string fileContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        EXPECT_TRUE(fileContent.find("Enable minidump log:true") != std::string::npos);
        int status = 0;
        waitpid(pid, &status, 0);
    }
    GTEST_LOG_(INFO) << "MinidumpSetConfig006: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
