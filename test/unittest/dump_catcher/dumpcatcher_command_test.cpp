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
#include <thread>
#include <unistd.h>

#include "dfx_define.h"
#include "dfx_dump_catcher.h"
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

/**
 * @tc.name: DumpCatcherCommandTest001
 * @tc.desc: test dumpcatcher command: -p [powermgr]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest001: start.";
    int testPid = GetProcessPid("powermgr");
    string testCommand = "dumpcatcher -p " + to_string(testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "powermgr";
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest001 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest001: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest002
 * @tc.desc: test dumpcatcher command: -p [powermgr] -t [main thread]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest002: start.";
    int testPid = GetProcessPid("powermgr");
    string testCommand = "dumpcatcher -p " + to_string(testPid) + " -t " + to_string(testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "powermgr";
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
    bool isSuccess = g_testPid != 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
    } else {
        isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Error process comm";
        } else {
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
    }
}

/**
 * @tc.name: DumpCatcherCommandTest004
 * @tc.desc: test dumpcatcher command: -p [test hap] -t [main thread]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest004: start.";
    bool isSuccess = g_testPid != 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
    } else {
        isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Error process comm";
        } else {
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
    }
}

/**
 * @tc.name: DumpCatcherCommandTest005
 * @tc.desc: test dumpcatcher command: -p [test hap]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest005: start.";
    bool isSuccess = g_testPid != 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
    } else {
        isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Error process comm";
        } else {
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
    }
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

/**
 * @tc.name: DumpCatcherCommandTest014
 * @tc.desc: test dumpcatcher command: -p 1 tid 1
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest014: start.";
    string systemui = "init";
    string procCMD = "dumpcatcher -p 1 -t 1";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Pid:1", "Name:init", "#00", "#01", "#02"};
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(procDumpLog, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest014 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest014: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest015
 * @tc.desc: test dumpcatcher command: -p [powermgr] -t [main thread] -T [1500]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest015: start.";
    int testPid = GetProcessPid("powermgr");
    int timeout = 1500;
    string testCommand = "dumpcatcher -p " + to_string(testPid) + " -t " + to_string(testPid);
    testCommand += " -T " + to_string(timeout);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "powermgr";
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest015 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest015: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest016
 * @tc.desc: test dumpcatcher command: -c save
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest016: start.";
    string testCommand = "dumpcatcher -c save";
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"input", "error"};
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest016 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest016: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest017
 * @tc.desc: test dumpcatcher command: -c save 0
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest017: start.";
    string testCommand = "dumpcatcher -c save 0";
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"pid", "error"};
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest017 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest017: end.";
}

/**
 * @tc.name: DumpCatcherCommandTest018
 * @tc.desc: test dumpcatcher command: -k [test hap]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest018: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        bool isSuccess = g_testPid != 0;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        } else {
            isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
            if (!isSuccess) {
                ASSERT_FALSE(isSuccess);
                GTEST_LOG_(ERROR) << "Error process comm";
            } else {
                string testCommand = "dumpcatcher -k " + to_string(g_testPid);
                string dumpRes = ExecuteCommands(testCommand);
                GTEST_LOG_(INFO) << dumpRes;
                string log[] = {"tid=", "name=", "/system"};
                log[0] = log[0] + to_string(g_testPid);
                log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
                int len = sizeof(log) / sizeof(log[0]);
                int count = GetKeywordsNum(dumpRes, log, len);
                EXPECT_EQ(count, len) << "DumpCatcherCommandTest018 Failed";
                GTEST_LOG_(INFO) << "DumpCatcherCommandTest018: end.";
            }
        }
    }
}

/**
 * @tc.name: DumpCatcherCommandTest019
 * @tc.desc: test dumpcatcher command: -k [test hap] -a
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest019: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        bool isSuccess = g_testPid != 0;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        } else {
            isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
            if (!isSuccess) {
                ASSERT_FALSE(isSuccess);
                GTEST_LOG_(ERROR) << "Error process comm";
            } else {
                string testCommand = "dumpcatcher -k " + to_string(g_testPid) + " -a";
                string dumpRes = ExecuteCommands(testCommand);
                GTEST_LOG_(INFO) << dumpRes;
                string log[] = {"tid=", "name=", "/system"};
                log[0] = log[0] + to_string(g_testPid);
                log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
                int len = sizeof(log) / sizeof(log[0]);
                int count = GetKeywordsNum(dumpRes, log, len);
                EXPECT_EQ(count, len) << "DumpCatcherCommandTest019 Failed";
                GTEST_LOG_(INFO) << "DumpCatcherCommandTest019: end.";
            }
        }
    }
}

/**
 * @tc.name: DumpCatcherCommandTest020
 * @tc.desc: test dumpcatcher command: -k [test hap] -f
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest020: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        bool isSuccess = g_testPid != 0;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        } else {
            isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
            if (!isSuccess) {
                ASSERT_FALSE(isSuccess);
                GTEST_LOG_(ERROR) << "Error process comm";
            } else {
                string testCommand = "dumpcatcher -k " + to_string(g_testPid) + " -f";
                string dumpRes = ExecuteCommands(testCommand);
                GTEST_LOG_(INFO) << dumpRes;
                string log[] = {"Tid:", "Name:", "#00", "#01", "#02", "/system"};
                log[0] = log[0] + to_string(g_testPid);
                log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
                int len = sizeof(log) / sizeof(log[0]);
                int count = GetKeywordsNum(dumpRes, log, len);
                EXPECT_EQ(count, len) << "DumpCatcherCommandTest020 Failed";
                GTEST_LOG_(INFO) << "DumpCatcherCommandTest020: end.";
            }
        }
    }
}

/**
 * @tc.name: DumpCatcherCommandTest021
 * @tc.desc: test dumpcatcher command: -k [test hap] -a -f
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest021: start.";
    std::string res = ExecuteCommands("uname");
    if (res.find("Linux") != std::string::npos) {
        ASSERT_NE(res.find("Linux"), std::string::npos);
    } else {
        bool isSuccess = g_testPid != 0;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        } else {
            isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
            if (!isSuccess) {
                ASSERT_FALSE(isSuccess);
                GTEST_LOG_(ERROR) << "Error process comm";
            } else {
                string testCommand = "dumpcatcher -k " + to_string(g_testPid) + " -a -f";
                string dumpRes = ExecuteCommands(testCommand);
                GTEST_LOG_(INFO) << dumpRes;
                string log[] = {"Tid:", "Name:", "#00", "#01", "#02", "/system"};
                log[0] = log[0] + to_string(g_testPid);
                log[1] = log[1] + TRUNCATE_TEST_BUNDLE_NAME;
                int len = sizeof(log) / sizeof(log[0]);
                int count = GetKeywordsNum(dumpRes, log, len);
                EXPECT_EQ(count, len) << "DumpCatcherCommandTest021 Failed";
                GTEST_LOG_(INFO) << "DumpCatcherCommandTest021: end.";
            }
        }
    }
}

/**
 * @tc.name: DumpCatcherCommandTest022
 * @tc.desc: test dumpcatcher command: -p [hdf_devmgr] -t [main thread]
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherCommandTest, DumpCatcherCommandTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest022: start.";
    int testPid = GetProcessPid("hdf_devmgr");
    string testCommand = "dumpcatcher -p " + to_string(testPid) + " -t " + to_string(testPid);
    string dumpRes = ExecuteCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "hdf_devmgr";
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(dumpRes, log, len);
    EXPECT_EQ(count, len) << "DumpCatcherCommandTest022 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherCommandTest022: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS