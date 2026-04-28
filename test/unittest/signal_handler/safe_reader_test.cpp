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
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

#include "dfx_define.h"
#include "safe_reader.h"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {

class SafeReaderTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
};

void SafeReaderTest::SetUpTestCase()
{
}

void SafeReaderTest::TearDownTestCase()
{
    DeInitPipe();
}

static size_t GetMin(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

/**
 * @tc.name: CopyReadableBufSafeTest001
 * @tc.desc: Test CopyReadableBufSafe normal copy function (destLen > srcLen scenario)
 * @tc.type: FUNC
 */
HWTEST_F(SafeReaderTest, CopyReadableBufSafeTest001, TestSize.Level2)
{
    int fd = open("/dev/urandom", O_RDONLY);
    ASSERT_GE(fd, 0);
    std::vector<uint8_t> srcBuf;
    std::vector<uint8_t> destBuf;
    constexpr size_t srcLen = 1024 * 6;
    constexpr size_t destLen = 1024 * 8;
    srcBuf.resize(srcLen);
    destBuf.resize(destLen);

    ssize_t ret = read(fd, srcBuf.data(), srcBuf.size());
    ASSERT_EQ(ret, static_cast<ssize_t>(srcLen));
    close(fd);

    ret = CopyReadableBufSafe(reinterpret_cast<uintptr_t>(destBuf.data()), destLen,
                              reinterpret_cast<uintptr_t>(srcBuf.data()), srcLen);
    ASSERT_EQ(ret, GetMin(destLen, srcLen));

    size_t compareLen = GetMin(destBuf.size(), srcBuf.size());
    ASSERT_TRUE(std::equal(destBuf.begin(), destBuf.begin() + compareLen, srcBuf.begin()));
}

/**
 * @tc.name: CopyReadableBufSafeTest002
 * @tc.desc: Test CopyReadableBufSafe edge case: destPtr equals 0
 * @tc.type: FUNC
 */
HWTEST_F(SafeReaderTest, CopyReadableBufSafeTest002, TestSize.Level2)
{
    std::vector<uint8_t> srcBuf;
    constexpr size_t srcLen = 64;
    srcBuf.resize(srcLen, 0xAB);

    auto ret = CopyReadableBufSafe(0, 0, reinterpret_cast<uintptr_t>(srcBuf.data()), srcLen);
    ASSERT_EQ(ret, 0u);
}

/**
 * @tc.name: CopyReadableBufSafeTest003
 * @tc.desc: Test CopyReadableBufSafe edge case: destLen equals 0
 * @tc.type: FUNC
 */
HWTEST_F(SafeReaderTest, CopyReadableBufSafeTest003, TestSize.Level2)
{
    std::vector<uint8_t> srcBuf;
    std::vector<uint8_t> destBuf;
    constexpr size_t srcLen = 128;
    constexpr size_t destLen = 0;
    srcBuf.resize(srcLen, 0xCD);
    destBuf.resize(destLen);

    auto ret = CopyReadableBufSafe(reinterpret_cast<uintptr_t>(destBuf.data()), destLen,
                                   reinterpret_cast<uintptr_t>(srcBuf.data()), srcLen);
    ASSERT_EQ(ret, 0u);
}

/**
 * @tc.name: CopyReadableBufSafeTest004
 * @tc.desc: Test CopyReadableBufSafe normal copy function (destLen < srcLen scenario)
 * @tc.type: FUNC
 */
HWTEST_F(SafeReaderTest, CopyReadableBufSafeTest004, TestSize.Level2)
{
    int fd = open("/dev/urandom", O_RDONLY);
    ASSERT_GE(fd, 0);
    std::vector<uint8_t> srcBuf;
    std::vector<uint8_t> destBuf;
    constexpr size_t srcLen = 1024 * 10;
    constexpr size_t destLen = 1024;
    srcBuf.resize(srcLen);
    destBuf.resize(destLen);

    ssize_t ret = read(fd, srcBuf.data(), srcBuf.size());
    ASSERT_EQ(ret, static_cast<ssize_t>(srcLen));
    close(fd);

    ret = CopyReadableBufSafe(reinterpret_cast<uintptr_t>(destBuf.data()), destLen,
                              reinterpret_cast<uintptr_t>(srcBuf.data()), srcLen);
    ASSERT_EQ(ret, destLen);

    ASSERT_TRUE(std::equal(destBuf.begin(), destBuf.end(), srcBuf.begin()));
}

/**
 * @tc.name: CopyReadableBufSafeTest005
 * @tc.desc: Test CopyReadableBufSafe page-aligned reading (4KB page boundary test)
 * @tc.type: FUNC
 */
HWTEST_F(SafeReaderTest, CopyReadableBufSafeTest005, TestSize.Level2)
{
    std::vector<uint8_t> srcBuf;
    std::vector<uint8_t> destBuf;
    constexpr size_t srcLen = 4096;
    constexpr size_t destLen = 4096;
    srcBuf.resize(srcLen);
    destBuf.resize(destLen);

    for (size_t i = 0; i < srcBuf.size(); ++i) {
        srcBuf[i] = static_cast<uint8_t>((i * 3 + 17) & 0xFF);
    }

    auto ret = CopyReadableBufSafe(reinterpret_cast<uintptr_t>(destBuf.data()), destLen,
                                   reinterpret_cast<uintptr_t>(srcBuf.data()), srcLen);
    ASSERT_EQ(ret, srcLen);
    ASSERT_TRUE(std::equal(destBuf.begin(), destBuf.begin() + srcLen, srcBuf.begin()));
}

} // namespace HiviewDFX
} // namespace OHOS