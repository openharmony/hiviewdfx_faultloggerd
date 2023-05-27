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
#include "dfx_dump_res.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class CommonTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

/**
 * @tc.name: DfxUtilTest001
 * @tc.desc: test DfxUtil GetCurrentTimeStr
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest001: start.";
    time_t now = time(nullptr);
    std::string timeStr = GetCurrentTimeStr(static_cast<uint64_t>(now));
    GTEST_LOG_(INFO) << timeStr;
    ASSERT_NE(timeStr, "invalid timestamp\n");
    now += 100000; // 100000 : test time offset
    timeStr = GetCurrentTimeStr(static_cast<uint64_t>(now));
    GTEST_LOG_(INFO) << timeStr;
    ASSERT_NE(timeStr, "invalid timestamp\n");
    GTEST_LOG_(INFO) << "DfxUtilTest001: end.";
}

/**
 * @tc.name: DfxUtilTest002
 * @tc.desc: test DfxUtil GetRealTargetPid
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest002: start.";
    pid_t pid = getppid();
    ASSERT_EQ(pid, GetRealTargetPid());
    GTEST_LOG_(INFO) << "DfxUtilTest002: end.";
}

/**
 * @tc.name: DfxUtilTest003
 * @tc.desc: test DfxUtil GetTidsByPid
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxUtilTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxUtilTest003: start.";
    struct ProcInfo procInfo;
    int result = GetProcStatusByPid(getpid(), procInfo);
    ASSERT_EQ(result, 0);
    std::vector<int> tids;
    std::vector<int> nstids;
    bool ret = false;
    ret = GetTidsByPid(getpid(), tids, nstids);
    ASSERT_EQ(ret, true);
    for (size_t i = 0; i < nstids.size(); ++i) {
        ret = IsThreadInCurPid(nstids[i]);
        ASSERT_EQ(ret, true);

        if (procInfo.ns) {
            int nstid = tids[i];
            TidToNstid(getpid(), tids[i], nstid);
            ASSERT_EQ(nstid, nstids[i]);
        }
    }

    GTEST_LOG_(INFO) << "DfxUtilTest003: end.";
}

/**
 * @tc.name: DfxDumpResTest001
 * @tc.desc: test DfxDumpRes functions
 * @tc.type: FUNC
 */
HWTEST_F(CommonTest, DfxDumpResTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxDumpResTest001: start.";
    int32_t res = DUMP_ESUCCESS;
    GTEST_LOG_(INFO) << DfxDumpRes::ToString(res);
    GTEST_LOG_(INFO) << "DfxDumpResTest001: end.";
}