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

#include "dfx_buffer_writer.h"
#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "decorative_dump_info.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DumpInfoHeaderTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void) {}
    void SetUp();
    void TearDown() {}
    static int WriteLogFunc(int32_t fd, const char *buf, int len);
    static std::string result;
};
} // namespace HiviewDFX
} // namespace OHOS

std::string DumpInfoHeaderTest::result = "";

void DumpInfoHeaderTest::SetUpTestCase(void)
{
    result = "";
}

void DumpInfoHeaderTest::SetUp(void)
{
    DfxBufferWriter::GetInstance().SetWriteFunc(DumpInfoHeaderTest::WriteLogFunc);
}

int DumpInfoHeaderTest::WriteLogFunc(int32_t fd, const char *buf, int len)
{
    DumpInfoHeaderTest::result.append(std::string(buf, len));
    return 0;
}
namespace {
/**
 * @tc.name: DumpInfoHeaderTest001
 * @tc.desc: test print cpp crash dump info header
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoHeaderTest, DumpInfoHeaderTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoHeaderTest001: start.";
    std::string msg = "test string type crashObject.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    pid_t tid = pid;
    pid_t nsPid = pid;
    uint64_t logFileCutoffSize = 1024;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_CPP_CRASH,
        .tid = tid, .pid = pid, .nsPid = pid,
        .timeStamp = GetTimeMilliSeconds(), .crashLogConfig = (logFileCutoffSize << 32) + 0x3, // 32 : high 32 bit save cutoff size
    };
#ifdef __aarch_64__
    const int moveBit = 56;
    uint8_t type = 1;
    request.crashObj = (static_cast<uintptr_t>(type) << moveBit) |
        (reinterpret_cast<uintptr_t>(msg.c_str()) & 0x00ffffffffffffff);
#endif
    siginfo_t siginfo = {
        .si_signo = SIGABRT,
        .si_code = SI_TKILL,
    };
    request.siginfo = siginfo;
    GetThreadNameByTid(request.tid, request.threadName, sizeof(request.threadName));
    GetProcessName(request.processName, sizeof(request.processName));
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), request.processName);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    DumpInfoHeader dumpInfoHeader;
    dumpInfoHeader.Print(process, request, unwinder);
    const std::vector<std::string> keyWords = {
        "Build info:", "Enabled app log configs:", "Extend pc lr printing:true",
        "Log cut off size:1024B", "Simplify maps printing:true", "Pid:", "Uid:",
        "Process name", "Timestamp:", "Process life time:", "Process Memory(kB):", "Reason:Signal:SIGABRT(SI_TKILL)",
#ifdef __aarch_64__
        "LastFatalMessage:" + msg,
#endif
    };
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    GTEST_LOG_(INFO) << "DumpInfoHeaderTest001: end.";
}

/**
 * @tc.name: DumpInfoHeaderTest002
 * @tc.desc: test print dump catch dump info header
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoHeaderTest, DumpInfoHeaderTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoHeaderTest002: start.";
    std::string msg = "test string type crashObject.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
        exit(0);
    }
    pid_t tid = pid;
    pid_t nsPid = pid;
    ProcessDumpRequest request = {
        .type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH,
        .tid = tid,
        .pid = pid,
        .nsPid = pid,
        .timeStamp = GetTimeMilliSeconds(),
    };
    GetProcessName(request.processName, sizeof(request.processName));
    DfxProcess process;
    process.InitProcessInfo(pid, nsPid, getuid(), request.processName);
    Unwinder unwinder(pid, nsPid, request.type == ProcessDumpType::DUMP_TYPE_CPP_CRASH);
    DumpInfoHeader dumpInfoHeader;
    dumpInfoHeader.Print(process, request, unwinder);
    const std::vector<std::string> keyWords = {
        "Timestamp:",
        "Pid:",
        "Uid:",
        "Process name",
    };
    for (const std::string& keyWord : keyWords) {
        ASSERT_TRUE(CheckContent(result, keyWord, true));
    }
    GTEST_LOG_(INFO) << "DumpInfoHeaderTest002: end.";
}
}
 