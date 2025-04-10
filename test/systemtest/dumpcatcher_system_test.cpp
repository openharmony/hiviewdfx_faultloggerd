/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <fstream>
#include <gtest/gtest.h>
#include <securec.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "dfx_dump_catcher.h"
#include "dfx_test_util.h"
#include "file_util.h"
#include "procinfo.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class DumpCatcherSystemTest : public testing::Test {
public:
    static void SetUpTestCase(void);
    static void TearDownTestCase(void);
    void SetUp();
    void TearDown();
};

void DumpCatcherSystemTest::SetUpTestCase(void)
{
    chmod("/data/crasher_c", 0755); // 0755 : -rwxr-xr-x
    chmod("/data/crasher_cpp", 0755); // 0755 : -rwxr-xr-x
}

void DumpCatcherSystemTest::TearDownTestCase(void)
{
}

void DumpCatcherSystemTest::SetUp(void)
{
}

void DumpCatcherSystemTest::TearDown(void)
{
}

namespace {
static const int ROOT_UID = 0;
static const int BMS_UID = 1000;
static const int OTHER_UID = 10000;

static const int MULTITHREAD_TEST_COUNT = 50;
static const int ARRAY_SIZE = 100;
static pid_t g_rootTid[ARRAY_SIZE] = { -1 };
static pid_t g_appTid[ARRAY_SIZE] = { -1 };
static pid_t g_sysTid[ARRAY_SIZE] = { -1 };

static pid_t g_loopSysPid = 0;
static pid_t g_loopRootPid = 0;
static pid_t g_loopCppPid = 0;
static pid_t g_loopAppPid = 0;
static unsigned int g_unsignedLoopSysPid = 0;
static int g_checkCnt = 0;

enum CrasherRunType {
    ROOT,           // rus as root uid
    SYSTEM,         // run as system uid
    APP_CRASHER_C,  // crasher_c run as app uid
    APP_CRASHER_CPP // crasher_cpp run as app uid
};
}

static string GetPidMax()
{
    const string defaultPidMax = "32768"; // 32768 pid max
    const string path = "/proc/sys/kernel/pid_max";
    string pidMax = defaultPidMax;
    OHOS::HiviewDFX::LoadStringFromFile(path, pidMax);
    return pidMax;
}

static string GetTidMax()
{
    const string defaultTidMax = "8825"; // 8825 tid max
    const string path = "/proc/sys/kernel/threads-max";
    string tidMax = defaultTidMax;
    OHOS::HiviewDFX::LoadStringFromFile(path, tidMax);
    return tidMax;
}

static void GetCrasherThreads(const int pid, const int uid)
{
    if (pid <= 0) {
        return;
    }

    usleep(5); // 5 : sleep 5us
    std::vector<int> tids;
    if (!GetTidsByPidWithFunc(pid, tids, nullptr)) {
        return;
    }
    for (size_t i = 0; i < tids.size(); ++i) {
        if (uid == ROOT_UID) {
            g_rootTid[i] = tids[i];
        } else if (uid == BMS_UID) {
            g_sysTid[i] = tids[i];
        } else {
            g_appTid[i] = tids[i];
        }
    }
}

static void StartCrasherLoopForUnsignedPidAndTid(const CrasherType type)
{
    setuid(BMS_UID);
    if (type == CRASHER_C) {
        system("/data/crasher_c thread-Loop &");
    } else {
        system("/data/crasher_cpp thread-Loop &");
    }
    string procCmd = "pgrep 'crasher'";
    std::string shellRes = ExecuteCommands(procCmd);
    if (shellRes.empty()) {
        exit(1);
    }
    g_unsignedLoopSysPid = atoi(shellRes.c_str());
    if (g_unsignedLoopSysPid == 0) {
        exit(0);
    }

    GetCrasherThreads(g_unsignedLoopSysPid, BMS_UID);
}

static void LaunchCrasher(const CrasherType type, const int uid)
{
    setuid(uid);
    if (type == CRASHER_C) {
        system("/data/crasher_c thread-Loop &");
    } else {
        system("/data/crasher_cpp thread-Loop &");
    }

    string procCmd = "pgrep 'crasher'";
    std::vector<std::string> ress;
    ExecuteCommands(procCmd, ress);
    if (ress.empty()) {
        exit(0);
    }

    pid_t curPid = 0;
    for (size_t i = 0; i < ress.size(); ++i) {
        pid_t pid = atoi(ress[i].c_str());
        if (pid <= 0) {
            continue;
        }

        if (uid == ROOT_UID) {
            if (g_loopSysPid == pid) {
                continue;
            }
            if (type == CRASHER_CPP) {
                g_loopCppPid = pid;
            } else {
                g_loopRootPid = pid;
            }
            curPid = g_loopRootPid;
            GTEST_LOG_(INFO) << "Root ID: " << g_loopRootPid;
        } else if (uid == BMS_UID) {
            g_loopSysPid = pid;
            curPid = g_loopSysPid;
            GTEST_LOG_(INFO) << "System ID: " << g_loopSysPid;
        } else {
            if ((g_loopSysPid == pid) || (g_loopRootPid == pid)) {
                continue;
            } else {
                g_loopAppPid = pid;
            }
            curPid = g_loopAppPid;
            GTEST_LOG_(INFO) << "APP ID: " << g_loopAppPid;
        }
    }
    GetCrasherThreads(curPid, uid);
}

static void StartCrasherLoop(const CrasherRunType type)
{
    switch (type) {
        case ROOT:
            LaunchCrasher(CRASHER_C, ROOT_UID);
            break;
        case SYSTEM:
            LaunchCrasher(CRASHER_C, BMS_UID);
            break;
        case APP_CRASHER_C:
            LaunchCrasher(CRASHER_C, OTHER_UID);
            break;
        case APP_CRASHER_CPP:
            LaunchCrasher(CRASHER_CPP, OTHER_UID);
            break;
        default:
            return;
    }
    setuid(OTHER_UID);
}

static void StopCrasherLoop(const CrasherRunType type)
{
    switch (type) {
        case ROOT:
            setuid(ROOT_UID);
            system(("kill -9 " + to_string(g_loopRootPid)).c_str());
            break;
        case SYSTEM:
            setuid(BMS_UID);
            system(("kill -9 " + to_string(g_loopSysPid)).c_str());
            break;
        case APP_CRASHER_C:
            setuid(ROOT_UID);
            system(("kill -9 " + to_string(g_loopAppPid)).c_str());
            break;
        case APP_CRASHER_CPP:
            setuid(BMS_UID);
            system(("kill -9 " + to_string(g_unsignedLoopSysPid)).c_str());
            break;
        default:
            break;
    }
}

/**
 * @tc.name: DumpCatcherSystemTest001
 * @tc.desc: test DumpCatch API: app PID(app), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest001: start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    GTEST_LOG_(INFO) << "appPid: " << g_loopAppPid;
    bool ret = dumplog.DumpCatch(g_loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest001 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest001: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest002
 * @tc.desc: test DumpCatch API: app PID(app), TID(PID)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest002: start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopAppPid, g_loopAppPid, msg);
    GTEST_LOG_(INFO) << ret;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest002 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest002: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest003
 * @tc.desc: test DumpCatch API: app PID(app), TID(<>PID)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest003: start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    int tid = g_appTid[0];
    if (g_loopAppPid == g_appTid[0]) {
        tid = g_appTid[1];
    }
    bool ret = dumplog.DumpCatch(g_loopAppPid, tid, msg);
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest003 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest003: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest004
 * @tc.desc: test DumpCatch API: app PID(system), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest004: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopSysPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest004 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest004: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest005
 * @tc.desc: test DumpCatch API: app PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest005: start.";
    StartCrasherLoop(ROOT);
    StartCrasherLoop(APP_CRASHER_C);
    setuid(OTHER_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopRootPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest005 Failed";
    StopCrasherLoop(ROOT);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest005: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest006
 * @tc.desc: test DumpCatch API: app PID(9999), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest006: start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(9999, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest006 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest006: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest007
 * @tc.desc: test DumpCatch API: app PID(app), TID(9999)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest007: start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopAppPid, 9999, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest007 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest007: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest008
 * @tc.desc: test DumpCatch API: app PID(app), TID(system)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest008 start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopAppPid, g_loopSysPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest008 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest008: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest009
 * @tc.desc: test DumpCatch API: app PID(0), TID(app)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest009 start.";
    StartCrasherLoop(APP_CRASHER_C);
    setuid(OTHER_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(0, g_loopAppPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest009 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest009: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest010
 * @tc.desc: test DumpCatch API: PID(-11), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest010 start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(-11, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest010 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest010: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest011
 * @tc.desc: test DumpCatch API: PID(root), TID(-11)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest011 start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopRootPid, -11, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest011 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest011: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest012
 * @tc.desc: test DumpCatch API: system PID(system), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest012: start.";
    StartCrasherLoop(SYSTEM);
    setuid(BMS_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopSysPid, 0, msg);
    GTEST_LOG_(INFO) << "g_loopSysPid : \n" << g_loopSysPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] + to_string(g_loopSysPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest012 Failed";
    StopCrasherLoop(SYSTEM);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest012: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest013
 * @tc.desc: test DumpCatch API: root PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest013: start.";
    StartCrasherLoop(ROOT);
    setuid(ROOT_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopRootPid, 0, msg);
    GTEST_LOG_(INFO) << "g_loopRootPid : \n" << g_loopRootPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] + to_string(g_loopRootPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest013 Failed";
    StopCrasherLoop(ROOT);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest013: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest014
 * @tc.desc: test DumpCatch API: system PID(app), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest014: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(APP_CRASHER_C);
    setuid(BMS_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "g_loopAppPid : \n" << g_loopAppPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] + to_string(g_loopAppPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest014 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest014: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest015
 * @tc.desc: test DumpCatch API: system PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest015: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(ROOT);
    setuid(BMS_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopRootPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "g_loopRootPid : \n" << g_loopRootPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] + to_string(g_loopRootPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest015 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(ROOT);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest015: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest016
 * @tc.desc: test DumpCatch API: root PID(system), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest016: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(ROOT);
    setuid(ROOT_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopSysPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "g_loopSysPid : \n" << g_loopSysPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] + to_string(g_loopSysPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest016 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(ROOT);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest016: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest017
 * @tc.desc: test DumpCatch API: root PID(app), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest017: start.";
    StartCrasherLoop(APP_CRASHER_C);
    StartCrasherLoop(ROOT);
    setuid(ROOT_UID);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "g_loopAppPid : \n" << g_loopAppPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] + to_string(g_loopAppPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest017 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    StopCrasherLoop(ROOT);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest017: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest018
 * @tc.desc: test DumpCatch API: app PID(app), TID(root)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest018 start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopAppPid, g_loopRootPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest018 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest018: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest019
 * @tc.desc: test DumpCatch API: PID(root), TID(app)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest019 start.";
    StartCrasherLoop(APP_CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopRootPid, g_loopAppPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest019 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest019: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest020
 * @tc.desc: test dumpcatcher command: dumpcatcher -p apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest020: start uid:" << getuid();
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest020 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest020: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest021
 * @tc.desc: test dumpcatcher command: dumpcatcher -p apppid -t apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest021: start uid:" << getuid();
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid) + " -t " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest021 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest021: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest022
 * @tc.desc: test dumpcatcher command: dumpcatcher -p apppid -t apptid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest022: start uid:" << getuid();
    StartCrasherLoop(APP_CRASHER_C);
    int tid = g_appTid[0];
    if (g_loopAppPid == g_appTid[0]) {
        tid = g_appTid[1];
    }
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid) + " -t " + to_string(tid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest022 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest022: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest023
 * @tc.desc: test dumpcatcher command: -p systempid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest023: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopSysPid) + " -t " + to_string(g_loopSysPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest023 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest023: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest024
 * @tc.desc: test dumpcatcher command: -p rootpid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest024: start.";
    StartCrasherLoop(ROOT);
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopRootPid) + " -t " + to_string(g_loopRootPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest024 Failed";
    StopCrasherLoop(ROOT);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest024: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest025
 * @tc.desc: test dumpcatcher command: -p 9999 -t apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest025, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest025: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p 9999 -t "+ to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest025 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest025: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest026
 * @tc.desc: test dumpcatcher command: -p apppid -t 9999
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest026, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest026: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid) + " -t 9999";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest026 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest026: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest027
 * @tc.desc: test dumpcatcher command: -p apppid -t systempid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest027, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest027: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid) + " -t " + to_string(g_loopSysPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest027 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest027: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest028
 * @tc.desc: test dumpcatcher command: -p systempid -t apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest028, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest028: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopSysPid) + " -t " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest028 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest028: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest029
 * @tc.desc: test dumpcatcher command: -p  -t apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest029, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest029: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p  -t " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest029 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest029: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest030
 * @tc.desc: test dumpcatcher command: -p apppid -t
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest030, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest030: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid) + " -t ";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest030 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest030: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest031
 * @tc.desc: test dumpcatcher command: -p -11 -t apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest031, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest031: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p -11 -t " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest031 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest031: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest032
 * @tc.desc: test dumpcatcher command: -p apppid -t -11
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest032, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest032: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid) + " -t -11";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest032 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest032: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest033
 * @tc.desc: test dumpcatcher command: -p systempid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest033, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest033: start.";
    StartCrasherLoop(SYSTEM);
    setuid(BMS_UID);
    string procCMD = "dumpcatcher -p " + to_string(g_loopSysPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + to_string(g_loopSysPid);
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest033 Failed";
    StopCrasherLoop(SYSTEM);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest033: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest034
 * @tc.desc: test dumpcatcher command: -p rootpid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest034, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest034: start.";
    StartCrasherLoop(ROOT);
    setuid(ROOT_UID);
    string procCMD = "dumpcatcher -p " + to_string(g_loopRootPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + to_string(g_loopRootPid);
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest034 Failed";
    StopCrasherLoop(ROOT);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest034: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest035
 * @tc.desc: test dumpcatcher command: -p apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest035, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest035: start.";
    StartCrasherLoop(APP_CRASHER_C);
    StartCrasherLoop(SYSTEM);
    setuid(BMS_UID);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + to_string(g_loopAppPid);
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest035 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    StopCrasherLoop(SYSTEM);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest035: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest036
 * @tc.desc: test dumpcatcher command: -p apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest036, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest036: start.";
    StartCrasherLoop(ROOT);
    StartCrasherLoop(SYSTEM);
    setuid(BMS_UID);
    string procCMD = "dumpcatcher -p " + to_string(g_loopRootPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + to_string(g_loopRootPid);
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    setuid(OTHER_UID);
    EXPECT_EQ(count, 5) << "DumpCatcherSystemTest036 Failed";
    StopCrasherLoop(ROOT);
    StopCrasherLoop(SYSTEM);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest036: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest037
 * @tc.desc: test dumpcatcher command: -p apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest037, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest037: start.";
    StartCrasherLoop(APP_CRASHER_C);
    StartCrasherLoop(ROOT);
    setuid(ROOT_UID);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher", ""};
    log[0] = log[0] + to_string(g_appTid[0]);
    log[5] = log[5] + to_string(g_appTid[1]);
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    setuid(OTHER_UID);
    EXPECT_EQ(count, 6) << "DumpCatcherSystemTest037 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    StopCrasherLoop(ROOT);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest037: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest038
 * @tc.desc: test dumpcatcher command: -p sytempid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest038, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest038: start.";
    StartCrasherLoop(SYSTEM);
    StartCrasherLoop(ROOT);
    setuid(ROOT_UID);
    string procCMD = "dumpcatcher -p " + to_string(g_loopSysPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher", ""};
    log[0] = log[0] + to_string(g_sysTid[0]);
    log[5] = log[5] + to_string(g_sysTid[1]);
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    setuid(OTHER_UID);
    EXPECT_EQ(count, 6) << "DumpCatcherSystemTest038 Failed";
    StopCrasherLoop(SYSTEM);
    StopCrasherLoop(ROOT);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest038: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest039
 * @tc.desc: test dumpcatcher command: -p apppid -t rootpid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest039, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest039: start.";
    StartCrasherLoop(ROOT);
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid) + " -t " + to_string(g_loopRootPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest039 Failed";
    StopCrasherLoop(ROOT);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest039: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest040
 * @tc.desc: test dumpcatcher command: -p rootpid, -t apppid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest040, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest040: start.";
    StartCrasherLoop(ROOT);
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + to_string(g_loopRootPid) + " -t " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest040 Failed";
    StopCrasherLoop(ROOT);
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest040: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest041
 * @tc.desc: test dumpcatcher command: -p pid-max, -t threads_max
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest041, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest041: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p " + GetPidMax() + " -t " + GetTidMax();
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest041 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest041: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest042
 * @tc.desc: test dumpcatcher command: -p 65535, -t 65535
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest042, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest042: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p 65535 -t 65535";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest042 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest042: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest043
 * @tc.desc: test dumpcatcher command: -p 65536, -t 65536
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest043, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest043: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p 65536 -t 65536";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest043 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest043: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest044
 * @tc.desc: test dumpcatcher command: -p 65534, -t 65534
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest044, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest044: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p 65534 -t 65534";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest044 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest044: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest045
 * @tc.desc: test CPP DumpCatch API: PID(apppid), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest045, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest045: start uid:" << getuid();
    StartCrasherLoop(APP_CRASHER_CPP);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_FALSE(ret) << "DumpCatcherSystemTest045 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest045: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest046
 * @tc.desc: test dumpcatcher command: -p rootpid
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest046, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest046: start uid:" << getuid();
    StartCrasherLoop(APP_CRASHER_CPP);
    string procCMD = "dumpcatcher -p " + to_string(g_loopAppPid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"failed"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    EXPECT_EQ(count, 1) << "DumpCatcherSystemTest046 Failed";
    StopCrasherLoop(APP_CRASHER_C);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest046: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest047
 * @tc.desc: test DumpCatch API: app unsigned PID(systempid), unsigned TID(systempid)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest,  DumpCatcherSystemTest047, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest047: start.";
    StartCrasherLoopForUnsignedPidAndTid(CRASHER_C);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_unsignedLoopSysPid, g_unsignedLoopSysPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = {"Tid:", "Name:crasher", "#00", "/data/crasher"};
    log[0] = log[0] + to_string(g_unsignedLoopSysPid);
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 4) << "DumpCatcherSystemTest047 Failed";
    StopCrasherLoop(APP_CRASHER_CPP);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest047: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest048
 * @tc.desc: test DumpCatch API: app unsigned PID(systempid), unsigned TID(systempid)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest,  DumpCatcherSystemTest048, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest048: start.";
    StartCrasherLoopForUnsignedPidAndTid(CRASHER_CPP);
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(g_unsignedLoopSysPid, g_unsignedLoopSysPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = {"Tid:", "Name:crasher", "#00", "/data/crasher"};
    log[0] = log[0] + to_string(g_unsignedLoopSysPid);
    int count = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 4) << "DumpCatcherSystemTest048 Failed";
    StopCrasherLoop(APP_CRASHER_CPP);
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest048: end.";
}

NOINLINE int TestFunc6(std::atomic_int* tid, std::atomic_bool* done)
{
    tid->store(gettid());
    while (!done->load()) {
        usleep(100); // 100 : pause for 100 microseconds to avoid excessive CPU resource consumption in the loop.
    }
    return 1;
}

NOINLINE int TestFunc5(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc6(tid, done);
    return val * val + 1;
}

NOINLINE int TestFunc4(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc5(tid, done);
    return val * val + 1;
}

NOINLINE int TestFunc3(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc4(tid, done);
    return val * val + 1;
}

NOINLINE int TestFunc2(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc3(tid, done);
    return val * val + 1;
}

NOINLINE int TestFunc1(std::atomic_int* tid, std::atomic_bool* done)
{
    int val = TestFunc2(tid, done);
    return val * val + 1;
}

/**
 * @tc.name: DumpCatcherSystemTest049
 * @tc.desc: test DumpCatch API: PID(getpid), unsigned TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest,  DumpCatcherSystemTest049, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest049: start.";
    std::atomic_int otherTid;
    std::atomic_bool done(false);
    std::thread th1([&otherTid, &done] {
        otherTid = gettid();
        TestFunc1(&otherTid, &done);
    });
    sleep(1);
    DfxDumpCatcher dumplog;
    string msg = "";
    int32_t pid = getpid();
    bool ret = dumplog.DumpCatch(pid, 0, msg);
    GTEST_LOG_(INFO) << "ret: " << ret;
    GTEST_LOG_(INFO) << "msg:\n" << msg;
    string log[] = {"#00", "test_faultloggerd", "Tid:", "Name", "Tid:"};
    log[2].append(std::to_string(pid));
    log[4].append(std::to_string(otherTid));
    int logSize = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(msg, log, logSize);
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, logSize) << "DumpCatcherSystemTest049 Failed";
    done.store(true);
    th1.join();
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest049: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest050
 * @tc.desc: test dumpcatcher command for system sandbox process
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest,  DumpCatcherSystemTest050, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest050: start.";
    string getPidCMD = "pidof netmanager";
    string pid = ExecuteCommands(getPidCMD);
    GTEST_LOG_(INFO) << "The pid of netmanager process: " << pid;
    string procCMD = "dumpcatcher -p " + pid;
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string keyword = "#00 pc";
    int numOfTid = GetKeywordCount(procDumpLog, keyword);
    // The target process contains multiple threads,
    // so the result of dumpcatcher should contain more than one keyword "#00 pc"
    EXPECT_GT(numOfTid, 1) << "DumpCatcherSystemTest050 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest050: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest051
 * @tc.desc: test dumpcatcher command for chipset sandbox process
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest,  DumpCatcherSystemTest051, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest051: start.";
    string getPidCMD = "pidof wifi_host";
    string pid = ExecuteCommands(getPidCMD);
    GTEST_LOG_(INFO) << "The pid of wifi_host process: " << pid;
    string procCMD = "dumpcatcher -p " + pid;
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string keyword = "#00 pc";
    int numOfTid = GetKeywordCount(procDumpLog, keyword);
    // The target process contains multiple threads,
    // so the result of dumpcatcher should contain more than one keyword "#00 pc"
    EXPECT_GT(numOfTid, 1) << "DumpCatcherSystemTest051 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest051: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest052
 * @tc.desc: test dumpcatcher command for appdata sandbox process
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest,  DumpCatcherSystemTest052, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest052: start.";
    string getPidCMD = "pidof com.ohos.medialibrary.medialibrarydata";
    string pid = ExecuteCommands(getPidCMD);
    GTEST_LOG_(INFO) << "The pid of com.ohos.medialibrary.medialibrarydata process: " << pid;
    string procCMD = "dumpcatcher -p " + pid;
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string keyword = "#00 pc";
    int numOfTid = GetKeywordCount(procDumpLog, keyword);
    // The target process contains multiple threads,
    // so the result of dumpcatcher should contain more than one keyword "#00 pc"
    EXPECT_GT(numOfTid, 1) << "DumpCatcherSystemTest052 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest052: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest101
 * @tc.desc: test using dumpcatcher command tools to dump the signal stop process
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest101, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest101: start uid:" << getuid();
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "DumpCatcherSystemTest101: Failed to fork a test process";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
    }
    kill(pid, SIGSTOP);
    string procCMD = "dumpcatcher -p " + to_string(pid);
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    std::vector<std::string> matchWords = { "status:", "Name:", "nonvoluntary_ctxt_switches:" };
    if (IsLinuxKernel()) {
        matchWords.emplace_back("wchan:");
        matchWords.emplace_back("Tid:");
    }
    int matchCount = static_cast<int>(matchWords.size());
    int count = GetKeywordsNum(procDumpLog, matchWords.data(), matchCount);
    kill(pid, SIGKILL);
    EXPECT_EQ(count, matchCount) << "DumpCatcherSystemTest101 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest101: end.";
}

/**
 * @tc.name: DumpCatcherSystemTest102
 * @tc.desc: test calling dumpcatcher interfaces to dump the signal stop process
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest102, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest102: start uid:" << getuid();
    pid_t pid = fork();
    if (pid < 0) {
        FAIL() << "DumpCatcherSystemTest102: Failed to fork a test process";
    } else if (pid == 0) {
        sleep(3); // 3 : sleep 3 seconds
    }
    kill(pid, SIGSTOP);
    DfxDumpCatcher dumplog;
    string msg = "";
    if (!dumplog.DumpCatch(pid, 0, msg)) {
        GTEST_LOG_(ERROR) << "DumpCatcherSystemTest102: Failed to dump target process.";
    }
    GTEST_LOG_(INFO) << msg;

    std::vector<std::string> matchWords = { "timeout", "status:", "Name:", "nonvoluntary_ctxt_switches:" };
    if (IsLinuxKernel()) {
        matchWords.emplace_back("wchan:");
        matchWords.emplace_back("Tid:");
    }
    int matchCount = static_cast<int>(matchWords.size());
    int count = GetKeywordsNum(msg, matchWords.data(), matchCount);
    kill(pid, SIGKILL);
    EXPECT_EQ(count, matchCount) << "DumpCatcherSystemTest102 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest102: end.";
}

static void TestDumpCatch(const int targetPid, const string& processName, const int threadIdx)
{
    DfxDumpCatcher dumplog;
    string msg = "";
    if (dumplog.DumpCatch(targetPid, 0, msg)) {
        GTEST_LOG_(INFO) << "threadIdx(" << threadIdx << ") dump sucessfully.";
        string log[] = {"Pid:" + to_string(targetPid), "Tid:" + to_string(targetPid), "Name:" + processName,
                        "#00", "#01", "#02"};
        int expectNum = sizeof(log) / sizeof(log[0]);
        int cnt = GetKeywordsNum(msg, log, sizeof(log) / sizeof(log[0]));
        EXPECT_EQ(cnt, expectNum) << "Check stack trace key words failed.";
        if (cnt == expectNum) {
            g_checkCnt++;
        }
    } else {
        GTEST_LOG_(INFO) << "threadIdx(" << threadIdx << ") dump failed.";
        if (msg.find("Result: pid(" + to_string(targetPid) + ") process is dumping.") == string::npos) {
            GTEST_LOG_(ERROR) << "threadIdx(" << threadIdx << ") dump error message is unexpectly.";
            FAIL();
        }
    }
}

/**
* @tc.name: DumpCatcherSystemTest201
* @tc.desc: Calling DumpCatch Func for same process in multiple threads at same time
* @tc.type: FUNC
*/
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest201, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest201: start.";
    int accountmgrPid = GetProcessPid(POWERMGR_NAME);
    setuid(ROOT_UID);
    g_checkCnt = 0;
    for (int threadIdx = 0; threadIdx < MULTITHREAD_TEST_COUNT; threadIdx++) {
        thread(TestDumpCatch, accountmgrPid, POWERMGR_NAME, threadIdx).detach();
    }
    sleep(2); // 2 : sleep 2 seconds
    EXPECT_GT(g_checkCnt, 0) << "DumpCatcherSystemTest201 failed";
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest201: end.";
}

/**
* @tc.name: DumpCatcherSystemTest202
* @tc.desc: Calling DumpCatch Func for different process in multiple threads at same time
* @tc.type: FUNC
*/
HWTEST_F(DumpCatcherSystemTest, DumpCatcherSystemTest202, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest202: start.";
    vector<string> testProcessNameVecs = {POWERMGR_NAME, FOUNDATION_NAME, APPSPAWN_NAME};
    vector<int> testPidVecs;
    for (auto processName : testProcessNameVecs) {
        testPidVecs.emplace_back(GetProcessPid(processName));
    }
    g_checkCnt = 0;
    auto testProcessListSize = testProcessNameVecs.size();
    for (auto idx = 0; idx < testProcessListSize; idx++) {
        thread(TestDumpCatch, testPidVecs[idx], testProcessNameVecs[idx], idx).detach();
    }
    sleep(2); // 2 : sleep 2 seconds
    EXPECT_EQ(g_checkCnt, 3) << "DumpCatcherSystemTest202 failed";
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest202: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
