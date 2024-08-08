/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>

#include <dlfcn.h>
#include <fcntl.h>
#include <securec.h>
#include <sys/wait.h>
#include <unistd.h>

#include "async_stack.h"
#include "dfx_test_util.h"
#include "elapsed_time.h"
#include "fp_unwinder.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "AsyncStackTest"
#define LOG_DOMAIN 0xD002D11

class AsyncStackTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void AsyncStackTest::SetUpTestCase()
{}

void AsyncStackTest::TearDownTestCase()
{}

void AsyncStackTest::SetUp()
{}

void AsyncStackTest::TearDown()
{}

/**
 * @tc.name: AsyncStackTest001
 * @tc.desc: test GetStackId()
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest001: start.";
    SetStackId(1);
    auto res = GetStackId();
    GTEST_LOG_(INFO) << "res: " << res;
#if defined(__arm__)
    ASSERT_EQ(0, res);
#elif defined(__aarch64__)
    ASSERT_NE(0, res);
#endif

    GTEST_LOG_(INFO) << "AsyncStackTest001: end.";
}

/**
 * @tc.name: AsyncStackTest002
 * @tc.desc: test CollectAsyncStack()
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest002: start.";
    auto ret = CollectAsyncStack();
#if defined(__aarch64__)
    ASSERT_NE(0, ret);
#else
    ASSERT_EQ(0, ret);
#endif
    GTEST_LOG_(INFO) << "AsyncStackTest002: end.";
}

/**
 * @tc.name: AsyncStackTest003
 * @tc.desc: test UnwindFallback Function
 * @tc.type: FUNC
 */
HWTEST_F(AsyncStackTest, AsyncStackTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "AsyncStackTest003: start.";
    const int32_t maxSize = 16;
    uintptr_t pcs[maxSize] = {0};
    int32_t skipFrameNum = 2;
    std::thread (FpUnwinder::Unwind, pcs, maxSize, skipFrameNum).join();
    int32_t ret = FpUnwinder::UnwindFallback(pcs, maxSize, skipFrameNum);
    ASSERT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "AsyncStackTest003: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
