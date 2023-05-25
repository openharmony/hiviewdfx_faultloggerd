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

static const string DEFAULT_PID_MAX = "32768";
static const string DEFAULT_TID_MAX = "8825";

static pid_t g_rootTid[ARRAY_SIZE] = { -1 };
static pid_t g_appTid[ARRAY_SIZE] = { -1 };
static pid_t g_sysTid[ARRAY_SIZE] = { -1 };
static char g_shellBuf[ARRAY_SIZE] = { 0 };

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
    const string path = "/proc/sys/kernel/pid_max";
    ifstream file(path);
    if (!file) {
        GTEST_LOG_(INFO) << "file not exists";
        return DEFAULT_PID_MAX;
    } else {
        string pidMax;
        file >> pidMax;
        return pidMax;
    }
    return DEFAULT_PID_MAX;
}

static string GetTidMax()
{
    const string path = "/proc/sys/kernel/threads-max";
    ifstream file(path);
    if (!file) {
        GTEST_LOG_(ERROR) << "file not exists";
        return DEFAULT_TID_MAX;
    } else {
        string tidMax;
        file >> tidMax;
        return tidMax;
    }
    return DEFAULT_TID_MAX;
}

static bool IsDigit(const string& pid)
{
    bool flag = true;
    for (unsigned int j = 0; j < pid.length() - 1; j++) {
        if (!isdigit(pid[j])) {
            flag = false;
            break;
        }
    }
    return flag;
}

static pid_t ToPid(const string& pidStr)
{
    if (!IsDigit(pidStr)) {
        return -1;
    }
    pid_t pid;
    if (sscanf_s(pidStr.c_str(), "%d", &pid) != 1) {
        GTEST_LOG_(ERROR) << "sscanf_s pid failed";
        return -1;
    }
    return pid;
}

static void StartCrasherLoopForUnsignedPidAndTid(const CrasherType type)
{
    int sysTidCount = 0;
    setuid(BMS_UID);
    if (type == CRASHER_C) {
        system("/data/crasher_c thread-Loop &");
    } else {
        system("/data/crasher_cpp thread-Loop &");
    }
    string procCmd = "pgrep 'crasher'";
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCmd.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    while (fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo) != nullptr) {
        g_unsignedLoopSysPid = ToPid(g_shellBuf);
        GTEST_LOG_(INFO) << "System ID: " << g_unsignedLoopSysPid;
    }
    pclose(procFileInfo);
    if (g_unsignedLoopSysPid == 0) {
        exit(0);
    }

    usleep(5); // 5 : sleep 5us
    procCmd = "ls /proc/" + to_string(g_unsignedLoopSysPid) + "/task";
    FILE *procFileInfo2 = nullptr;
    procFileInfo2 = popen(procCmd.c_str(), "r");
    if (procFileInfo2 == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    while (fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo2) != nullptr) {
        GTEST_LOG_(INFO) << "procFileInfo2 print info = " << g_shellBuf;
        g_sysTid[sysTidCount] = ToPid(g_shellBuf);
        sysTidCount++;
    }
    pclose(procFileInfo2);
}

static string LaunchCrasher(const CrasherType type, const int uid)
{
    setuid(uid);
    if (type == CRASHER_C) {
        system("/data/crasher_c thread-Loop &");
    } else {
        system("/data/crasher_cpp thread-Loop &");
    }

    string procCmd = "pgrep 'crasher'";
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCmd.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    if (uid == ROOT_UID) {
        int i = 0;
        while ((fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo) != nullptr) && (i < 60)) { // 60 : max buf line
            pid_t pid = ToPid(g_shellBuf);
            if (pid < 0) {
                continue;
            }
            if (g_loopSysPid == pid) {
                GTEST_LOG_(INFO) << g_loopSysPid;
            } else {
                if (type == CRASHER_CPP) {
                    g_loopCppPid = pid;
                } else {
                    g_loopRootPid = pid;
                }
            }
            i++;
        }
        GTEST_LOG_(INFO) << "Root ID: " << g_loopRootPid;
    } else if (uid == BMS_UID) {
        while (fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo) != nullptr) {
            g_loopSysPid = ToPid(g_shellBuf);
            GTEST_LOG_(INFO) << "System ID: " << g_loopSysPid;
        }
    } else {
        while (fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo) != nullptr) {
            pid_t pid = ToPid(g_shellBuf);
            if (g_loopSysPid == pid) {
                GTEST_LOG_(INFO) << g_loopSysPid;
            } else if (g_loopRootPid == pid) {
                GTEST_LOG_(INFO) << g_loopRootPid;
            } else {
                g_loopAppPid = pid;
            }
        }
        GTEST_LOG_(INFO) << "APP ID: " << g_loopAppPid;
    }
    pclose(procFileInfo);
    return to_string(g_loopSysPid);
}

static void StartRootCrasherLoop()
{
    setuid(ROOT_UID);
    LaunchCrasher(CRASHER_C, ROOT_UID);
    if (g_loopRootPid == 0) {
        exit(0);
    }

    usleep(5); // 5 : sleep 5us
    string procCmd = "ls /proc/" + to_string(g_loopRootPid) + "/task";
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCmd.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    int rootTidCount = 0;
    while (fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo) != nullptr) {
        GTEST_LOG_(INFO) << "procFileInfo print info = " << g_shellBuf;
        g_rootTid[rootTidCount] = ToPid(g_shellBuf);
        rootTidCount++;
    }
    setuid(OTHER_UID);
    pclose(procFileInfo);
}

static void StartSystemCrasherLoop()
{
    setuid(BMS_UID);
    LaunchCrasher(CRASHER_C, BMS_UID);
    if (g_loopSysPid == 0) {
        exit(0);
    }

    usleep(5); // 5 : sleep 5us
    string procCmd = "ls /proc/" + to_string(g_loopSysPid) + "/task";
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCmd.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    int sysTidCount = 0;
    while (fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo) != nullptr) {
        GTEST_LOG_(INFO) << "procFileInfo print info = " << g_shellBuf;
        g_sysTid[sysTidCount] = ToPid(g_shellBuf);
        sysTidCount++;
    }
    setuid(OTHER_UID);
    pclose(procFileInfo);
}

static void StartCommonCrasherLoop(const CrasherType type)
{
    setuid(OTHER_UID);
    LaunchCrasher(type, OTHER_UID);
    if (g_loopAppPid == 0) {
        exit(0);
    }

    usleep(5); // 5 : sleep 5us
    string procCmd = "ls /proc/" + to_string(g_loopAppPid) + "/task";
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCmd.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    int appTidCount = 0;
    while (fgets(g_shellBuf, sizeof(g_shellBuf), procFileInfo) != nullptr) {
        GTEST_LOG_(INFO) << "procFileInfo print info = " << g_shellBuf;
        g_appTid[appTidCount] = ToPid(g_shellBuf);
        appTidCount++;
    }
    pclose(procFileInfo);
}

static void StartCrasherLoop(const CrasherRunType type)
{
    switch (type) {
        case ROOT:
            StartRootCrasherLoop();
            break;
        case SYSTEM:
            StartSystemCrasherLoop();
            break;
        case APP_CRASHER_C:
            StartCommonCrasherLoop(CRASHER_C);
            break;
        case APP_CRASHER_CPP:
            StartCommonCrasherLoop(CRASHER_CPP);
            break;
        default:
            break;
    }
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
    log[0] = log[0] +to_string(g_loopSysPid) + ", Name:crasher";
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Usage:", "dump the stacktrace"};
    int count = GetKeywordsNum(procDumpLog, log, sizeof(log) / sizeof(log[0]));
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 2) << "DumpCatcherSystemTest030 Failed";
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
    GTEST_LOG_(INFO) << "DumpCatcherSystemTest117: start.";
    StartCrasherLoop(APP_CRASHER_C);
    string procCMD = "dumpcatcher -p 65534 -t 65534";
    string procDumpLog = ExecuteCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    string log[] = {"Failed"};
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
    string log[] = {"Failed"};
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
        if (msg.find("Result: pid(" + to_string(targetPid) + ") is dumping.") == string::npos) {
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
    int accountmgrPid = GetProcessPid(ACCOUNTMGR_NAME);
    g_checkCnt = 0;
    for (int threadIdx = 0; threadIdx < MULTITHREAD_TEST_COUNT; threadIdx++) {
        thread(TestDumpCatch, accountmgrPid, ACCOUNTMGR_NAME, threadIdx).detach();
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
    vector<string> testProcessNameVecs = {ACCOUNTMGR_NAME, FOUNDATION_NAME, APPSPAWN_NAME};
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