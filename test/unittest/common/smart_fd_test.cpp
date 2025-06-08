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
#include <fcntl.h>
#include "smart_fd.h"

using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
/**
 * @tc.name: SmartFdTest001
 * @tc.desc: test invalid.
 * @tc.type: FUNC
 */
HWTEST(SmartFdTest, SmartFdTest001, TestSize.Level0)
{
    SmartFd fd;
    ASSERT_FALSE(fd);
    ASSERT_LT(fd.GetFd(), 0);

    SmartFd fd1{-1};
    ASSERT_FALSE(fd1);
    ASSERT_LT(fd1.GetFd(), 0);
}

/**
 * @tc.name: SmartFdTest002
 * @tc.desc: test valid.
 * @tc.type: FUNC
 */
HWTEST(SmartFdTest, SmartFdTest002, TestSize.Level0)
{
    SmartFd fd(open("/dev/null", O_WRONLY));
    ASSERT_TRUE(fd);
    ASSERT_GT(fd.GetFd(), 0);

    fd.Reset();
    ASSERT_FALSE(fd);
    ASSERT_LT(fd.GetFd(), 0);

    SmartFd fd1(open("/dev/null", O_WRONLY), false);
    ASSERT_TRUE(fd1);
    ASSERT_GT(fd1.GetFd(), 0);
}

/**
 * @tc.name: SmartFdTest003
 * @tc.desc: test move construct.
 * @tc.type: FUNC
 */
HWTEST(SmartFdTest, SmartFdTest003, TestSize.Level0)
{
    SmartFd fd(open("/dev/null", O_WRONLY));
    ASSERT_TRUE(fd);

    SmartFd fd1(std::move(fd));
    ASSERT_TRUE(fd1);
    ASSERT_FALSE(fd);
}

/**
 * @tc.name: SmartFdTest004
 * @tc.desc: test move assign.
 * @tc.type: FUNC
 */
HWTEST(SmartFdTest, SmartFdTest004, TestSize.Level0)
{
    SmartFd fd(open("/dev/null", O_WRONLY));
    ASSERT_TRUE(fd);

    SmartFd fd1;
    ASSERT_FALSE(fd1);

    fd1 = std::move(fd);
    ASSERT_TRUE(fd1);
    ASSERT_FALSE(fd);

    SmartFd fd2(open("/dev/null", O_WRONLY), false);
    ASSERT_TRUE(fd2);

    fd1 = std::move(fd2);
    ASSERT_TRUE(fd1);
    ASSERT_FALSE(fd2);
}

/**
 * @tc.name: SmartFdTest005
 * @tc.desc: test close.
 * @tc.type: FUNC
 */
HWTEST(SmartFdTest, SmartFdTest005, TestSize.Level2)
{
    SmartFd fd(open("/dev/null", O_WRONLY));
    ASSERT_TRUE(fd);
    int handle = fd.GetFd();
    {
        SmartFd {std::move(fd)};
        ASSERT_FALSE(fd);
    }
    // double close
    ASSERT_LT(close(handle), 0);

    SmartFd fd1(open("/dev/null", O_WRONLY));
    {
        SmartFd tmp;
        tmp = std::move(fd1);
        ASSERT_TRUE(tmp);
        handle = tmp.GetFd();
    }
    // double close
    ASSERT_LT(close(handle), 0);
}

} // namespace HiviewDFX
} // namespace OHOS