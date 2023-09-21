/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <thread>
#include <unistd.h>
#include <hilog/log.h>
#include <malloc.h>
#include <securec.h>
#include "unwinder.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxUnwinderTest"
#define LOG_DOMAIN 0xD002D11

class UnwinderTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: UnwinderTest001
 * @tc.desc: test unwinder local unwind
 * @tc.type: FUNC
 */
HWTEST_F(UnwinderTest, UnwinderTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "UnwinderTest001: start.";
    pid_t child = fork();
    if (child == 0) {
        Unwinder unwinder;
        bool unwRet = unwinder.UnwindLocal(true);
        EXPECT_EQ(true, unwRet) << "UnwinderTest001: unwRet:" << unwRet;
        const auto& frames = unwinder.GetFrames();
        ASSERT_GT(frames.size(), 0);
        GTEST_LOG_(INFO) << "frames:\n" << unwinder.GetFramesStr(frames);
        _exit(0);
    }

    int status;
    int ret = wait(&status);
    GTEST_LOG_(INFO) << "Status:" << status << " Result:" << ret;
    GTEST_LOG_(INFO) << "UnwinderTest001: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
