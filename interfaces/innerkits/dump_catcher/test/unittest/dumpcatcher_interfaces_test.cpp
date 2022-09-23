/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <vector>

#include "dfx_dump_catcher.h"

using namespace testing;
using namespace testing::ext;

namespace OHOS {
namespace HiviewDFX {
class DumpCatcherInterfacesTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void DumpCatcherInterfacesTest::SetUpTestCase()
{}

void DumpCatcherInterfacesTest::TearDownTestCase()
{}

void DumpCatcherInterfacesTest::SetUp()
{}

void DumpCatcherInterfacesTest::TearDown()
{}

static int GetProcessPid(std::string applyName)
{
    std::string procCMD = "pgrep '" + applyName + "'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMD;
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    std::string applyPid;
    char resultBufShell[100] = { 0, };
    while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
        applyPid = resultBufShell;
        GTEST_LOG_(INFO) << "applyPid: " << applyPid;
    }
    pclose(procFileInfo);
    return std::atoi(applyPid.c_str());
}

static std::string GetCmdResultFromPopen(const std::string& cmd)
{
    if (cmd.empty()) {
        return "";
    }
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp == nullptr) {
        return "";
    }
    const int bufSize = 128;
    char buffer[bufSize];
    std::string result = "";
    while (!feof(fp)) {
        if (fgets(buffer, bufSize - 1, fp) != nullptr) {
            result += buffer;
        }
    }
    pclose(fp);
    return result;
}

static int GetServicePid(const std::string& serviceName)
{
    std::string cmd = "pidof " + serviceName;
    std::string pidStr = GetCmdResultFromPopen(cmd);
    int32_t pid = 0;
    std::stringstream pidStream(pidStr);
    pidStream >> pid;
    printf("the pid of service(%s) is %s \n", serviceName.c_str(), pidStr.c_str());
    return pid;
}

/**
 * @tc.name: DumpCatcherInterfacesTest001
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(app), PID(telephony)}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest001: start.";
    std::string testProcess1 = "telephony";
    int testPid1 = GetProcessPid(testProcess1);
    GTEST_LOG_(INFO) << "testPid1:" << testPid1;
    std::string testProcess2 = "foundation";
    int testPid2 = GetProcessPid(testProcess2);
    GTEST_LOG_(INFO) << "testPid2:" << testPid2;
    std::vector<int> multiPid {testPid1, testPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = {"Tid:", "Name:", "Tid:", "Name:"};
    log[0] = log[0] + std::to_string(testPid1);
    log[1] = log[1] + testProcess1;
    log[2] = log[2] + std::to_string(testPid2);
    log[3] = log[3] + testProcess2;
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << "DumpCatcherInterfacesTest001 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest001: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest002
 * @tc.desc: test DumpCatchMultiPid API: multiPid{0, 0}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest002: start.";
    int testPid1 = 0;
    GTEST_LOG_(INFO) << "testPid1:" << testPid1;
    int testPid2 = 0;
    GTEST_LOG_(INFO) << "testPid2:" << testPid2;
    std::vector<int> multiPid {testPid1, testPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest002 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest002: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest003
 * @tc.desc: test DumpCatchMultiPid API: multiPid{-11, -11}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest003: start.";
    int testPid1 = -11;
    GTEST_LOG_(INFO) << "testPid1:" << testPid1;
    int testPid2 = -11;
    GTEST_LOG_(INFO) << "testPid2:" << testPid2;
    std::vector<int> multiPid {testPid1, testPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest003 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest003: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest004
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(telephony), 0}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest004: start.";
    std::string testProcess = "telephony";
    int applyPid1 = GetProcessPid(testProcess);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = 0;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "Name:", "Failed" };
    log[0] = log[0] + std::to_string(applyPid1);
    log[1] = log[1] + "telephony";
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << "DumpCatcherInterfacesTest004 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest004: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest005
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(telephony),PID(app),PID(foundation)}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest005: start.";
    std::string testProcess1 = "telephony";
    int testPid1 = GetProcessPid(testProcess1);
    GTEST_LOG_(INFO) << "testPid1:" << testPid1;
    std::string testProcess2 = "foundation";
    int testPid2 = GetProcessPid(testProcess2);
    GTEST_LOG_(INFO) << "testPid2:" << testPid2;
    std::string testProcess3 = "com.ohos.systemui";
    int testPid3 = GetServicePid(testProcess3);
    GTEST_LOG_(INFO) << "testPid3:" << testPid3;
    std::vector<int> multiPid {testPid1, testPid2, testPid3};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "Name:", "Tid:", "Name:", "Tid:", "Name:" };
    log[0] = log[0] + std::to_string(testPid1);
    log[1] = log[1] + testProcess1;
    log[2] = log[2] + std::to_string(testPid2);
    log[3] = log[3] + testProcess2;
    log[4] = log[4] + std::to_string(testPid3);
    log[5] = log[5] + "com.ohos.system";
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << "DumpCatcherInterfacesTest005 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest005: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest006
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(telephony), -11}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest006: start.";
    std::string testProcess = "telephony";
    int testPid1 = GetProcessPid(testProcess);
    GTEST_LOG_(INFO) << "applyPid1:" << testPid1;
    int testPid2 = -11;
    GTEST_LOG_(INFO) << "applyPid2:" << testPid2;
    std::vector<int> multiPid {testPid1, testPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "Name:", "Failed"};
    log[0] = log[0] + std::to_string(testPid1);
    log[1] = log[1] + "telephony";
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << "DumpCatcherInterfacesTest006 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest006: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest007
 * @tc.desc: test DumpCatchMultiPid API: multiPid{9999, 9999}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest007: start.";
    int applyPid = 9999;
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid;
    std::vector<int> multiPid {applyPid, applyPid};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest007 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest007: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest008
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(telephony), 9999}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest0019: start.";
    std::string apply = "telephony";
    int applyPid1 = GetProcessPid(apply);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = 9999;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "Name:", "Failed"};
    log[0] = log[0] + std::to_string(applyPid1);
    log[1] = log[1] + apply;
    int expectNum = sizeof(log) / sizeof(log[0]);
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << "DumpCatcherInterfacesTest008 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest008: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest009
 * @tc.desc: test CatchFrame API: PID(getpid()), TID(gettid())
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest009: start.";
    DfxDumpCatcher dumplog(getpid());
    std::vector<std::shared_ptr<DfxFrame>> frameV;
    bool ret = dumplog.InitFrameCatcher();
    EXPECT_EQ(ret, true);
    ret = dumplog.RequestCatchFrame(gettid());
    EXPECT_EQ(ret, true);
    ret = dumplog.CatchFrame(gettid(), frameV);
    EXPECT_EQ(ret, true);
    dumplog.DestroyFrameCatcher();
    EXPECT_GT(frameV.size(), 0) << "DumpCatcherInterfacesTest009 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest009: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest010
 * @tc.desc: test CatchFrame API: PID(com.ohos.systemui), TID(com.ohos.systemui)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest010: start.";
    std::string testProcess = "com.ohos.systemui";
    int testPid = GetServicePid(testProcess);
    DfxDumpCatcher dumplog(testPid);
    std::vector<std::shared_ptr<DfxFrame>> frameV;
    bool ret = dumplog.InitFrameCatcher();
    EXPECT_EQ(ret, true);
    ret = dumplog.RequestCatchFrame(testPid);
    EXPECT_EQ(ret, false);
    ret = dumplog.CatchFrame(testPid, frameV);
    EXPECT_EQ(ret, false);
    dumplog.DestroyFrameCatcher();
    EXPECT_EQ(frameV.size(), 0) << "DumpCatcherInterfacesTest010 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest010: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest011
 * @tc.desc: test CatchFrame API: TID(gettid())
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest011: start.";
    DfxDumpCatcher dumplog;
    std::vector<std::shared_ptr<DfxFrame>> frameV;
    bool ret = dumplog.CatchFrame(gettid(), frameV);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest011 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest011: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest012
 * @tc.desc: test CatchFrame API: app TID(-11)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest012: start.";
    DfxDumpCatcher dumplog;
    std::vector<std::shared_ptr<DfxFrame>> frameV;
    bool ret = dumplog.CatchFrame(-11, frameV);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest012 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest012: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest013
 * @tc.desc: test CatchFrame API: TID(-1)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest013: start.";
    DfxDumpCatcher dumplog;
    std::vector<std::shared_ptr<DfxFrame>> frameV;
    bool ret = dumplog.CatchFrame(-1, frameV);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest013 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest013: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest014
 * @tc.desc: test DumpCatchMix API: PID(systemui pid), TID(0)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest014: start.";
    std::string systemui = "com.ohos.systemui";
    int systemuiPid = GetServicePid(systemui);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(systemuiPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "comm:com.ohos.system", "#00", "/system/bin/appspawn",
        "comm:dfx_watchdog", "comm:GC_WorkerThread", "comm:ace.bg.1"};
    log[0] += std::to_string(systemuiPid);
    string::size_type idx;
    int j = 0;
    int count = 0;
    int expectNum = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }

    EXPECT_EQ(count, expectNum) << "DumpCatcherInterfacesTest014 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest014: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest015
 * @tc.desc: test DumpCatchMix API: PID(systemui pid), TID(systemui pid)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest015: start.";
    std::string systemui = "com.ohos.systemui";
    int systemuiPid = GetServicePid(systemui);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(systemuiPid, systemuiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "comm:com.ohos.system", "#00", "/system/bin/appspawn"};
    log[0] += std::to_string(systemuiPid);
    int expectNum = sizeof(log) / sizeof(log[0]);
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << "DumpCatcherInterfacesTest015 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest015: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest016
 * @tc.desc: test DumpCatchMix API: PID(systemui pid), TID(-1)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest016: start.";
    std::string systemui = "com.ohos.systemui";
    int systemuiPid = GetServicePid(systemui);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(systemuiPid, -1, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest016 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest016: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest017
 * @tc.desc: test DumpCatchMix API: PID(-1), TID(-1)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest017: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(-1, -1, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest017 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest017: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
