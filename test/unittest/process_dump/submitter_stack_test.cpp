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

#include "async_stack.h"
#include "dfx_buffer_writer.h"
#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "decorative_dump_info.h"
#include "uv.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class SubmitterStackTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
    static int WriteLogFunc(int32_t fd, const char *buf, size_t len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string SubmitterStackTest::result = "";

void SubmitterStackTest::SetUpTestCase(void)
{
    result = "";
}

void SubmitterStackTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(SubmitterStackTest::WriteLogFunc);
}

int SubmitterStackTest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    SubmitterStackTest::result.append(std::string(buf, len));
    return 0;
}

namespace {
static bool g_done = false;

NOINLINE static void WorkCallback(uv_work_t* req) {}

NOINLINE static void AfterWorkCallback(uv_work_t* req, int status)
{
    if (!g_done) {
        uv_queue_work(req->loop, req, WorkCallback, AfterWorkCallback);
    }
}

static void TimerCallback(uv_timer_t* handle)
{
    g_done = true;
}
/**
 * @tc.name: SubmitterStackTest001
 * @tc.desc: test print submitter stack
 * @tc.type: FUNC
 */
HWTEST_F(SubmitterStackTest, SubmitterStackTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SubmitterStackTest001: start.";
    DfxInitAsyncStack();
    uv_timer_t timerHandle;
    uv_work_t work;
    uv_loop_t* loop = uv_default_loop();
    int timeout = 1000;
    uv_timer_init(loop, &timerHandle);
    uv_timer_start(&timerHandle, TimerCallback, timeout, 0);
    uv_queue_work(loop, &work, WorkCallback, AfterWorkCallback);
    uv_run(loop, UV_RUN_DEFAULT);
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_CPP_CRASH,
        .tid = pid,
        .pid = pid,
        .nsPid = pid,
        .stackId = DfxGetSubmitterStackId(),
    };
#if defined(__aarch64__)
    ASSERT_NE(request.stackId, 0);
#endif
    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "");
    process.SetVmPid(pid);
    process.InitKeyThread(request);
    Unwinder unwinder(pid, pid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    SubmitterStack submitterStack;
    submitterStack.Print(process, request, unwinder);
#if defined(__aarch64__)
    std::vector<std::string> keyWords = {
        "SubmitterStacktrace",
        "#00",
        "#01",
        "#02",
    };
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
#endif
    process.Detach();
    GTEST_LOG_(INFO) << "SubmitterStackTest001: end.";
}
}
 