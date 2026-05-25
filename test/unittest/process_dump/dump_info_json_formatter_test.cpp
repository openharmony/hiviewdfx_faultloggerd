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
#include <vector>

#include "dfx_buffer_writer.h"
#include "dfx_define.h"
#include "dfx_frame.h"
#include "dfx_process.h"
#include "dfx_thread.h"
#include "dump_info_json_formatter.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DumpInfoJsonFormatterTest : public testing::Test {
public:
    static void SetUpTestCase();
    void SetUp() override;
    static int WriteLogFunc(int32_t fd, const char *buf, size_t len);
    static string result;
};
} // namespace HiviewDFX
} // namespace OHOS

string DumpInfoJsonFormatterTest::result = "";

void DumpInfoJsonFormatterTest::SetUpTestCase()
{
    result = "";
}

void DumpInfoJsonFormatterTest::SetUp()
{
    result = "";
    DfxBufferWriter::GetInstance().SetWriteFunc(DumpInfoJsonFormatterTest::WriteLogFunc);
}

int DumpInfoJsonFormatterTest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    DumpInfoJsonFormatterTest::result.append(string(buf, len));
    return 0;
}

namespace {
/**
 * @tc.name: GetJsonFormatInfoDumpCatchType
 * @tc.desc: test GetJsonFormatInfo producing json output for dump catch type request
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, GetJsonFormatInfo_DumpCatchType_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest001: start.";
    pid_t pid = getpid();
    pid_t tid = gettid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = tid;
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_process");
    process.InitKeyThread(request);
    Unwinder unwinder;
    unwinder.UnwindLocal();
    process.GetKeyThread()->SetFrames(unwinder.GetFrames());

    string jsonInfo;
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
#ifdef is_ohos_lite
    ASSERT_EQ(ret, false);
#else
    ASSERT_EQ(ret, true);
    ASSERT_FALSE(jsonInfo.empty());
    ASSERT_TRUE(jsonInfo.find("dump_result") != string::npos);
    ASSERT_TRUE(jsonInfo.find("dump_threads") != string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest001: end.";
}

/**
 * @tc.name: GetJsonFormatInfoNonDumpCatchType
 * @tc.desc: test GetJsonFormatInfo producing json output for non-dump catch type request
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, GetJsonFormatInfo_NonDumpCatchType_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest002: start.";
    pid_t pid = getpid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_CPP_CRASH;
    request.pid = pid;
    request.tid = gettid();

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_process");

    string jsonInfo;
#ifdef is_ohos_lite
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, false);
#else
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("dump_result") == string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest002: end.";
}

/**
 * @tc.name: GetJsonFormatInfoNullKeyThread
 * @tc.desc: test GetJsonFormatInfo with null key thread producing default values
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, GetJsonFormatInfo_NullKeyThread_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest003: start.";
    pid_t pid = getpid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = gettid();
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_process");

    string jsonInfo;
#ifdef is_ohos_lite
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, false);
#else
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("\"thread_name\":\"\"") != string::npos);
    ASSERT_TRUE(jsonInfo.find("\"tid\":0") != string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest003: end.";
}

/**
 * @tc.name: GetJsonFormatInfoWithOtherThreads
 * @tc.desc: test GetJsonFormatInfo with other threads in json output
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, GetJsonFormatInfo_WithOtherThreads_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest004: start.";
    pid_t pid = getpid();
    pid_t tid = gettid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = tid;
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_process");
    process.InitKeyThread(request);
    Unwinder unwinder;
    unwinder.UnwindLocal();
    process.GetKeyThread()->SetFrames(unwinder.GetFrames());

    auto otherThread = DfxThread::Create(pid, pid, pid, false);
    otherThread->SetFrames(unwinder.GetFrames());
    process.GetOtherThreads().push_back(otherThread);

    string jsonInfo;
#ifdef is_ohos_lite
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, false);
#else
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("thread_name") != string::npos);
#endif
    process.ClearOtherThreads();
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest004: end.";
}

/**
 * @tc.name: FillFramesJsonJsFrame
 * @tc.desc: test FillFramesJson formatting JS frame info in json output
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, FillFramesJson_JsFrame_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest005: start.";
#ifdef is_ohos_lite
    GTEST_LOG_(INFO) << "Skip on lite platform.";
#else
    pid_t pid = getpid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = gettid();
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_js");
    process.InitKeyThread(request);

    vector<DfxFrame> frames;
    DfxFrame jsFrame;
    jsFrame.isJsFrame = true;
    jsFrame.mapName = "test.js";
    jsFrame.packageName = "com.test.app";
    jsFrame.funcName = "testFunc";
    jsFrame.line = 10;
    jsFrame.column = 5;
    frames.push_back(jsFrame);
    process.GetKeyThread()->SetFrames(frames);

    string jsonInfo;
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("\"file\":\"test.js\"") != string::npos);
    ASSERT_TRUE(jsonInfo.find("\"packageName\":\"com.test.app\"") != string::npos);
    ASSERT_TRUE(jsonInfo.find("\"symbol\":\"testFunc\"") != string::npos);
    ASSERT_TRUE(jsonInfo.find("\"line\":10") != string::npos);
    ASSERT_TRUE(jsonInfo.find("\"column\":5") != string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest005: end.";
}

/**
 * @tc.name: FillFramesJsonNativeFrame
 * @tc.desc: test FillFramesJson formatting native frame info in json output
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, FillFramesJson_NativeFrame_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest006: start.";
#ifdef is_ohos_lite
    GTEST_LOG_(INFO) << "Skip on lite platform.";
#else
    pid_t pid = getpid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = gettid();
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_native");
    process.InitKeyThread(request);

    vector<DfxFrame> frames;
    DfxFrame nativeFrame;
    nativeFrame.isJsFrame = false;
    nativeFrame.relPc = 0x1234;
    nativeFrame.funcName = "shortFunc";
    nativeFrame.funcOffset = 8;
    nativeFrame.mapName = "/system/lib/test.so";
    nativeFrame.buildId = "abc123";
    frames.push_back(nativeFrame);
    process.GetKeyThread()->SetFrames(frames);

    string jsonInfo;
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("\"symbol\":\"shortFunc\"") != string::npos);
    ASSERT_TRUE(jsonInfo.find("\"offset\":8") != string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest006: end.";
}

/**
 * @tc.name: FillFramesJsonNativeFrameLongFuncName
 * @tc.desc: test FillFramesJson formatting native frame with long function name
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, FillFramesJson_NativeFrameLongFuncName_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest007: start.";
#ifdef is_ohos_lite
    GTEST_LOG_(INFO) << "Skip on lite platform.";
#else
    pid_t pid = getpid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = gettid();
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_longfunc");
    process.InitKeyThread(request);

    vector<DfxFrame> frames;
    DfxFrame nativeFrame;
    nativeFrame.isJsFrame = false;
    nativeFrame.relPc = 0x5678;
    string longFuncName(300, 'a');
    nativeFrame.funcName = longFuncName;
    nativeFrame.funcOffset = 4;
    nativeFrame.mapName = "/system/lib/test.so";
    nativeFrame.buildId = "abc123";
    frames.push_back(nativeFrame);
    process.GetKeyThread()->SetFrames(frames);

    string jsonInfo;
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("\"symbol\":\"\"") != string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest007: end.";
}

/**
 * @tc.name: FillFramesJsonEmptyFrames
 * @tc.desc: test FillFramesJson formatting empty frames in json output
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, FillFramesJson_EmptyFrames_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest008: start.";
#ifdef is_ohos_lite
    GTEST_LOG_(INFO) << "Skip on lite platform.";
#else
    pid_t pid = getpid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = gettid();
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_empty");
    process.InitKeyThread(request);
    process.GetKeyThread()->SetFrames({});

    string jsonInfo;
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("\"frames\":[]") != string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest008: end.";
}

/**
 * @tc.name: FillThreadstatInfoNullInfo
 * @tc.desc: test FillThreadstatInfo with null thread info omitting state field
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, FillThreadstatInfo_NullInfo_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest009: start.";
#ifdef is_ohos_lite
    GTEST_LOG_(INFO) << "Skip on lite platform.";
#else
    pid_t pid = getpid();
    pid_t tid = gettid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = tid;
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_nullinfo");
    auto thread = DfxThread::Create(pid, tid, tid, false);
    thread->SetFrames({});
    process.GetKeyThread() = thread;

    string jsonInfo;
    bool ret = DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo, 0);
    ASSERT_EQ(ret, true);
    ASSERT_TRUE(jsonInfo.find("\"state\"") == string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest009: end.";
}

/**
 * @tc.name: GetJsonFormatInfoDumpErrorVariants
 * @tc.desc: test GetJsonFormatInfo producing json with different dump error result codes
 * @tc.type: FUNC
 */
HWTEST_F(DumpInfoJsonFormatterTest, GetJsonFormatInfo_DumpErrorVariants_010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest010: start.";
#ifdef is_ohos_lite
    GTEST_LOG_(INFO) << "Skip on lite platform.";
#else
    pid_t pid = getpid();
    ProcessDumpRequest request = {};
    request.type = ProcessDumpType::DUMP_TYPE_DUMP_CATCH;
    request.pid = pid;
    request.tid = gettid();
    request.nsPid = pid;

    DfxProcess process;
    process.InitProcessInfo(pid, pid, getuid(), "test_error");
    process.InitKeyThread(request);
    Unwinder unwinder;
    unwinder.UnwindLocal();
    process.GetKeyThread()->SetFrames(unwinder.GetFrames());

    string jsonInfo0;
    DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo0, 0);
    ASSERT_TRUE(jsonInfo0.find("\"dump_result\":0") != string::npos);

    string jsonInfo1;
    DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfo1, 1);
    ASSERT_TRUE(jsonInfo1.find("\"dump_result\":1") != string::npos);

    string jsonInfoNeg;
    DumpInfoJsonFormatter::GetJsonFormatInfo(request, process, jsonInfoNeg, -1);
    ASSERT_TRUE(jsonInfoNeg.find("\"dump_result\":-1") != string::npos);
#endif
    GTEST_LOG_(INFO) << "DumpInfoJsonFormatterTest010: end.";
}
}