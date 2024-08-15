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

#include <gtest/gtest.h>

#include <string>
#include "dfx_util.h"
#include "fault_logger_pipe.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerPipeTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: FaultLoggerPipeTest001
 * @tc.desc: test FaultLoggerPipeMap Check Set Get func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerPipeTest, FaultLoggerPipeTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerPipeTest001: start.";
    std::shared_ptr<FaultLoggerPipeMap> ptr = std::make_shared<FaultLoggerPipeMap>();
    uint64_t time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    int pid = 100;
    bool check = ptr->Check(pid, time);
    EXPECT_EQ(check, false) << "FaultLoggerPipeTest001 Check failed";

    ptr->Set(pid, time);
    auto ret = ptr->Get(pid);
    EXPECT_EQ(true, ret != nullptr) << "FaultLoggerPipeTest001 Get failed";

    sleep(11); // sleep 11 seconds
    time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    check = ptr->Check(pid, time);
    EXPECT_EQ(check, false) << "FaultLoggerPipeTest001 Check failed";

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
    uint64_t time = OHOS::HiviewDFX::GetTimeMilliSeconds();
    int pid = 100;
    ptr->Set(pid, time);
    auto ret = ptr->Get(pid);
    EXPECT_EQ(true, ret != nullptr) << "FaultLoggerPipeTest002 Get failed";
    ptr->Del(pid);
    ret = ptr->Get(pid);
    EXPECT_EQ(true, ret == nullptr) << "FaultLoggerPipeTest002 Del failed";
    GTEST_LOG_(INFO) << "FaultLoggerPipeTest002: end.";
}

/**
 * @tc.name: FaultLoggerPipeTest003
 * @tc.desc: test FaultLoggerPipeMap Del func
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerPipeTest, FaultLoggerPipeTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerPipeTest003: start.";
    FaultLoggerPipe faultLoggerPipe;
    faultLoggerPipe.init_ = false;
    bool ret = faultLoggerPipe.SetSize(1);
    EXPECT_FALSE(ret);
    ret = faultLoggerPipe.Init();
    EXPECT_TRUE(ret);
    ret = faultLoggerPipe.SetSize(1);
    EXPECT_TRUE(ret);
    int result = faultLoggerPipe.GetWriteFd();
    EXPECT_NE(-1, result);

    FaultLoggerPipe2 faultLoggerPipe2(time(nullptr), true);

    FaultLoggerPipeMap faultLoggerPipeMap;
    ret = faultLoggerPipeMap.Check(1, time(nullptr));
    EXPECT_FALSE(ret);

    GTEST_LOG_(INFO) << "FaultLoggerPipeTest003: end.";
}
}