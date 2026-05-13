/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include <gtest/hwext/gtest-multithread.h>

#include <string>
#include <thread>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_test_util.h"
#include "dfx_dump_catcher_slow_policy.h"

using namespace testing;
using namespace testing::ext;
using namespace testing::mt;

namespace OHOS {
namespace HiviewDFX {
class DfxDumpCatcherSlowPolicyTest : public testing::Test {};

/**
 * @tc.name: DfxDumpCatcherSlowPolicyTest001
 * @tc.desc: test DfxDumpCatcherSlowPolicy Set And Get Same Pid
 * @tc.type: FUNC
 */
HWTEST_F(DfxDumpCatcherSlowPolicyTest, DfxDumpCatcherSlowPolicyTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpCatcherSlowPolicyTest001: start.";
    const int pid = 12345;
    const int waitSec = 5; // 5 : 5s for slow period
    DfxDumpCatcherSlowPolicy::GetInstance().SetDumpCatcherSlowStat(pid);
    sleep(1); // 1 : 1s
    ASSERT_TRUE(DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(pid));
    sleep(waitSec);
    ASSERT_FALSE(DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(pid));
    GTEST_LOG_(INFO) << "DfxDumpCatcherSlowPolicyTest001: end.";
}

/**
 * @tc.name: DfxDumpCatcherSlowPolicyTest002
 * @tc.desc: test DfxDumpCatcherSlowPolicy Set And Get diff Pid
 * @tc.type: FUNC
 */
HWTEST_F(DfxDumpCatcherSlowPolicyTest, DfxDumpCatcherSlowPolicyTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpCatcherSlowPolicyTest002: start.";
    const int pid = 12345;
    const int otherPid = 54321;
    const int waitSec = 5; // 5 : 5s for slow period
    DfxDumpCatcherSlowPolicy::GetInstance().SetDumpCatcherSlowStat(pid);
    sleep(1); // 1 : 1s
    ASSERT_FALSE(DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(otherPid));
    sleep(waitSec);
    ASSERT_FALSE(DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(pid));
    GTEST_LOG_(INFO) << "DfxDumpCatcherSlowPolicyTest002: end.";
}

/**
 * @tc.name: DfxDumpCatcherSlowPolicyTest003
 * @tc.desc: test DfxDumpCatcherSlowPolicy Set And Get muti Pid
 * @tc.type: FUNC
 */
HWTEST_F(DfxDumpCatcherSlowPolicyTest, DfxDumpCatcherSlowPolicyTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpCatcherSlowPolicyTest003: start.";
    const int waitSec = 5; // 5 : 5s for slow period
    const int pidNum = 50; // 50 : save 50 record

    for (int pid = 0; pid < pidNum; pid++) {
        DfxDumpCatcherSlowPolicy::GetInstance().SetDumpCatcherSlowStat(pid);
        usleep(100000); // 100000 : 100ms
        ASSERT_TRUE(DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(pid));
    }

    sleep(waitSec);
    for (int pid = 0; pid < pidNum; pid++) {
        ASSERT_FALSE(DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(pid));
    }

    for (int pid = 0; pid < pidNum + 1; pid++) {
        DfxDumpCatcherSlowPolicy::GetInstance().SetDumpCatcherSlowStat(pid);
        usleep(1000); // 1000 : 1ms
    }
    ASSERT_FALSE(DfxDumpCatcherSlowPolicy::GetInstance().IsDumpCatcherInSlowPeriod(1));
    GTEST_LOG_(INFO) << "DfxDumpCatcherSlowPolicyTest003: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
