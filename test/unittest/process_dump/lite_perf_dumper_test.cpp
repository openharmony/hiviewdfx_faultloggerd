/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <cerrno>
#include <cstring>
#include <securec.h>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_dump_request.h"
#include "lite_perf_dumper.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class LitePerfDumperTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: LitePerfDumperPerf
 * @tc.desc: test LitePerfDumper perf with invalid fd
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfDumperTest, LitePerfDumper_Perf_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfDumper_Perf_001: start.";
    LitePerfParam lperf;
    (void)memset_s(&lperf, sizeof(lperf), 0, sizeof(lperf));
    int32_t ret = LitePerfDumper::GetInstance().ReadLperfAndCheck(lperf, -1);
    ASSERT_EQ(ret, -1);
    LitePerfDumper::GetInstance().Perf(-1);
    GTEST_LOG_(INFO) << "LitePerfDumper_Perf_001: end.";
}

/**
 * @tc.name: LitePerfDumperReadLperfAndCheck
 * @tc.desc: test LitePerfDumper reading and checking lite perf param with insufficient data
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfDumperTest, LitePerfDumper_ReadLperfAndCheck_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfDumper_ReadLperfAndCheck_002: start.";
    int fds[2] = {-1, -1};
    if (pipe(fds) == 0) {
        LitePerfParam lperf;
        char smallData[4] = {0};
        write(fds[1], smallData, sizeof(smallData));
        close(fds[1]);
        int32_t ret = LitePerfDumper::GetInstance().ReadLperfAndCheck(lperf, fds[0]);
        ASSERT_EQ(ret, -1);
        close(fds[0]);
    }
    GTEST_LOG_(INFO) << "LitePerfDumper_ReadLperfAndCheck_002: end.";
}

/**
 * @tc.name: LitePerfDumperWriteSampleData
 * @tc.desc: test LitePerfDumper writing sample data to fd
 * @tc.type: FUNC
 */
HWTEST_F(LitePerfDumperTest, LitePerfDumper_WriteSampleData_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LitePerfDumper_WriteSampleData_004: start.";
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe(fds), 0);
    std::string testData = "sample_data_content_for_test";
    LitePerfDumper::GetInstance().WriteSampleData(fds[1], testData);
    close(fds[1]);
    char readBuf[256] = {0};
    ssize_t nread = read(fds[0], readBuf, sizeof(readBuf) - 1);
    ASSERT_EQ(nread, static_cast<ssize_t>(testData.size()));
    ASSERT_EQ(std::string(readBuf, nread), testData);
    close(fds[0]);
    GTEST_LOG_(INFO) << "LitePerfDumper_WriteSampleData_004: end.";
}
}