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

#include "elapsed_time.h"
#include "fp_unwinder.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_TAG "DfxFpUnwinderTest"
#define LOG_DOMAIN 0xD002D11

class FpUnwinderTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: FpUnwinderTest000
 * @tc.desc: test fp unwinder
 * @tc.type: FUNC
 */
HWTEST_F(FpUnwinderTest, FpUnwinderTest000, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FpUnwinderTest000: start.";
#ifdef __aarch64__
#endif
    GTEST_LOG_(INFO) << "FpUnwinderTest000: end.";
}

/**
 * @tc.name: FpUnwinderTest001
 * @tc.desc: test fp unwinder Unwind
 * @tc.type: FUNC
 */
HWTEST_F(FpUnwinderTest, FpUnwinderTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FpUnwinderTest001: start.";
#ifdef __aarch64__
#endif
    GTEST_LOG_(INFO) << "FpUnwinderTest001: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
