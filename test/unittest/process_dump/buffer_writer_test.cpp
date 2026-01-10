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
#include "smart_fd.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class BufferWriterTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS
namespace {
/**
 * @tc.name: BufferWriterTest001
 * @tc.desc: test buffer writer
 * @tc.type: FUNC
 */
HWTEST_F(BufferWriterTest, BufferWriterTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "BufferWriterTest001: start.";
    int fds[PIPE_NUM_SZ] = {-1, -1};
    if (pipe2(fds, O_NONBLOCK) != 0) {
        GTEST_LOG_(ERROR) << "Failed to create pipe. ";
        return;
    }
    auto readFd = SmartFd{fds[PIPE_READ]};
    auto writeFd = SmartFd{fds[PIPE_WRITE]};
    if (fcntl(readFd.GetFd(), F_SETPIPE_SZ, MAX_PIPE_SIZE) < 0 ||
        fcntl(writeFd.GetFd(), F_SETPIPE_SZ, MAX_PIPE_SIZE) < 0) {
        GTEST_LOG_(ERROR) << "Failed to set pipe size. ";
    }
    std::string finishFlag = "Write finish!";
    int pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "BufferWriterTest001 fork fail. ";
    } else if (pid == 0) {
        DfxBufferWriter::GetInstance().SetWriteBufFd(std::move(writeFd));
        std::string msg(MAX_PIPE_SIZE, 'c');
        msg += finishFlag;
        DfxBufferWriter::GetInstance().WriteMsg(msg);
        _exit(0);
    }
    std::vector<char> buffer(MAX_PIPE_SIZE, 0);
    std::string msg;
    for (int i = 0; i < 2 ; i++) {
        ssize_t nread = 0;
        int savedErrno = 0;
        do {
            nread = read(readFd.GetFd(), buffer.data(), MAX_PIPE_SIZE);
            savedErrno = errno;
            if (nread == -1 && savedErrno == EAGAIN) {
                usleep(1000); // 1000 : sleep 1ms try again
            }
        } while (nread == -1 && (savedErrno == EINTR || savedErrno == EAGAIN));
        ASSERT_TRUE(nread != -1);
        msg = std::string(buffer.begin(), buffer.begin() + nread);
    }
    GTEST_LOG_(INFO) << msg;
    ASSERT_EQ(msg, finishFlag);
    GTEST_LOG_(INFO) << "BufferWriterTest001: end.";
}
}
