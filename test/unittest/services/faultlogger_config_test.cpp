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

/* This files contains unit test for config module. */

#include <gtest/gtest.h>
#include "fault_logger_config.h"

#include <memory>
#include <string>

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerConfigTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};
} // namespace HiviewDFX
} // namespace OHOS

/** FaultLoggerConfigTest001
 * @tc.name: DfxMapsRequestTest033
 * @tc.desc: test get file max number
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerConfigTest, FaultLoggerConfigTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest001: start.";
    std::shared_ptr<FaultLoggerConfig> config = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
        LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    int input = 100;
    bool ret = config->SetLogFileMaxNumber(input);
    if (ret) {
        int output = config->GetLogFileMaxNumber();
        EXPECT_EQ(true, input == output);
    }
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest001: end.";
}

/**
 * @tc.name: FaultLoggerConfigTest002
 * @tc.desc: test get file max size
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerConfigTest, FaultLoggerConfigTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest002: start.";
    std::shared_ptr<FaultLoggerConfig> config = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
        LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    long input = 100;
    bool ret = config->SetLogFileMaxSize(input);
    if (ret) {
        long output = config->GetLogFileMaxSize();
        EXPECT_EQ(true, input == output);
    }
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest002: end.";
}

/**
 * @tc.name: FaultLoggerConfigTest003
 * @tc.desc: test get file path
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerConfigTest, FaultLoggerConfigTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest003: start.";
    std::shared_ptr<FaultLoggerConfig> config = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
        LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    std::string input = "/data/log.txt";
    bool ret = config->SetLogFilePath(input);
    if (ret) {
        std::string output = config->GetLogFilePath();
        EXPECT_EQ(true, input == output);
    }
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest003: end.";
}

/**
 * @tc.name: FaultLoggerConfigTest004
 * @tc.desc: test get debug file path
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerConfigTest, FaultLoggerConfigTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest004: start.";
    std::shared_ptr<FaultLoggerConfig> config = std::make_shared<FaultLoggerConfig>(LOG_FILE_NUMBER, LOG_FILE_SIZE,
        LOG_FILE_PATH, DEBUG_LOG_FILE_PATH);
    std::string input = "/data/debug/log.txt";
    bool ret = config->SetDebugLogFilePath(input);
    if (ret) {
        std::string output = config->GetDebugLogFilePath();
        EXPECT_EQ(true, input == output);
    }
    GTEST_LOG_(INFO) << "FaultLoggerConfigTest004: end.";
}
