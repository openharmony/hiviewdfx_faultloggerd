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
    bool ret = GetProcStatus(procInfo);
    ASSERT_EQ(ret, true);
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
    bool ret = GetTidsByPidWithFunc(getpid(), tids, nullptr);
    ASSERT_EQ(ret, true);
    GTEST_LOG_(INFO) << "ProcinfoTest002: end.";
}

/**
 * @tc.name: ProcinfoTest003
 * @tc.desc: test TidToNstid
 * @tc.type: FUNC
 */
HWTEST_F(ProcinfoTest, ProcinfoTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "ProcinfoTest003: start.";
    struct ProcInfo procInfo;
    bool ret = GetProcStatusByPid(getpid(), procInfo);
    ASSERT_EQ(ret, true);
    std::vector<int> tids;
    std::vector<int> nstids;
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
    GTEST_LOG_(INFO) << "ProcinfoTest003: end.";
}
