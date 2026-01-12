/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include <csignal>
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dfx_exit_hook.h"

using namespace testing::ext;
using namespace std;
namespace {
class DfxFuncHookUnitTest : public testing::Test {
public:
    void SetUp() override;
};

void DfxFuncHookUnitTest::SetUp(void)
{
    StartHookExitFunc();
}
/**
 * @tc.name: FuncHookTest001
 * @tc.desc: fork a child process and exit with calling _exit
 * @tc.type: FUNC
 */
HWTEST_F(DfxFuncHookUnitTest, FuncHookTest001, TestSize.Level3)
{
    GTEST_LOG_(INFO) << "FuncHookTest001: start.";
    const int retCode = 99;
    pid_t pid = fork();
    bool isSuccess = pid >= 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
    } else {
        if (pid == 0) {
            exit(retCode);
        } else {
            int status;
            int ret = waitpid(pid, &status, 0);
            printf("child ret with pid:%d status:%d\n", ret, status);
            if (WIFEXITED(status)) {
                int code = WEXITSTATUS(status);
                printf("Exit code was %d\n", code);
                EXPECT_EQ(code, retCode);
            }
        }
        GTEST_LOG_(INFO) << "FuncHookTest001: end.";
    }
}

/**
 * @tc.name: FuncHookTest002
 * @tc.desc: fork a child process and kill it in parent process
 * @tc.type: FUNC
 */
HWTEST_F(DfxFuncHookUnitTest, FuncHookTest002, TestSize.Level3)
{
    GTEST_LOG_(INFO) << "FuncHookTest002: start.";
    pid_t pid = fork();
    bool isSuccess = pid >= 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
    } else {
        if (pid == 0) {
            while (true) {
                sleep(1);
            }
        } else {
            printf("child pid:%d\n", pid);
            kill(pid, SIGKILL);
            int status;
            int ret = waitpid(pid, &status, 0);
            printf("child ret with pid:%d status:%d\n", ret, status);
            if (WIFSIGNALED(status)) {
                int signal = WTERMSIG(status);
                printf("Exit signal was %d\n", signal);
                EXPECT_EQ(signal, SIGKILL);
            }
        }
        GTEST_LOG_(INFO) << "FuncHookTest002: end.";
    }
}
}
