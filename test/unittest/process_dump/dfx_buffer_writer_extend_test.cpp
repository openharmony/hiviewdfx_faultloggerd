/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_dump_res.h"
#include "dfx_buffer_writer.h"
#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "smart_fd.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class BufferWriterExtendTest : public testing::Test {
public:
    void SetUp() override;
    static int WriteLogFunc(int32_t fd, const char *buf, size_t len);
    static string result;
};
} // namespace HiviewDFX
} // namespace OHOS

string BufferWriterExtendTest::result = "";

void BufferWriterExtendTest::SetUp()
{
    result = "";
    DfxBufferWriter::GetInstance().SetWriteFunc(BufferWriterExtendTest::WriteLogFunc);
}

int BufferWriterExtendTest::WriteLogFunc(int32_t fd, const char *buf, size_t len)
{
    BufferWriterExtendTest::result.append(string(buf, len));
    return len;
}

namespace {
/**
 * @tc.name: BufferWriterFinish
 * @tc.desc: test DfxBufferWriter finishing with pipe fd
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_Finish_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_Finish_001: start.";
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe2(fds, O_NONBLOCK), 0);
    auto writeFd = SmartFd{fds[1]};
    DfxBufferWriter::GetInstance().SetWriteBufFd(std::move(writeFd));
    DfxBufferWriter::GetInstance().Finish();
    close(fds[0]);
    GTEST_LOG_(INFO) << "BufferWriter_Finish_001: end.";
}

/**
 * @tc.name: BufferWriterWriteFormatMsg
 * @tc.desc: test DfxBufferWriter writing formatted message to pipe fd
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_WriteFormatMsg_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_WriteFormatMsg_002: start.";
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe2(fds, O_NONBLOCK), 0);
    auto writeFd = SmartFd{fds[1]};
    DfxBufferWriter::GetInstance().SetWriteBufFd(std::move(writeFd));
    DfxBufferWriter::GetInstance().SetWriteFunc(nullptr);
    DfxBufferWriter::GetInstance().WriteFormatMsg("Test %s %d", "format", 42);
    close(fds[0]);
    DfxBufferWriter::GetInstance().SetWriteFunc(BufferWriterExtendTest::WriteLogFunc);
    GTEST_LOG_(INFO) << "BufferWriter_WriteFormatMsg_002: end.";
}

/**
 * @tc.name: BufferWriterWriteFormatMsg
 * @tc.desc: test DfxBufferWriter writing simple formatted message via custom write func
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_WriteFormatMsg_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_WriteFormatMsg_003: start.";
    DfxBufferWriter::GetInstance().WriteFormatMsg("Simple format test: %d", 100);
    ASSERT_TRUE(result.find("Simple format test: 100") != string::npos);
    GTEST_LOG_(INFO) << "BufferWriter_WriteFormatMsg_003: end.";
}

/**
 * @tc.name: BufferWriterAppendBriefDumpInfo
 * @tc.desc: test DfxBufferWriter appending and printing brief dump info
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_AppendBriefDumpInfo_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_AppendBriefDumpInfo_004: start.";
    DfxBufferWriter::GetInstance().AppendBriefDumpInfo("Reason:SIGSEGV\nTid:1234\n");
    DfxBufferWriter::GetInstance().PrintBriefDumpInfo();
    DfxBufferWriter::GetInstance().WriteMsg("verify_still_working");
    ASSERT_TRUE(result.find("verify_still_working") != string::npos);
    GTEST_LOG_(INFO) << "BufferWriter_AppendBriefDumpInfo_004: end.";
}

/**
 * @tc.name: BufferWriterWriteFormatCrashInfo
 * @tc.desc: test DfxBufferWriter writing formatted crash info
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_WriteFormatCrashInfo_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_WriteFormatCrashInfo_005: start.";
    size_t prevLen = result.size();
    DfxBufferWriter::GetInstance().WriteFormatCrashInfo();
    ASSERT_TRUE(result.size() >= prevLen);
    GTEST_LOG_(INFO) << "BufferWriter_WriteFormatCrashInfo_005: end.";
}

/**
 * @tc.name: BufferWriterGetBufFd
 * @tc.desc: test DfxBufferWriter getting and setting buffer file descriptor
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_GetBufFd_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_GetBufFd_006: start.";
    int fdBefore = DfxBufferWriter::GetInstance().GetBufFd();
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe2(fds, O_NONBLOCK), 0);
    auto writeFd = SmartFd{fds[1]};
    DfxBufferWriter::GetInstance().SetWriteBufFd(std::move(writeFd));
    int fdAfter = DfxBufferWriter::GetInstance().GetBufFd();
    ASSERT_TRUE(fdAfter >= 0);
    ASSERT_NE(fdBefore, fdAfter);
    close(fds[0]);
    GTEST_LOG_(INFO) << "BufferWriter_GetBufFd_006: end.";
}

/**
 * @tc.name: BufferWriterWriteDumpRes
 * @tc.desc: test DfxBufferWriter writing dump result to result fd
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_WriteDumpRes_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_WriteDumpRes_007: start.";
    int resfds[2] = {-1, -1};
    ASSERT_EQ(pipe2(resfds, O_NONBLOCK), 0);
    auto resWriteFd = SmartFd{resfds[1]};
    auto resReadFd = SmartFd{resfds[0]};
    DfxBufferWriter::GetInstance().SetWriteResFd(std::move(resWriteFd));
    bool ret = DfxBufferWriter::GetInstance().WriteDumpRes(DUMP_ESUCCESS);
    EXPECT_TRUE(ret);
    DumpResMessage resMsg;
    ssize_t nread = read(resReadFd.GetFd(), &resMsg, sizeof(resMsg));
    EXPECT_EQ(nread, sizeof(resMsg));
    EXPECT_EQ(resMsg.code, DUMP_ESUCCESS);
    GTEST_LOG_(INFO) << "BufferWriter_WriteDumpRes_007: end.";
}

/**
 * @tc.name: BufferWriterCreateFileForCrash
 * @tc.desc: test DfxBufferWriter creating crash log file
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterExtendTest, BufferWriter_CreateFileForCrash_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriter_CreateFileForCrash_009: start.";
    
    string crashDir = "/log/crash/";
    bool dirExists = std::filesystem::exists(crashDir);
    if (dirExists) {
        string clearCrashFilesCmd = "rm -rf /log/crash/*";
        system(clearCrashFilesCmd.c_str());
        int32_t fd = DfxBufferWriter::GetInstance().CreateFileForCrash(getpid(), 0);
        ASSERT_TRUE(fd != INVALID_FD);
        close(fd);
    }
    GTEST_LOG_(INFO) << "BufferWriter_CreateFileForCrash_009: end.";
}
}