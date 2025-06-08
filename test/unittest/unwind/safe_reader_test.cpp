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

#include <algorithm>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "dfx_define.h"
#include "dfx_maps.h"
#include "safe_reader.h"
#include "smart_fd.h"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {

/**
 * @tc.name: SafeReaderTest001
 * @tc.desc: test CopyReadbaleBufSafe read stack
 * @tc.type: FUNC
 */
HWTEST(SafeReaderTest, SafeReaderTest001, TestSize.Level2)
{
    /**
     * @tc.steps: step1. read maps.
     * @tc.expected: read success
     */
    auto maps = DfxMaps::Create(getpid(), false);
    uintptr_t bottom;
    uintptr_t top;
    ASSERT_TRUE(maps->GetStackRange(bottom, top));

    std::vector<uint8_t> stackBuf;
    constexpr int stackBufferSize = 64 * 1024;
    SafeReader reader;
    stackBuf.resize(stackBufferSize);

    /**
     * @tc.steps: step2. read stack form bottom.
     * @tc.expected: read success
     */
    uintptr_t stackSize = top - bottom;
    auto ret = reader.CopyReadbaleBufSafe(reinterpret_cast<uintptr_t>(stackBuf.data()), stackBuf.size(),
                                           bottom, stackSize);
    ASSERT_EQ(ret, std::min(stackSize, stackBuf.size()));

    /**
     * @tc.steps: step3. read stack form top - 2048.
     * @tc.expected: read success
     */
    constexpr uintptr_t offset = 2048;
    ret = reader.CopyReadbaleBufSafe(reinterpret_cast<uintptr_t>(stackBuf.data()), stackBuf.size(),
                                      top - offset, top - bottom);
    ASSERT_EQ(ret, offset);

    ret = reader.CopyReadbaleBufSafe(reinterpret_cast<uintptr_t>(stackBuf.data()), stackBuf.size(),
                                      top - offset, offset);
    ASSERT_EQ(ret, offset);

    /**
     * @tc.steps: step4. read stack form top + 2048.
     * @tc.expected: read failed
     */
    ret = reader.CopyReadbaleBufSafe(reinterpret_cast<uintptr_t>(stackBuf.data()), stackBuf.size(),
                                      top + offset, offset);
    ASSERT_EQ(ret, 0);
}

/**
 * @tc.name: SafeReaderTest002
 * @tc.desc: test DfxParam class functions
 * @tc.type: FUNC
 */
HWTEST(SafeReaderTest, SafeReaderTest002, TestSize.Level2)
{
    /**
     * @tc.steps: step1. read data from /dev/random.
     * @tc.expected: read success
     */
    SmartFd fd(open("/dev/random", O_RDONLY));
    std::vector<uint8_t> srcBuf;
    std::vector<uint8_t> destBuf;
    constexpr uintptr_t socLen = 1024 * 6;
    constexpr uintptr_t destBuf1 = 1024 * 8;
    constexpr uintptr_t destBuf2 = 1024 * 3;
    srcBuf.resize(socLen, 1);
    destBuf.resize(destBuf1, 0);
    ssize_t ret = read(fd.GetFd(), srcBuf.data(), srcBuf.size());
    ASSERT_EQ(ret, srcBuf.size());

    /**
     * @tc.steps: step2. test CopyReadbaleBufSafe.
     * @tc.expected: copy success
     */
    SafeReader reader;
    ret = reader.CopyReadbaleBufSafe(reinterpret_cast<uintptr_t>(destBuf.data()), destBuf.size(),
                                           reinterpret_cast<uintptr_t>(srcBuf.data()), srcBuf.size());
    ASSERT_EQ(ret, std::min(destBuf.size(), srcBuf.size()));

    /**
     * @tc.steps: step3. compare data.
     * @tc.expected: data consistency
     */
    size_t compareLen = std::min(destBuf.size(), srcBuf.size());
    ASSERT_TRUE(std::equal(destBuf.begin(), destBuf.begin() + compareLen, srcBuf.begin()));

    /**
     * @tc.steps: step4. clear destBuf.
     */
    destBuf.clear();
    destBuf.resize(destBuf2, 0);

    /**
     * @tc.steps: step5. test CopyReadbaleBufSafe.
     * @tc.expected: copy success
     */
    ret = reader.CopyReadbaleBufSafe(reinterpret_cast<uintptr_t>(destBuf.data()), destBuf.size(),
                                      reinterpret_cast<uintptr_t>(srcBuf.data()), srcBuf.size());
    ASSERT_EQ(ret, std::min(destBuf.size(), srcBuf.size()));

    /**
     * @tc.steps: step6. compare data.
     * @tc.expected: data consistency
     */
    compareLen = std::min(destBuf.size(), srcBuf.size());
    ASSERT_TRUE(std::equal(destBuf.begin(), destBuf.begin() + compareLen, srcBuf.begin()));
}

/**
 * @tc.name: SafeReaderTest003
 * @tc.desc: test fd = -1
 * @tc.type: FUNC
 */
HWTEST(SafeReaderTest, SafeReaderTest003, TestSize.Level2)
{
    /**
     * @tc.steps: step1. close fd
     */

    SafeReader reader;
    if (reader.pfd_[PIPE_READ] > 0) {
        syscall(SYS_close, reader.pfd_[PIPE_READ]);
        reader.pfd_[PIPE_READ] = -1;
    }
    if (reader.pfd_[PIPE_WRITE] > 0) {
        syscall(SYS_close, reader.pfd_[PIPE_WRITE]);
        reader.pfd_[PIPE_WRITE] = -1;
    }

    /**
     * @tc.steps: step2. compare data.
     * @tc.expected: data consistency
     */
    ASSERT_FALSE(reader.IsReadbaleAddr(0));
}

/**
 * @tc.name: SafeReaderTest004
 * @tc.desc: test GetCurrentPageEndAddr
 * @tc.type: FUNC
 */
HWTEST(SafeReaderTest, SafeReaderTest004, TestSize.Level2)
{
    int pageSize = getpagesize();

    /**
     * @tc.steps: step1. 0 ~ (pageSize - 1) >>> pageSize
     */
    ASSERT_EQ(SafeReader::GetCurrentPageEndAddr(0), pageSize);
    ASSERT_EQ(SafeReader::GetCurrentPageEndAddr(1), pageSize);
    ASSERT_EQ(SafeReader::GetCurrentPageEndAddr(pageSize - 1), pageSize);

    /**
     * @tc.steps: step1. pageSize ~ (2*pageSize - 1) >>> 2*pageSize
     */
    ASSERT_EQ(SafeReader::GetCurrentPageEndAddr(pageSize), pageSize * 2);
    ASSERT_EQ(SafeReader::GetCurrentPageEndAddr(pageSize + 1), pageSize * 2);
    ASSERT_EQ(SafeReader::GetCurrentPageEndAddr(2 * pageSize - 1), pageSize * 2);
}
} // namespace HiviewDFX
} // namespace OHOS
