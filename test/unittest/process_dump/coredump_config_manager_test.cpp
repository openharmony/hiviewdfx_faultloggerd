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
#include <string>

#include "cJSON.h"
#include "coredump_config_manager.h"
#include "coredump_controller.h"
#include "coredump_common.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class CoredumpConfigManagerTest : public testing::Test {};
} // namespace HiviewDFX
} // namespace OHOS

namespace {
/**
 * @tc.name: CoredumpConfigManagerTest001
 * @tc.desc: test CoredumpConfigManager getting default configuration values
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpConfigManager_GetDefaultConfig_001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager_GetDefaultConfig_001: start.";
    auto& manager = CoredumpConfigManager::GetInstance();
    const auto& config = manager.GetConfig();
    ASSERT_EQ(config.coredumpSwitch, false);
    ASSERT_EQ(config.dumperThreadCount, 1);
    ASSERT_EQ(config.process.prpsinfo, true);
    ASSERT_EQ(config.process.multiThread, true);
    ASSERT_EQ(config.process.auxv, true);
    ASSERT_EQ(config.threads.prstatus, true);
    ASSERT_EQ(config.threads.fpregset, true);
    ASSERT_EQ(config.threads.siginfo, true);
    ASSERT_EQ(config.threads.dumpAll, true);
    GTEST_LOG_(INFO) << "CoredumpConfigManager_GetDefaultConfig_001: end.";
}

/**
 * @tc.name: CoredumpConfigManagerLoadProfileFromJson
 * @tc.desc: test CoredumpConfigManager loading profile from empty json string
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpConfigManager_LoadProfileFromJson_002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_002: start.";
    auto& manager = CoredumpConfigManager::GetInstance();
    bool ret = manager.LoadProfileFromJson("", "default");
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_002: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest003
 * @tc.desc: test CoredumpConfigManager loading json without coredump profiles key
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpConfigManager_LoadProfileFromJson_NoProfiles_003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_NoProfiles_003: start.";
    auto& manager = CoredumpConfigManager::GetInstance();
    std::string jsonText = "{\"otherKey\":123}";
    bool ret = manager.LoadProfileFromJson(jsonText, "default");
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_NoProfiles_003: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest004
 * @tc.desc: test CoredumpConfigManager loading invalid profile name
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpConfigManager_LoadProfileFromJson_InvalidProfile_004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_InvalidProfile_004: start.";
    auto& manager = CoredumpConfigManager::GetInstance();
    std::string jsonText = "{\"coredumpProfiles\":{\"otherProfile\":{}}}";
    bool ret = manager.LoadProfileFromJson(jsonText, "default");
    ASSERT_FALSE(ret);
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_InvalidProfile_004: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest005
 * @tc.desc: test CoredumpConfigManager loading valid json profile configuration
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpConfigManager_LoadProfileFromJson_Valid_005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_Valid_005: start.";
    auto& manager = CoredumpConfigManager::GetInstance();
    std::string jsonText = "{\"coredumpProfiles\":{\"default\":{\"coredumpSwitch\":true,"
        "\"dumperThreadCount\":4}}}";
    bool ret = manager.LoadProfileFromJson(jsonText, "default");
    ASSERT_TRUE(ret);
    const auto& config = manager.GetConfig();
    ASSERT_EQ(config.coredumpSwitch, true);
    ASSERT_EQ(config.dumperThreadCount, 4);
    GTEST_LOG_(INFO) << "CoredumpConfigManager_LoadProfileFromJson_Valid_005: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest006
 * @tc.desc: test CoredumpConfigManager filling config from null json object
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpConfigManager_FillConfigFromObject_Null_006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpConfigManager_FillConfigFromObject_Null_006: start.";
    auto& manager = CoredumpConfigManager::GetInstance();
    std::string jsonText = "{\"coredumpProfiles\":{\"default\":null}}";
    bool ret = manager.LoadProfileFromJson(jsonText, "default");
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "CoredumpConfigManager_FillConfigFromObject_Null_006: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest007
 * @tc.desc: test CoredumpJsonUtil safely getting integer value from json object
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpJsonUtil_GetIntSafe_007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpJsonUtil_GetIntSafe_007: start.";
    cJSON* obj = cJSON_Parse("{\"num\":42,\"str\":\"hello\"}");
    ASSERT_TRUE(CoredumpJsonUtil::GetIntSafe(obj, "num", 0) == 42);
    ASSERT_TRUE(CoredumpJsonUtil::GetIntSafe(obj, "str", 10) == 10);
    ASSERT_TRUE(CoredumpJsonUtil::GetIntSafe(obj, "missing", 5) == 5);
    ASSERT_TRUE(CoredumpJsonUtil::GetIntSafe(nullptr, "num", 5) == 5);
    cJSON_Delete(obj);
    GTEST_LOG_(INFO) << "CoredumpJsonUtil_GetIntSafe_007: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest008
 * @tc.desc: test CoredumpJsonUtil safely getting boolean value from json object
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpJsonUtil_GetBoolSafe_008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpJsonUtil_GetBoolSafe_008: start.";
    cJSON* obj = cJSON_Parse("{\"flag\":true,\"num\":42}");
    ASSERT_TRUE(CoredumpJsonUtil::GetBoolSafe(obj, "flag", false) == true);
    ASSERT_TRUE(CoredumpJsonUtil::GetBoolSafe(obj, "num", true) == true);
    ASSERT_TRUE(CoredumpJsonUtil::GetBoolSafe(obj, "missing", false) == false);
    ASSERT_TRUE(CoredumpJsonUtil::GetBoolSafe(nullptr, "flag", false) == false);
    cJSON_Delete(obj);
    GTEST_LOG_(INFO) << "CoredumpJsonUtil_GetBoolSafe_008: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest009
 * @tc.desc: test CoredumpJsonUtil safely getting string value from json object
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpJsonUtil_GetStrSafe_009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpJsonUtil_GetStrSafe_009: start.";
    cJSON* obj = cJSON_Parse("{\"name\":\"test\",\"num\":42}");
    ASSERT_TRUE(strcmp(CoredumpJsonUtil::GetStrSafe(obj, "name", "def"), "test") == 0);
    ASSERT_TRUE(strcmp(CoredumpJsonUtil::GetStrSafe(obj, "num", "def"), "def") == 0);
    ASSERT_TRUE(strcmp(CoredumpJsonUtil::GetStrSafe(obj, "missing", "def"), "def") == 0);
    ASSERT_TRUE(strcmp(CoredumpJsonUtil::GetStrSafe(nullptr, "name", "def"), "def") == 0);
    cJSON_Delete(obj);
    GTEST_LOG_(INFO) << "CoredumpJsonUtil_GetStrSafe_009: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest010
 * @tc.desc: test CoredumpProc initialization and validity check
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpProc_InitAndValid_010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpProc_InitAndValid_010: start.";
    CoredumpProc proc;
    ASSERT_FALSE(proc.IsValid());
    proc.Init(1, 2);
    ASSERT_TRUE(proc.IsValid());
    ASSERT_EQ(proc.targetPid, 1);
    ASSERT_EQ(proc.keyTid, 2);
    CoredumpProc procZero;
    procZero.Init(0, 1);
    ASSERT_FALSE(procZero.IsValid());
    CoredumpProc procNeg;
    procNeg.Init(1, -1);
    ASSERT_FALSE(procNeg.IsValid());
    GTEST_LOG_(INFO) << "CoredumpProc_InitAndValid_010: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest011
 * @tc.desc: test CoredumpController checking if signal is coredump signal
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpController_IsCoredumpSignal_011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpController_IsCoredumpSignal_011: start.";
    ProcessDumpRequest req1 = {};
    req1.siginfo.si_signo = SIGLEAK_STACK;
    req1.siginfo.si_code = SIGLEAK_STACK_COREDUMP;
    ASSERT_TRUE(CoredumpController::IsCoredumpSignal(req1));
    req1.siginfo.si_code = -SIGLEAK_STACK_COREDUMP;
    ASSERT_TRUE(CoredumpController::IsCoredumpSignal(req1));

    ProcessDumpRequest req2 = {};
    req2.siginfo.si_signo = SIGSEGV;
    ASSERT_FALSE(CoredumpController::IsCoredumpSignal(req2));

    ProcessDumpRequest req3 = {};
    req3.siginfo.si_signo = SIGLEAK_STACK;
    req3.siginfo.si_code = 0;
    ASSERT_FALSE(CoredumpController::IsCoredumpSignal(req3));
    GTEST_LOG_(INFO) << "CoredumpController_IsCoredumpSignal_011: end.";
}

/**
 * @tc.name: CoredumpConfigManagerTest012
 * @tc.desc: test CoredumpController verifying coredump trust list for signal permission
 * @tc.type: FUNC
 */
HWTEST_F(CoredumpConfigManagerTest, CoredumpController_VerifyTrustList_012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CoredumpController_VerifyTrustList_012: start.";
    ProcessDumpRequest reqMdm = {};
    reqMdm.siginfo.si_signo = SIGSEGV;
    ASSERT_FALSE(CoredumpController::IsCoredumpAllowed(reqMdm));

    ProcessDumpRequest reqCoredump = {};
    reqCoredump.siginfo.si_signo = SIGLEAK_STACK;
    reqCoredump.siginfo.si_code = SIGLEAK_STACK_COREDUMP;
    ASSERT_TRUE(CoredumpController::IsCoredumpAllowed(reqCoredump));

    reqCoredump.siginfo.si_code = -SIGLEAK_STACK_COREDUMP;
    ASSERT_TRUE(CoredumpController::IsCoredumpAllowed(reqCoredump));
    GTEST_LOG_(INFO) << "CoredumpController_VerifyTrustList_012: end.";
}
}