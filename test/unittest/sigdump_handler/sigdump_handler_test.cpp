/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include <csignal>
#include <map>
#include <securec.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/prctl.h>

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_dump_catcher.h"
#include "dfx_sigdump_handler.h"

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DfxSigDumpHandlerTest : public testing::Test {
public:
    static void TestThreadRunTask();
};

static const int TEST_THREAD_NUM = 10;
static const int TEST_SLEEP_SEC = 5;

void DfxSigDumpHandlerTest::TestThreadRunTask()
{
    std::this_thread::sleep_for(std::chrono::seconds(TEST_SLEEP_SEC));
}
/**
 * @tc.name: DfxSigDumpHandlerTest
 * @tc.desc: test DfxSigDumpHandler
 * @tc.type: FUNC
 */
HWTEST_F(DfxSigDumpHandlerTest, DfxSigDumpHandlerTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DfxSigDumpHandlerTest001: start.";
    pid_t targetPid = getpid();
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(1);
        DfxDumpCatcher dumplog;
        std::string msg = "";
        bool ret = dumplog.DumpCatch(targetPid, 0, msg);
        EXPECT_EQ(ret, true) << "DfxSigDumpHandlerTest001 Failed";
        std::string log[] = {"Pid:", "Timestamp", "test_sigdump_handler", "#00"};
        log[0] = log[0] + std::to_string(targetPid);
        int len = sizeof(log) / sizeof(log[0]);
        int count = GetKeywordsNum(msg, log, len);
        GTEST_LOG_(INFO) << msg;
        EXPECT_EQ(count, len) << "DfxSigDumpHandlerTest001 Failed";
        CheckAndExit(HasFailure());
    } else {
        ASSERT_TRUE(InitSigDumpHandler());
        std::thread testThread[TEST_THREAD_NUM];
        for (int i = 0; i < TEST_THREAD_NUM; i++) {
            testThread[i] = std::thread(&DfxSigDumpHandlerTest::TestThreadRunTask);
        }
        for (int i = 0; i < TEST_THREAD_NUM; i++) {
            testThread[i].join();
        }
        ASSERT_TRUE(InitSigDumpHandler()); // Initialize again return true
        int status;
        wait(&status);
        ASSERT_EQ(status, 0);
        DeinitSigDumpHandler();
    }
    GTEST_LOG_(INFO) << "DfxSigDumpHandlerTest001: end.";
}

} // namespace HiviewDFX
} // namepsace OHOS
