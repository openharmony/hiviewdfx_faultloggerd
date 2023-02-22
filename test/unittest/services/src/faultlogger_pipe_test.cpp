/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

/* This files contains faultlog pipe module unittest. */

#include "faultlogger_pipe_test.h"

#include <string>
#include "fault_logger_pipe.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

void FaultLoggerPipeTest::SetUpTestCase(void)
{
}

void FaultLoggerPipeTest::TearDownTestCase(void)
{
}

void FaultLoggerPipeTest::SetUp(void)
{
}

void FaultLoggerPipeTest::TearDown(void)
{
}


/**
 * @tc.name: FaultLoggerPipeTest001
 * @tc.desc: test FaultLoggerPipeMap Set Get func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerPipeTest, FaultLoggerPipeTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerPipeTest001: start.";
    std::shared_ptr<FaultLoggerPipeMap> ptr = std::make_shared<FaultLoggerPipeMap>();
    int pid = 100;
    auto ret = ptr->Get(pid);
    if (ret != nullptr) {
        GTEST_LOG_(ERROR) << "FaultLoggerPipeTest001 Get failed";
        ASSERT_TRUE(false);
    }
    ptr->Set(pid);
    ret = ptr->Get(pid);
    EXPECT_EQ(true, ret != nullptr) << "FaultLoggerPipeTest001 Get failed";
    GTEST_LOG_(INFO) << "FaultLoggerPipeTest001: end.";
}

/**
 * @tc.name: FaultLoggerPipeTest002
 * @tc.desc: test FaultLoggerPipeMap Del func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerPipeTest, FaultLoggerPipeTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerPipeTest002: start.";
    std::shared_ptr<FaultLoggerPipeMap> ptr = std::make_shared<FaultLoggerPipeMap>();
    int pid = 100;
    ptr->Set(pid);
    auto ret = ptr->Get(pid);
    EXPECT_EQ(true, ret != nullptr) << "FaultLoggerPipeTest002 Get failed";
    ptr->Del(pid);
    ret = ptr->Get(pid);
    EXPECT_EQ(true, ret == nullptr) << "FaultLoggerPipeTest002 Del failed";
    GTEST_LOG_(INFO) << "FaultLoggerPipeTest002: end.";
}