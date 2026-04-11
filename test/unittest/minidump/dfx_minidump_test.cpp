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
#include <fstream>
#include <iostream>
#include <sys/wait.h>

#include "dfx_signal_handler.h"
#include "dfx_test_util.h"

#define private public

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxMiniDumpTest : public testing::Test {};

namespace {
}
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
        return;
    }
    if (pid == 0) {
        DFX_SetCrashLogConfig(SET_MINIDUMP, 1);
        abort();
    }

    int status = 0;
    waitpid(pid, &status, 0);

    auto file = WaitCreateCrashFile("minidump", pid);
    EXPECT_FALSE(file.empty()) << "minidump file should be created when enabled";

    GTEST_LOG_(INFO) << "MinidumpSetConfig001: end.";
}

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
        return;
    }
    if (pid == 0) {
        DFX_SetCrashLogConfig(SET_MINIDUMP, 0);
        abort();
    }

    int status = 0;
    waitpid(pid, &status, 0);

    auto file = WaitCreateCrashFile("minidump", pid);
    EXPECT_TRUE(file.empty()) << "minidump file should not be created when disabled";

    GTEST_LOG_(INFO) << "MinidumpSetConfig002: end.";
}

} // namespace HiviewDFX
} // namespace OHOS
