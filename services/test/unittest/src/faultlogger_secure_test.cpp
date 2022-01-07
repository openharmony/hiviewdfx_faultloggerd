/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

/* This files contains faultlog secure module unittest. */

#include "faultlogger_secure_test.h"

#include <string>
#include "fault_logger_secure.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

void FaultLoggerSecureTest::SetUpTestCase(void)
{
}

void FaultLoggerSecureTest::TearDownTestCase(void)
{
}

void FaultLoggerSecureTest::SetUp(void)
{
}

void FaultLoggerSecureTest::TearDown(void)
{
}


/**
 * @tc.name: FaultLoggerSecureTest001
 * @tc.desc: test check caller uid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerSecureTest, FaultLoggerSecureTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerSecureTest001: start.";
    std::shared_ptr<FaultLoggerSecure> secure = std::make_shared<FaultLoggerSecure>();
    int callingUid = 1000;
    int pid = 100;
    bool ret = secure->CheckCallerUID(callingUid, pid);
    EXPECT_EQ(true, ret == true);
    GTEST_LOG_(INFO) << "FaultLoggerSecureTest001: end.";
}
