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
#include <thread>
#include <vector>

#include <unistd.h>

#include "dfx_define.h"
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

static const int THREAD_ALIVE_TIME = 2;

static const int CREATE_THREAD_TIMEOUT = 300000;

static pid_t g_threadId = 0;

static pid_t g_processId = 0;

int g_testPid = 0;

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

static int LaunchTestHap(const std::string& abilityName, const std::string& bundleName)
{
    std::string launchCmd = "/system/bin/aa start -a " + abilityName + " -b " + bundleName;
    (void)GetCmdResultFromPopen(launchCmd);
    sleep(2); // 2 : sleep 2s
    return GetServicePid(bundleName);
}

void DumpCatcherInterfacesTest::SetUpTestCase()
{
    std::string installCmd = "bm install -p /data/FaultloggerdJsTest.hap";
    (void)GetCmdResultFromPopen(installCmd);
    std::string testBundleName = TEST_BUNDLE_NAME;
    std::string testAbiltyName = testBundleName + ".MainAbility";
    g_testPid = LaunchTestHap(testAbiltyName, testBundleName);
}

void DumpCatcherInterfacesTest::TearDownTestCase()
{
    std::string stopCmd = "/system/bin/aa force-stop ";
    stopCmd += TEST_BUNDLE_NAME;
    (void)GetCmdResultFromPopen(stopCmd);

    std::string uninstallCmd = "bm uninstall -n ";
    uninstallCmd += TEST_BUNDLE_NAME;
    (void)GetCmdResultFromPopen(uninstallCmd);
}

void DumpCatcherInterfacesTest::SetUp()
{}

void DumpCatcherInterfacesTest::TearDown()
{}

static void* CreateThread(void *argv)
{
    g_threadId = gettid();
    GTEST_LOG_(INFO) << "create MultiThread " << gettid();
    sleep(THREAD_ALIVE_TIME);
    GTEST_LOG_(INFO) << "create MultiThread thread sleep end.";
    return nullptr;
}

static int MultiThreadConstructor(void)
{
    pthread_t thread;

    pthread_create(&thread, nullptr, CreateThread, nullptr);
    pthread_detach(thread);
    usleep(CREATE_THREAD_TIMEOUT);
    return 0;
}

static void ForkMultiThreadProcess(void)
{
    int pid = fork();
    if (pid == 0) {
        MultiThreadConstructor();
        _exit(0);
    } else if (pid < 0) {
        GTEST_LOG_(INFO) << "ForkMultiThreadProcess fail. ";
    } else {
        g_processId = pid;
        GTEST_LOG_(INFO) << "ForkMultiThreadProcess success, pid: " << pid;
        usleep(CREATE_THREAD_TIMEOUT);
        GTEST_LOG_(INFO) << "ForkMultiThreadProcess success, thread id: " << g_threadId;
    }
}

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

static bool CheckProcessComm(int pid)
{
    std::string cmd = "cat /proc/" + std::to_string(pid) + "/comm";
    std::string comm = GetCmdResultFromPopen(cmd);
    size_t pos = comm.find('\n');
    if (pos != std::string::npos) {
        comm.erase(pos, 1);
    }
    if (!strcmp(comm.c_str(), TRUNCATE_TEST_BUNDLE_NAME)) {
        return true;
    }
    return false;
}

/**
 * @tc.name: DumpCatcherInterfacesTest001
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(app), PID(accountmgr)}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest001: start.";
    std::string testProcess1 = "accountmgr";
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
        if (idx != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find "+ log[j];
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << msg << "DumpCatcherInterfacesTest001 Failed";
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
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest003 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest003: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest004
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr), 0}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest004: start.";
    std::string testProcess = "accountmgr";
    int applyPid1 = GetProcessPid(testProcess);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = 0;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    string log[] = { "Tid:", "Name:", "Failed" };
    log[0] = log[0] + std::to_string(applyPid1);
    log[1] = log[1] + "accountmgr";
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find "+ log[j];
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << msg << "DumpCatcherInterfacesTest004 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest004: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest005
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr),PID(app),PID(foundation)}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest005: start.";
    std::string testProcess1 = "accountmgr";
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
    string log[] = { "Tid:", "Name:", "Tid:", "Name:", "Tid:", "Name:" };
    log[0] = log[0] + std::to_string(testPid1);
    log[1] = log[1] + testProcess1;
    log[2] = log[2] + std::to_string(testPid2);
    log[3] = log[3] + testProcess2;
    log[4] = log[4] + std::to_string(testPid3);
    log[5] = log[5] + "m.ohos.systemui";
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find "+ log[j];
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << msg << "DumpCatcherInterfacesTest005 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest005: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest006
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr), -11}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest006: start.";
    std::string testProcess = "accountmgr";
    int testPid1 = GetProcessPid(testProcess);
    GTEST_LOG_(INFO) << "applyPid1:" << testPid1;
    int testPid2 = -11;
    GTEST_LOG_(INFO) << "applyPid2:" << testPid2;
    std::vector<int> multiPid {testPid1, testPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    string log[] = { "Tid:", "Name:", "Failed"};
    log[0] = log[0] + std::to_string(testPid1);
    log[1] = log[1] + "accountmgr";
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find "+ log[j];
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << msg << "DumpCatcherInterfacesTest006 Failed";
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
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest007 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest007: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest008
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr), 9999}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest008: start.";
    std::string apply = "accountmgr";
    int applyPid1 = GetProcessPid(apply);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = 9999;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    string log[] = { "Tid:", "Name:", "Failed"};
    log[0] = log[0] + std::to_string(applyPid1);
    log[1] = log[1] + apply;
    int expectNum = sizeof(log) / sizeof(log[0]);
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find "+ log[j];
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << msg << "DumpCatcherInterfacesTest008 Failed";
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
    std::vector<NativeFrame> frameV;
    bool ret = dumplog.InitFrameCatcher();
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
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    DfxDumpCatcher dumplog(g_testPid);
    std::vector<NativeFrame> frameV;
    bool ret = dumplog.InitFrameCatcher();
    EXPECT_EQ(ret, true);
    ret = dumplog.CatchFrame(g_testPid, frameV);
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
    std::vector<NativeFrame> frameV;
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
    std::vector<NativeFrame> frameV;
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
    std::vector<NativeFrame> frameV;
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
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(g_testPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn",
        "Name:DfxWatchdog", "Name:GC_WorkerThread", "Name:ace.bg.1"};
    log[0] += std::to_string(g_testPid);
    log[1] += TRUNCATE_TEST_BUNDLE_NAME;
    string::size_type idx;
    int j = 0;
    int count = 0;
    int expectNum = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find "+ log[j];
        }
        j++;
    }

    EXPECT_EQ(count, expectNum) << msg << "DumpCatcherInterfacesTest014 Failed";
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
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(g_testPid, g_testPid, msg);
    GTEST_LOG_(INFO) << ret;
    string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn"};
    log[0] += std::to_string(g_testPid);
    log[1] += TRUNCATE_TEST_BUNDLE_NAME;
    int expectNum = sizeof(log) / sizeof(log[0]);
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < expectNum; i++) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        } else {
            GTEST_LOG_(INFO) << "Can not find "+ log[j];
        }
        j++;
    }
    EXPECT_EQ(count, expectNum) << msg << "DumpCatcherInterfacesTest015 Failed";
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
    if (g_testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    if (!CheckProcessComm(g_testPid)) {
        GTEST_LOG_(ERROR) << "Error process comm";
        return;
    }
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(g_testPid, -1, msg);
    GTEST_LOG_(INFO) << ret;
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
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest017 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest017: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest018
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest018: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(getpid(), gettid(), msg, 1);
    GTEST_LOG_(INFO) << ret;

    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest018 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest018: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest019
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest019: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(getpid(), 0, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest019 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest019: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest020
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest020: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(getpid(), -1, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest020 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest020: end.";
}


/**
 * @tc.name: DumpCatcherInterfacesTest021
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest021: start.";
    std::string apply = "accountmgr";
    int applyPid = GetProcessPid(apply);
    GTEST_LOG_(INFO) << "apply:" << apply << ", pid:" << applyPid;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(applyPid, 0, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest021 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest021: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest022
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest022: start.";
    std::string apply = "accountmgr";
    int applyPid = GetProcessPid(apply);
    GTEST_LOG_(INFO) << "apply:" << apply << ", pid:" << applyPid;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(applyPid, applyPid, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest022 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest022: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest023
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest023: start.";
    std::string apply = "accountmgr";
    int applyPid = GetProcessPid(apply);
    GTEST_LOG_(INFO) << "apply:" << apply << ", pid:" << applyPid;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(applyPid, -1, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest023 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest023: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest024
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest024: start.";
    std::string apply = "accountmgr";
    int applyPid = GetProcessPid(apply);
    GTEST_LOG_(INFO) << "apply:" << apply << ", pid:" << applyPid;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(applyPid, 9999, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest024 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest024: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest025
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest025, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest025: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(getpid(), 9999, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest025 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest025: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest026
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest026, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest026: start.";
    MultiThreadConstructor();

    DfxDumpCatcher dumplog;
    std::string msg = "";
    GTEST_LOG_(INFO) << "dump local process, "  << " tid:" << g_threadId;
    bool ret = dumplog.DumpCatchFd(getpid(), g_threadId, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest026 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest026: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest027
 * @tc.desc: test DumpCatchFd API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest027, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest027: start.";
    ForkMultiThreadProcess();

    GTEST_LOG_(INFO) << "dump remote process, "  << " pid:" << g_processId << ", tid:" << g_threadId;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchFd(g_processId, g_threadId, msg, 1);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest027 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest027: end.";
}

int32_t g_tid = 0;
std::mutex g_mutex;
__attribute__((noinline)) void Test002()
{
    printf("Test002\n");
    g_mutex.lock();
    g_mutex.unlock();
}

__attribute__((noinline)) void Test001()
{
    g_tid = gettid();
    printf("Test001:%d\n", g_tid);
    Test002();
}

/**
 * @tc.name: DumpCatcherInterfacesTest028
 * @tc.desc: test unwind in newly create threads
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest028, TestSize.Level2)
{
    DfxDumpCatcher dumplog(getpid());
    std::vector<NativeFrame> frames;
    bool result = dumplog.InitFrameCatcher();
    EXPECT_EQ(result, true);

    g_mutex.lock();
    std::thread worker(Test001);
    sleep(1);
    if (g_tid <= 0) {
        result = false;
    }

    bool hasEmptyBinaryName = false;
    if (result) {
        result = dumplog.CatchFrame(g_tid, frames);
        for (auto& frame : frames) {
            printf("name:%s\n", frame.binaryName.c_str());
            if (frame.binaryName.empty()) {
                hasEmptyBinaryName = true;
            }
        }
    }

    g_mutex.unlock();
    g_tid = 0;
    if (worker.joinable()) {
        worker.join();
    }

    dumplog.DestroyFrameCatcher();
    if (hasEmptyBinaryName) {
        FAIL() << "BinaryName should not be empty.\n";
    }
    EXPECT_EQ(result, true);
    EXPECT_GT(frames.size(), 0) << "DumpCatcherInterfacesTest028 Failed";
}
} // namespace HiviewDFX
} // namepsace OHOS
