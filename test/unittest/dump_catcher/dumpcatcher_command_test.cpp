/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <string>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_test_util.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
namespace {
static int g_testPid = 0;
}
class DumpCatcherCommandTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void DumpCatcherCommandTest::SetUpTestCase()
{
    InstallTestHap("/data/FaultloggerdJsTest.hap");
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";
    g_testPid = LaunchTestHap(testAbiltyName, testBundleName);
}

void DumpCatcherCommandTest::TearDownTestCase()
{
    StopTestHap(TEST_BUNDLE_NAME);
    UninstallTestHap(TEST_BUNDLE_NAME);
}

void DumpCatcherCommandTest::SetUp()
{}

void DumpCatcherCommandTest::TearDown()
{}

/**
 * @tc.name: DumpCatcherCommandTest001
 * @tc.desc: test dumpcatcher command: -p [accountmgr]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest001: start.";
    int testPid = GetProcessPid("accountmgr");
    string testCommand = "dumpcatcher -p " + to_string(testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "accountmgr";
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest001 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest001: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest002
 * @tc.desc: test dumpcatcher command: -p [accountmgr] -t [main thread]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest002: start.";
    int testPid = GetProcessPid("accountmgr");
    string testCommand = "dumpcatcher -p " + to_string(testPid) + " -t " + to_string(testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "accountmgr";
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest002 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest002: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest003
 * @tc.desc: test dumpcatcher command: -p [test hap]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest003: start.";
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string testCommand = "dumpcatcher -p " + to_string(g_testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(g_testPid);
    log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest003 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest003: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest004
 * @tc.desc: test dumpcatcher command: -p [test hap] -t [main thread]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest004: start.";
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string testCommand = "dumpcatcher -p " + to_string(g_testPid) + " -t " + to_string(g_testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(g_testPid);
    log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest004 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest004: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest005
 * @tc.desc: test dumpcatcher command: -p [test hap]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest005: start.";
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string testCommand = "dumpcatcher -p " + to_string(g_testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(g_testPid);
    log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest005 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest005: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest006
 * @tc.desc: test dumpcatcher command: -m -p [test hap]
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest006: start.";
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    if (testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string procCMD = "dumpcatcher -m -p " + to_string(testPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn", "Name:OS_DfxWatchdog" };
    log[0] += to_string(testPid);
    log[1] += TRUNCATE_TEST_BUNDLE_NAME;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DumpCatcherCommandTest006 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest006: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest007
 * @tc.desc: test dumpcatcher command: -m -p [test hap] -t [main thread]
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest007: start.";
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    if (testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string procCMD = "dumpcatcher -m -p " + to_string(testPid) +
        " -t " + to_string(testPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn" };
    log[0] += to_string(testPid);
    log[1] += TRUNCATE_TEST_BUNDLE_NAME;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DumpCatcherCommandTest007 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest007: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest008
 * @tc.desc: test dumpcatcher command: -m -p [com.ohos.systemui] tid -1
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest008: start.";
    string systemui = "com.ohos.systemui";
    int systemuiPid = GetProcessPid(systemui);
    string procCMD = "dumpcatcher -m -p " + to_string(systemuiPid) + " -t -1";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Failed"};
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DumpCatcherCommandTest008 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest008: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest009
 * @tc.desc: test dumpcatcher command: -m -p -1 tid -1
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest009: start.";
    string systemui = "com.ohos.systemui";
    string procCMD = "dumpcatcher -m -p -1 -t -1";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Failed"};
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest009 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest009: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest010
 * @tc.desc: test dumpcatcher command: -c -p pid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest010: start.";
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    if (testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string procCMD = "dumpcatcher -c -p " + to_string(testPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DumpCatcherCommandTest010 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest010: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest011
 * @tc.desc: test dumpcatcher command: -k -p pid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest011: start.";
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    if (testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(testPid, TRUNCATE_TEST_BUNDLE_NAME)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string procCMD = "dumpcatcher -k -p " + to_string(testPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Failed"};
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DumpCatcherCommandTest011 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest011: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest012
 * @tc.desc: test dumpcatcher command:
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest012: start.";
    string procCMD = "dumpcatcher";
    string procDumpLog = ExecuteCommands(procCMD);
    EXPECT_EQ(procDumpLog, "") << "DumpCatcherCommandTest012 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest012: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest013
 * @tc.desc: test dumpcatcher command: -i
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest013: start.";
    string procCMD = "dumpcatcher -i";
    string procDumpLog = ExecuteCommands(procCMD);
    string log[] = {"Usage:"};
    int expectNum = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, expectNum);
    EXPECT_EQ(count, expectNum) << "DumpCatcherCommandTest013 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest013: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS