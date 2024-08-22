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
HWTEST_F(CrashExceptionTest, CrashExceptionTest001, TestSize.Level2)
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
    ASSERT_FALSE(crashListener->CheckKeywordInReasons());
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
    SetCrashProcInfo(keyWord, TEST_PROCESS_ID, TEST_UID);
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
    SetCrashProcInfo(keyWord, TEST_PROCESS_ID, TEST_UID);
    ReportUnwinderException(0);
    ASSERT_FALSE(crashListener->CheckKeywordInReasons());
    HiSysEventManager::RemoveListener(crashListener);
    GTEST_LOG_(INFO) << "CrashExceptionTest007: end.";
}

/**
 * @tc.name: CrashExceptionTest008
 * @tc.desc: test CheckCrashLogValid, valid fault log.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest008: start.";
    std::string file = std::string("Module name:sensors\n") +
        "Fault thread info:\n" +
        "Tid:667, Name:sensors\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::Write\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDr\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "Registers:\n" +
        "r0:00000006 r1:c0306201 r2:ffe08e08 r3:00000009\n" +
        "r4:00000100 r5:f755a010 r6:00000000 r7:00000036\n" +
        "r8:00000006 r9:c0306201 r10:0000000c\n" +
        "fp:ffe08db8 ip:f6cdf360 sp:ffe08c90 lr:f6ccefe3 pc:f7e5cf0c\n" +
        "Other thread info:\n" +
        "Tid:875, Name:OS_IPC_0_875\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop()+34)\n" +
        "Memory near registers:\n" +
        "r2([stack]):\n" +
        "    ffe08e00 80407202\n" +
        "    ffe08e04 f755a038\n" +
        "FaultStack:\n" +
        "    ffe08c50 00000040\n" +
        "Maps:\n" +
        "4d0000-4d3000 r--p 00000000 /system/bin/sa_main\n" +
        "4d3000-4d6000 r-xp 00003000 /system/bin/sa_main\n" +
        "4d6000-4d7000 r--p 00006000 /system/bin/sa_main\n" +
        "HiLog:\n" +
        "08-05 18:30:41.876   667   667 E C03f00/MUSL-SIGCHAIN: signal_chain_handler call\n";
    int32_t ret = CheckCrashLogValid(file);
    ASSERT_TRUE(ret == CrashExceptionCode::CRASH_ESUCCESS);
    GTEST_LOG_(INFO) << "CrashExceptionTest008: end.";
}

/**
 * @tc.name: CrashExceptionTest009
 * @tc.desc: test CheckCrashLogValid, invalid Fault thread info.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest009: start.";
    std::string file = std::string("Module name:sensors\n") +
        "Fault thread info:\n" +
        "Tid:667, Name:sensors\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::Write\n" +
        "Registers:\n" +
        "r0:00000006 r1:c0306201 r2:ffe08e08 r3:00000009\n" +
        "r4:00000100 r5:f755a010 r6:00000000 r7:00000036\n" +
        "r8:00000006 r9:c0306201 r10:0000000c\n" +
        "fp:ffe08db8 ip:f6cdf360 sp:ffe08c90 lr:f6ccefe3 pc:f7e5cf0c\n" +
        "Other thread info:\n" +
        "Tid:875, Name:OS_IPC_0_875\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop()+34)\n" +
        "Memory near registers:\n" +
        "r2([stack]):\n" +
        "    ffe08e00 80407202\n" +
        "    ffe08e04 f755a038\n" +
        "FaultStack:\n" +
        "    ffe08c50 00000040\n" +
        "Maps:\n" +
        "4d0000-4d3000 r--p 00000000 /system/bin/sa_main\n" +
        "4d3000-4d6000 r-xp 00003000 /system/bin/sa_main\n" +
        "4d6000-4d7000 r--p 00006000 /system/bin/sa_main\n" +
        "HiLog:\n" +
        "08-05 18:30:41.876   667   667 E C03f00/MUSL-SIGCHAIN: signal_chain_handler call\n";
    int32_t ret = CheckCrashLogValid(file);
    ASSERT_FALSE(ret == CrashExceptionCode::CRASH_ESUCCESS);
    GTEST_LOG_(INFO) << "CrashExceptionTest009: end.";
}

/**
 * @tc.name: CrashExceptionTest010
 * @tc.desc: test CheckCrashLogValid, invalid Registers.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest010: start.";
    std::string file = std::string("Module name:sensors\n") +
        "Fault thread info:\n" +
        "Tid:667, Name:sensors\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::Write\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDr\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "Other thread info:\n" +
        "Tid:875, Name:OS_IPC_0_875\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop()+34)\n" +
        "Memory near registers:\n" +
        "r2([stack]):\n" +
        "    ffe08e00 80407202\n" +
        "    ffe08e04 f755a038\n" +
        "FaultStack:\n" +
        "    ffe08c50 00000040\n" +
        "Maps:\n" +
        "4d0000-4d3000 r--p 00000000 /system/bin/sa_main\n" +
        "4d3000-4d6000 r-xp 00003000 /system/bin/sa_main\n" +
        "4d6000-4d7000 r--p 00006000 /system/bin/sa_main\n" +
        "HiLog:\n" +
        "08-05 18:30:41.876   667   667 E C03f00/MUSL-SIGCHAIN: signal_chain_handler call\n";
    int32_t ret = CheckCrashLogValid(file);
    ASSERT_FALSE(ret == CrashExceptionCode::CRASH_ESUCCESS);
    GTEST_LOG_(INFO) << "CrashExceptionTest010: end.";
}

/**
 * @tc.name: CrashExceptionTest011
 * @tc.desc: test CheckCrashLogValid, invalid Other thread info.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest011: start.";
    std::string file = std::string("Module name:sensors\n") +
        "Fault thread info:\n" +
        "Tid:667, Name:sensors\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::Write\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDr\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "Registers:\n" +
        "r0:00000006 r1:c0306201 r2:ffe08e08 r3:00000009\n" +
        "r4:00000100 r5:f755a010 r6:00000000 r7:00000036\n" +
        "r8:00000006 r9:c0306201 r10:0000000c\n" +
        "fp:ffe08db8 ip:f6cdf360 sp:ffe08c90 lr:f6ccefe3 pc:f7e5cf0c\n" +
        "Other thread info:\n" +
        "Tid:875, Name:OS_IPC_0_875\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder\n" +
        "Memory near registers:\n" +
        "r2([stack]):\n" +
        "    ffe08e00 80407202\n" +
        "    ffe08e04 f755a038\n" +
        "FaultStack:\n" +
        "    ffe08c50 00000040\n" +
        "Maps:\n" +
        "4d0000-4d3000 r--p 00000000 /system/bin/sa_main\n" +
        "4d3000-4d6000 r-xp 00003000 /system/bin/sa_main\n" +
        "4d6000-4d7000 r--p 00006000 /system/bin/sa_main\n" +
        "HiLog:\n" +
        "08-05 18:30:41.876   667   667 E C03f00/MUSL-SIGCHAIN: signal_chain_handler call\n";
    int32_t ret = CheckCrashLogValid(file);
    ASSERT_FALSE(ret == CrashExceptionCode::CRASH_ESUCCESS);
    GTEST_LOG_(INFO) << "CrashExceptionTest011: end.";
}

/**
 * @tc.name: CrashExceptionTest012
 * @tc.desc: test CheckCrashLogValid, invalid Memory near registers.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest012: start.";
    std::string file = std::string("Module name:sensors\n") +
        "Fault thread info:\n" +
        "Tid:667, Name:sensors\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::Write\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDr\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "Registers:\n" +
        "r0:00000006 r1:c0306201 r2:ffe08e08 r3:00000009\n" +
        "r4:00000100 r5:f755a010 r6:00000000 r7:00000036\n" +
        "r8:00000006 r9:c0306201 r10:0000000c\n" +
        "fp:ffe08db8 ip:f6cdf360 sp:ffe08c90 lr:f6ccefe3 pc:f7e5cf0c\n" +
        "Other thread info:\n" +
        "Tid:875, Name:OS_IPC_0_875\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop()+34)\n" +
        "FaultStack:\n" +
        "    ffe08c50 00000040\n" +
        "Maps:\n" +
        "4d0000-4d3000 r--p 00000000 /system/bin/sa_main\n" +
        "4d3000-4d6000 r-xp 00003000 /system/bin/sa_main\n" +
        "4d6000-4d7000 r--p 00006000 /system/bin/sa_main\n" +
        "HiLog:\n" +
        "08-05 18:30:41.876   667   667 E C03f00/MUSL-SIGCHAIN: signal_chain_handler call\n";
    int32_t ret = CheckCrashLogValid(file);
    ASSERT_FALSE(ret == CrashExceptionCode::CRASH_ESUCCESS);
    GTEST_LOG_(INFO) << "CrashExceptionTest012: end.";
}

/**
 * @tc.name: CrashExceptionTest013
 * @tc.desc: test CheckCrashLogValid, invalid FaultStack.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest013: start.";
    std::string file = std::string("Module name:sensors\n") +
        "Fault thread info:\n" +
        "Tid:667, Name:sensors\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::Write\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDr\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "Registers:\n" +
        "r0:00000006 r1:c0306201 r2:ffe08e08 r3:00000009\n" +
        "r4:00000100 r5:f755a010 r6:00000000 r7:00000036\n" +
        "r8:00000006 r9:c0306201 r10:0000000c\n" +
        "fp:ffe08db8 ip:f6cdf360 sp:ffe08c90 lr:f6ccefe3 pc:f7e5cf0c\n" +
        "Other thread info:\n" +
        "Tid:875, Name:OS_IPC_0_875\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop()+34)\n" +
        "Memory near registers:\n" +
        "r2([stack]):\n" +
        "    ffe08e00 80407202\n" +
        "    ffe08e04 f755a038\n" +
        "Maps:\n" +
        "4d0000-4d3000 r--p 00000000 /system/bin/sa_main\n" +
        "4d3000-4d6000 r-xp 00003000 /system/bin/sa_main\n" +
        "4d6000-4d7000 r--p 00006000 /system/bin/sa_main\n" +
        "HiLog:\n" +
        "08-05 18:30:41.876   667   667 E C03f00/MUSL-SIGCHAIN: signal_chain_handler call\n";
    int32_t ret = CheckCrashLogValid(file);
    ASSERT_FALSE(ret == CrashExceptionCode::CRASH_ESUCCESS);
    GTEST_LOG_(INFO) << "CrashExceptionTest013: end.";
}

/**
 * @tc.name: CrashExceptionTest014
 * @tc.desc: test CheckCrashLogValid, invalid Maps.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest014: start.";
    std::string file = std::string("Module name:sensors\n") +
        "Fault thread info:\n" +
        "Tid:667, Name:sensors\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::Write\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDr\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop(\n" +
        "Registers:\n" +
        "r0:00000006 r1:c0306201 r2:ffe08e08 r3:00000009\n" +
        "r4:00000100 r5:f755a010 r6:00000000 r7:00000036\n" +
        "r8:00000006 r9:c0306201 r10:0000000c\n" +
        "fp:ffe08db8 ip:f6cdf360 sp:ffe08c90 lr:f6ccefe3 pc:f7e5cf0c\n" +
        "Other thread info:\n" +
        "Tid:875, Name:OS_IPC_0_875\n" +
        "#00 pc 000c1f0c /system/lib/ld-musl-arm.so.1(ioctl+72)(1b29d22c50bcb2ceee39f8a2bbb936dc)\n" +
        "#01 pc 0000efdf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder\n" +
        "#02 pc 00032a49 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::TransactWithDriver\n" +
        "#03 pc 00032b43 /system/lib/platformsdk/libipc_core.z.so(OHOS::BinderInvoker::StartWorkLoop()+34)\n" +
        "Memory near registers:\n" +
        "r2([stack]):\n" +
        "    ffe08e00 80407202\n" +
        "    ffe08e04 f755a038\n" +
        "FaultStack:\n" +
        "    ffe08c50 00000040\n" +
        "HiLog:\n" +
        "08-05 18:30:41.876   667   667 E C03f00/MUSL-SIGCHAIN: signal_chain_handler call\n";
    int32_t ret = CheckCrashLogValid(file);
    ASSERT_FALSE(ret == CrashExceptionCode::CRASH_ESUCCESS);
    GTEST_LOG_(INFO) << "CrashExceptionTest014: end.";
}

/**
 * @tc.name: CrashExceptionTest015
 * @tc.desc: test CheckFaultSummaryValid, valid Fault Summary.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest015: start.";
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
    GTEST_LOG_(INFO) << "CrashExceptionTest015: end.";
}

/**
 * @tc.name: CrashExceptionTest016
 * @tc.desc: test CheckFaultSummaryValid, valid Fault Summary.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest016: start.";
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
    GTEST_LOG_(INFO) << "CrashExceptionTest016: end.";
}

/**
 * @tc.name: CrashExceptionTest017
 * @tc.desc: test CheckFaultSummaryValid, invalid Fault Summary.
 * @tc.type: FUNC
 */
HWTEST_F(CrashExceptionTest, CrashExceptionTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "CrashExceptionTest017: start.";
    std::string summary = std::string("Thread name:sensors\n") +
        "#00 pc 000c5738 /system/lib/ld-musl-arm.so.1(ioctl+72)(b985d2b9b22c3e542388f5803bac6a56)\n" +
        "#01 pc 00007fcf /system/lib/chipset-pub-sdk/libipc_common.z.so(OHOS::BinderConnector::WriteBinder(\n";
    ASSERT_FALSE(CheckFaultSummaryValid(summary));
    GTEST_LOG_(INFO) << "CrashExceptionTest017: end.";
}
} // namespace HiviewDFX
} // namespace OHOS