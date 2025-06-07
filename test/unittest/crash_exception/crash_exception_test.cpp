/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include <ctime>
#include <gtest/gtest.h>
#include <securec.h>
#include <string>
#include <vector>

#include "crash_exception.h"
#include "crash_exception_listener.h"
#include "dfx_errors.h"
#include "dfx_util.h"
#include "hisysevent_manager.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class CrashExceptionTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
    void SetUp() {}
    void TearDown() {}
};

static constexpr int32_t TEST_PROCESS_ID = 1234;
static constexpr int32_t TEST_UID = 5678;

/**
 * @tc.name: CrashExceptionTest001
 * @tc.desc: test ReportCrashException
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest001: start.";
    char testProcessName[] = "process_name_ptr";
    std::shared_ptr<CrashExceptionListener> crashListener = std::make_shared<CrashExceptionListener>();
    ListenerRule tagRule("RELIABILITY", "CPP_CRASH_EXCEPTION", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(crashListener, sysRules);
    crashListener->SetKeyWord(testProcessName);
    ReportCrashException(testProcessName, TEST_PROCESS_ID, TEST_UID, CrashExceptionCode::CRASH_UNKNOWN);
    ASSERT_TRUE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest001: end.";
}

/**
 * @tc.name: CrashExceptionTest002
 * @tc.desc: test ReportCrashException, error code is success.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest002: start.";
    char testProcessName[] = "process_name_ptr";
    std::shared_ptr<CrashExceptionListener> crashListener = std::make_shared<CrashExceptionListener>();
    ListenerRule tagRule("RELIABILITY", "CPP_CRASH_EXCEPTION", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(crashListener, sysRules);
    crashListener->SetKeyWord(testProcessName);
    ReportCrashException(testProcessName, TEST_PROCESS_ID, TEST_UID, CrashExceptionCode::CRASH_ESUCCESS);
    ASSERT_FALSE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest002: end.";
}

/**
 * @tc.name: CrashExceptionTest003
 * @tc.desc: test ReportCrashException, process name length is more than 128 bytes.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest003: start.";
    char testProcessName[] = "process_name_ptr_1111111111111111111111111111111111111"
    "11111111111111111111111111111111111111111111111111111111111111111111111111111111"
    "11111111111111111111111111111111111111111111111111111111111111111111111111111111";
    std::shared_ptr<CrashExceptionListener> crashListener = std::make_shared<CrashExceptionListener>();
    ListenerRule tagRule("RELIABILITY", "CPP_CRASH_EXCEPTION", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(crashListener, sysRules);
    crashListener->SetKeyWord(testProcessName);
    ReportCrashException(testProcessName, TEST_PROCESS_ID, TEST_UID, CrashExceptionCode::CRASH_UNKNOWN);
    ASSERT_TRUE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest003: end.";
}

/**
 * @tc.name: CrashExceptionTest004
 * @tc.desc: test ReportCrashException
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest004: start.";
    std::string keyWord = "process_name_string";
    std::shared_ptr<CrashExceptionListener> crashListener = std::make_shared<CrashExceptionListener>();
    ListenerRule tagRule("RELIABILITY", "CPP_CRASH_EXCEPTION", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(crashListener, sysRules);
    crashListener->SetKeyWord(keyWord);
    ReportCrashException(keyWord, TEST_PROCESS_ID, TEST_UID, CrashExceptionCode::CRASH_UNKNOWN);
    ASSERT_TRUE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest004: end.";
}

/**
 * @tc.name: CrashExceptionTest005
 * @tc.desc: test ReportCrashException, error code is success.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest005: start.";
    std::string keyWord = "process_name_string";
    std::shared_ptr<CrashExceptionListener> crashListener = std::make_shared<CrashExceptionListener>();
    ListenerRule tagRule("RELIABILITY", "CPP_CRASH_EXCEPTION", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(crashListener, sysRules);
    crashListener->SetKeyWord(keyWord);
    ReportCrashException(keyWord, TEST_PROCESS_ID, TEST_UID, CrashExceptionCode::CRASH_ESUCCESS);
    ASSERT_FALSE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest005: end.";
}

/**
 * @tc.name: CrashExceptionTest006
 * @tc.desc: test ReportUnwinderException
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest006: start.";
    std::string keyWord = "process_name_unwind";
    std::shared_ptr<CrashExceptionListener> crashListener = std::make_shared<CrashExceptionListener>();
    ListenerRule tagRule("RELIABILITY", "CPP_CRASH_EXCEPTION", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(crashListener, sysRules);
    crashListener->SetKeyWord(keyWord);
    SetCrashProcInfo(ProcessDumpType::DUMP_TYPE_CPP_CRASH, keyWord, TEST_PROCESS_ID, TEST_UID);
    ReportUnwinderException(UnwindErrorCode::UNW_ERROR_STEP_ARK_FRAME);
    ASSERT_TRUE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest006: end.";
}

/**
 * @tc.name: CrashExceptionTest007
 * @tc.desc: test ReportUnwinderException, invalid unwind error code.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest007: start.";
    std::string keyWord = "process_name_unwind";
    std::shared_ptr<CrashExceptionListener> crashListener = std::make_shared<CrashExceptionListener>();
    ListenerRule tagRule("RELIABILITY", "CPP_CRASH_EXCEPTION", RuleType::WHOLE_WORD);
    std::vector<ListenerRule> sysRules;
    sysRules.push_back(tagRule);
    HiSysEventManager::AddListener(crashListener, sysRules);
    crashListener->SetKeyWord(keyWord);
    SetCrashProcInfo(ProcessDumpType::DUMP_TYPE_CPP_CRASH, keyWord, TEST_PROCESS_ID, TEST_UID);
    ReportUnwinderException(0);
    ASSERT_FALSE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest007: end.";
}

/**
 * @tc.name: CrashExceptionTest08
 * @tc.desc: test CheckFaultSummaryValid, valid Fault Summary.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest08, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest08: start.";
    std::string summary = std::string("Thread name:sensors\n") +
        "#00 pc 000c5738 /system/lib/ld-musl-arm.so.1(ioctl+72)(b985d2b9b22c3e542388f5803bac6a56)\n" +
        "#01 pc 00007fcf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder(\n" +
        "#02 pc 00034f35 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver(\n" +
        "#03 pc 000350a5 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "#04 pc 000363df /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::JoinThread(\n" +
        "#05 pc 00010955 /system/lib/platformsdk/libsystem_ability_fwk.z.so(\n" +
        "#06 pc 0000391b /system/bin/sa_main(main.cfi+1986)(c626ef160394bf644c17e6769318dc7d)\n" +
        "#07 pc 00073560 /system/lib/ld-musl-arm.so.1(libc_start_main_stage2+56)(\n" +
        "#08 pc 00003078 /system/bin/sa_main(_start_c+84)(c626ef160394bf644c17e6769318dc7d)\n" +
        "#09 pc 0000301c /system/bin/sa_main(c626ef160394bf644c17e6769318dc7d)\n";
    ASSERT_TRUE(CheckFaultSummaryValid(summary));
    GTEST_LOG_(INFO) << "CrashExceptionTest08: end.";
}

/**
 * @tc.name: CrashExceptionTest09
 * @tc.desc: test CheckFaultSummaryValid, valid Fault Summary.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest09, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest09: start.";
    std::string summary = std::string("Thread name:sensors\n") +
        "#00 pc 000c5738 /system/lib/ld-musl-arm.so.1(ioctl+72)(b985d2b9b22c3e542388f5803bac6a56)\n" +
        "#01 pc 00007fcf Not mapped\n" +
        "#02 pc 00034f35 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver(\n" +
        "#03 pc 000350a5 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "#04 pc 000363df /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::JoinThread(\n" +
        "#05 pc 00010955 /system/lib/platformsdk/libsystem_ability_fwk.z.so(\n" +
        "#06 pc 0000391b /system/bin/sa_main(main.cfi+1986)(c626ef160394bf644c17e6769318dc7d)\n" +
        "#07 pc 00073560 /system/lib/ld-musl-arm.so.1(libc_start_main_stage2+56)(\n" +
        "#08 pc 00003078 /system/bin/sa_main(_start_c+84)(c626ef160394bf644c17e6769318dc7d)\n" +
        "#09 pc 0000301c /system/bin/sa_main(c626ef160394bf644c17e6769318dc7d)\n";
    ASSERT_TRUE(CheckFaultSummaryValid(summary));
    GTEST_LOG_(INFO) << "CrashExceptionTest09: end.";
}

/**
 * @tc.name: CrashExceptionTest010
 * @tc.desc: test CheckFaultSummaryValid, invalid Fault Summary.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest010: start.";
    std::string summary = std::string("Thread name:sensors\n") +
        "#00 pc 000c5738 /system/lib/ld-musl-arm.so.1(ioctl+72)(b985d2b9b22c3e542388f5803bac6a56)\n" +
        "#01 pc 00007fcf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder(\n";
    ASSERT_FALSE(CheckFaultSummaryValid(summary));
    GTEST_LOG_(INFO) << "CrashExceptionTest010: end.";
}

/**
 * @tc.name: CrashExceptionTest011
 * @tc.desc: test SetCrashProcInfo functions
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest011: start.";
    std::string name = "";
    const int32_t pid = -1;
    SetCrashProcInfo(ProcessDumpType::DUMP_TYPE_CPP_CRASH, name, pid, 0);
    ReportUnwinderException(0);
    ASSERT_EQ(name, "");
    GTEST_LOG_(INFO) << "CrashExceptionTest011: end.";
}

/**
 * @tc.name: CrashExceptionTest12
 * @tc.desc: test CheckFaultSummaryValid, valid Fault Summary.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest12, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest12: start.";
    std::string summary = std::string("Thread name:sensors\n") +
        "#00 pc 000c5738 /system/lib/ld-musl-arm.so.1(ioctl+72)(b985d2b9b22c3e542388f5803bac6a56)\n" +
        "#01 pc 00007fcf Not mapped\n" +
        "#02 at /system/etc/abc/framework/stateMgmt.abc\n";
    ASSERT_TRUE(CheckFaultSummaryValid(summary));
    GTEST_LOG_(INFO) << "CrashExceptionTest12: end.";
}
} // namespace HiviewDFX
} // namespace OHOS