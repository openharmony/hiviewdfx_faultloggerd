/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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
#include <future>
#include <malloc.h>
#include <map>
#include <securec.h>
#include <thread>
#include <unistd.h>

#include "dfx_ptrace.h"
#include "dfx_regs_get.h"
#include "elapsed_time.h"
#include "unwinder.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class AccessorsTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: IsValidFrameTest001
 * @tc.desc: test DfxAccessorsLocal IsValidFrame interface
 * @tc.type: FUNC
 */
HWTEST_F(AccessorsTest, IsValidFrameTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "IsValidFrameTest001: start.";
    auto accessors = std::make_shared<DfxAccessorsLocal>();
    uintptr_t stackBottom = 999;
    uintptr_t addr = 10;
    uintptr_t stackTop = static_cast<uintptr_t>(1);
    ASSERT_FALSE(accessors->IsValidFrame(addr, stackBottom, stackTop));
    stackBottom = 1;
    stackTop = static_cast<uintptr_t>(-1);
    ASSERT_TRUE(accessors->IsValidFrame(addr, stackBottom, stackTop));
    GTEST_LOG_(INFO) << "IsValidFrameTest001: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS