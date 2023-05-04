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

static int LaunchTestHap(const string& abilityName, const string& bundleName)
{
    string launchCmd = "/system/bin/aa start -a " + abilityName + " -b " + bundleName;
    (void)ExecuteCommands(launchCmd);
    sleep(2); // 2 : sleep 2s
    return GetProcessPid(bundleName);
}

void DumpCatcherCommandTest::SetUpTestCase()
{
    string installCmd = "bm install -p /data/FaultloggerdJsTest.hap";
    (void)ExecuteCommands(installCmd);
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";
    g_testPid = LaunchTestHap(testAbiltyName, testBundleName);
}

void DumpCatcherCommandTest::TearDownTestCase()
{
    string stopCmd = "/system/bin/aa force-stop ";
    stopCmd += TEST_BUNDLE_NAME;
    (void)ExecuteCommands(stopCmd);
    string uninstallCmd = "bm uninstall -n ";
    uninstallCmd += TEST_BUNDLE_NAME;
    (void)ExecuteCommands(uninstallCmd);
}

void DumpCatcherCommandTest::SetUp()
{}

void DumpCatcherCommandTest::TearDown()
{}

static bool CheckProcessComm(int pid)
{
    string cmd = "cat /proc/" + to_string(pid) + "/comm";
    string comm = ExecuteCommands(cmd);
    size_t pos = comm.find('\n');
    if (pos != string::npos) {
        comm.erase(pos, 1);
    }
    if (!strcmp(comm.c_str(), TRUNCATE_TEST_BUNDLE_NAME)) {
        return true;
    }
    return false;
}

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
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "accountmgr";
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        if (dumpRes.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find " << log[i];
        }
    }
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
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "accountmgr";
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        if (dumpRes.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find " << log[i];
        }
    }
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
    if (!CheckProcessComm(g_testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string testCommand = "dumpcatcher -p " + to_string(g_testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(g_testPid);
    log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        if (dumpRes.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find " << log[i];
        }
    }
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
    if (!CheckProcessComm(g_testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string testCommand = "dumpcatcher -p " + to_string(g_testPid) + " -t " + to_string(g_testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(g_testPid);
    log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        if (dumpRes.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find " << log[i];
        }
    }
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
    if (!CheckProcessComm(g_testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string testCommand = "dumpcatcher -p " + to_string(g_testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(g_testPid);
    log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        if (dumpRes.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find " << log[i];
        }
    }
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
    if (!CheckProcessComm(testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string procCMD = "dumpcatcher -m -p " + to_string(testPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn", "Name:DfxWatchdog",
        "Name:GC_WorkerThread", "Name:ace.bg.1" };
    log[0] += to_string(testPid);
    log[1] += TRUNCATE_TEST_BUNDLE_NAME;
    int expectNum = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < expectNum; i++) {
        if (procDumpLog.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(ERROR) << "not found (" << log[i] << ")";
        }
    }
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
    if (!CheckProcessComm(testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    string procCMD = "dumpcatcher -m -p " + to_string(testPid) +
        " -t " + to_string(testPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn" };
    log[0] += to_string(testPid);
    log[1] += TRUNCATE_TEST_BUNDLE_NAME;
    int expectNum = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < expectNum; i++) {
        if (procDumpLog.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(ERROR) << "not found (" << log[i] << ")";
        }
    }
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
    int count = 0;
    string log[] = {"Failed"};
    for (int i = 0; i < 1; i++) {
        if (procDumpLog.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(ERROR) << "not found (" << log[i] << ")";
        }
    }
    EXPECT_EQ(count, 1) << "DumpCatcherCommandTest008 Failed";
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
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest010: start.";
    string systemui = "com.ohos.systemui";
    string procCMD = "dumpcatcher -m -p -1 -t -1";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    for (int i = 0; i < 1; i++) {
        if (procDumpLog.find(log[i]) != string::npos) {
            count++;
        } else {
            GTEST_LOG_(ERROR) << "not found (" << log[i] << ")";
        }
    }
    EXPECT_EQ(count, 1) << "DumpCatcherCommandTest009 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest009: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS