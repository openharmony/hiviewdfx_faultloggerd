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
#include <ctime>
#include <securec.h>
#include <string>
#include <vector>
#include "dfx_util.h"
#include "procinfo.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class ProcinfoTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

/**
 * @tc.name: ProcinfoTest001
 * @tc.desc: test GetProcStatus
 * @tc.type: FUNC
 */
HWTEST_F(ProcinfoTest, ProcinfoTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcinfoTest001: start.";
    ProcInfo procInfo;
    ASSERT_TRUE(GetProcStatus(procInfo));
    ASSERT_EQ(getpid(), procInfo.pid);
    GTEST_LOG_(INFO) << "ProcinfoTest001: end.";
}

/**
 * @tc.name: ProcinfoTest002
 * @tc.desc: test GetTidsByPidWithFunc
 * @tc.type: FUNC
 */
HWTEST_F(ProcinfoTest, ProcinfoTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcinfoTest002: start.";
    std::vector<int> tids;
    ASSERT_TRUE(GetTidsByPidWithFunc(getpid(), tids, nullptr));
    GTEST_LOG_(INFO) << "ProcinfoTest002: end.";
}

/**
 * @tc.name: ProcinfoTest003
 * @tc.desc: test GetProcStatusByPid, GetTidsByPid, IsThreadInPid
 * @tc.type: FUNC
 */
HWTEST_F(ProcinfoTest, ProcinfoTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcinfoTest003: start.";
    struct ProcInfo procInfo;
    ASSERT_TRUE(GetProcStatusByPid(getpid(), procInfo));
    std::vector<int> tids;
    std::vector<int> nstids;
    ASSERT_TRUE(GetTidsByPid(getpid(), tids, nstids));
    for (size_t i = 0; i < nstids.size(); ++i) {
        ASSERT_TRUE(IsThreadInPid(getpid(), nstids[i]));
        if (procInfo.ns) {
            int nstid = tids[i];
            TidToNstid(getpid(), tids[i], nstid);
            ASSERT_EQ(nstid, nstids[i]);
        }
    }
    GTEST_LOG_(INFO) << "ProcinfoTest003: end.";
}

/**
 * @tc.name: ProcinfoTest004
 * @tc.desc: test TidToNstid
 * @tc.type: FUNC
 */
HWTEST_F(ProcinfoTest, ProcinfoTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcinfoTest004: start.";
    int nstid = -1;
    ASSERT_TRUE(TidToNstid(getpid(), gettid(), nstid));
    ASSERT_EQ(gettid(), nstid);
    GTEST_LOG_(INFO) << "ProcinfoTest004: end.";
}

/**
 * @tc.name: ProcinfoTest005
 * @tc.desc: test ReadProcessStatus, ReadProcessWchan, ReadThreadWchan, ReadProcessName, ReadThreadName
 * @tc.type: FUNC
 */
HWTEST_F(ProcinfoTest, ProcinfoTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcinfoTest005: start.";
    std::string result;
    ReadProcessStatus(result, getpid());
    GTEST_LOG_(INFO) << result;
    ASSERT_TRUE(result.find("Name:") != std::string::npos);
    ASSERT_TRUE(result.find("SigQ:") != std::string::npos);
    ASSERT_TRUE(result.find("nonvoluntary_ctxt_switches") != std::string::npos);
    ReadProcessWchan(result, getpid(), false, true);
    GTEST_LOG_(INFO) << result;
    ASSERT_TRUE(result.find("Process wchan:") != std::string::npos);
    ReadThreadWchan(result, gettid(), true);
    GTEST_LOG_(INFO) << result;
    ASSERT_TRUE(result.find("Tid:") != std::string::npos);
    ASSERT_TRUE(result.find("wchan:") != std::string::npos);
    ReadProcessName(getpid(), result);
    GTEST_LOG_(INFO) << result;
    ASSERT_TRUE(result.find("test_procinfo") != std::string::npos);
    ReadThreadName(getpid(), result);
    GTEST_LOG_(INFO) << result;
    ASSERT_TRUE(result.find("test_procinfo") != std::string::npos);
    ReadThreadNameByPidAndTid(getpid(), gettid(), result);
    GTEST_LOG_(INFO) << result;
    ASSERT_TRUE(result.find("test_procinfo") != std::string::npos);
    GTEST_LOG_(INFO) << "ProcinfoTest005: end.";
}
