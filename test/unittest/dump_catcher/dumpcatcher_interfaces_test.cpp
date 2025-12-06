/*
 * Copyright (c) 2022-2025 Huawei Device Co., Ltd.
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

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "dfx_define.h"
#include "dfx_dump_catcher.h"
#include "dfx_json_formatter.h"
#include "dfx_kernel_stack.h"
#include "dfx_test_util.h"
#include "dfx_util.h"
#include "faultloggerd_client.h"
#include "json/json.h"
#include "kernel_stack_async_collector.h"
#include "proc_util.h"
#include "procinfo.h"

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

void DumpCatcherInterfacesTest::SetUpTestCase()
{
    InstallTestHap("/data/FaultloggerdJsTest.hap");
    std::string testBundleName = TEST_BUNDLE_NAME;
    std::string testAbiltyName = testBundleName + ".MainAbility";
    g_testPid = LaunchTestHap(testAbiltyName, testBundleName);
}

void DumpCatcherInterfacesTest::TearDownTestCase()
{
    StopTestHap(TEST_BUNDLE_NAME);
    UninstallTestHap(TEST_BUNDLE_NAME);
}

void DumpCatcherInterfacesTest::SetUp()
{}

void DumpCatcherInterfacesTest::TearDown()
{}

AT_OPT_NONE static void TestFunRecursive(int recursiveCount)
{
    GTEST_LOG_(INFO) << "Enter TestFunRecursive recursiveCount:" << recursiveCount;
    if (recursiveCount <= 0) {
        GTEST_LOG_(INFO) << "start enter sleep" << gettid();
        sleep(THREAD_ALIVE_TIME);
        GTEST_LOG_(INFO) << "sleep end.";
    } else {
        TestFunRecursive(recursiveCount - 1);
    }
}

static void* CreateRecursiveThread(void *argv)
{
    g_threadId = gettid();
    GTEST_LOG_(INFO) << "create Recursive MultiThread " << gettid();
    TestFunRecursive(266); // 266: set recursive count to 266, used for dumpcatcher get stack info
    GTEST_LOG_(INFO) << "Recursive MultiThread thread sleep end.";
    return nullptr;
}

static int RecursiveMultiThreadConstructor(void)
{
    pthread_t thread;
    pthread_create(&thread, nullptr, CreateRecursiveThread, nullptr);
    pthread_detach(thread);
    usleep(CREATE_THREAD_TIMEOUT);
    return 0;
}

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
    }
}

static std::string RemoveJsonStackSymbol(const std::string& jsonStack)
{
    Json::Reader reader;
    Json::Value threads;
    if (!(reader.parse(jsonStack, threads))) {
        GTEST_LOG_(ERROR) << "Failed to parse json stack info.";
        return "";
    }
    for (uint32_t i = 0; i < threads.size(); ++i) {
        Json::Value& thread = threads[i];
        EXPECT_TRUE(thread["state"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["utime"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["stime"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["priority"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["nice"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread.isMember("frames"));
        EXPECT_TRUE(thread["frames"].isArray());

        Json::Value& frames = thread["frames"];
        for (uint32_t j = 0; j < frames.size(); ++j) {
            std::string frameStr = "";
            if (frames[j].isMember("line")) {
                continue;
            }
            frames[j]["buildId"] = "";
            frames[j]["symbol"] = "";
        }
    }

    Json::FastWriter writer;
    return writer.write(threads);
}

static std::string JsonAsString(const Json::Value& val)
{
    if (val.isConvertibleTo(Json::stringValue)) {
        return val.asString();
    }
    return "";
}

static bool CheckJsonStackSymbol(const std::string& jsonStack)
{
    Json::Reader reader;
    Json::Value threads;
    if (!(reader.parse(jsonStack, threads))) {
        GTEST_LOG_(ERROR) << "Failed to parse json stack info.";
        return false;
    }
    for (uint32_t i = 0; i < threads.size(); ++i) {
        Json::Value& thread = threads[i];
        EXPECT_TRUE(thread["state"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["utime"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["stime"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["priority"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread["nice"].isConvertibleTo(Json::stringValue));
        EXPECT_TRUE(thread.isMember("frames"));
        EXPECT_TRUE(thread["frames"].isArray());

        const Json::Value& frames = thread["frames"];
        for (uint32_t j = 0; j < frames.size(); ++j) {
            std::string frameStr = "";
            if (frames[j].isMember("line")) {
                continue;
            }
            auto buildId = JsonAsString(frames[j]["buildId"]);
            auto funcName = JsonAsString(frames[j]["symbol"]);
            if (!buildId.empty() || !funcName.empty()) {
                return true;
            }
        }
    }

    return false;
}

/**
 * @tc.name: DumpCatcherInterfacesTest001
 * @tc.desc: test DumpCatch API: PID(test hap), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest001: start.";
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
            DfxDumpCatcher dumplog;
            std::string msg = "";
            bool ret = dumplog.DumpCatch(g_testPid, 0, msg);
            GTEST_LOG_(INFO) << ret;
#if defined(__aarch64__)
            string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn", "Name:OS_DfxWatchdog",
                             "at jsFunc", "index_.js"};
#else
            string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn", "Name:OS_DfxWatchdog"};
#endif
            log[0] += std::to_string(g_testPid);
            log[1] += TRUNCATE_TEST_BUNDLE_NAME;
            int len = sizeof(log) / sizeof(log[0]);
            int count = GetKeywordsNum(msg, log, len);
            EXPECT_EQ(count, len) << msg << "DumpCatcherInterfacesTest001 Failed";
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest001: end.";
        }
    }
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
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(powermgr), 0}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest004: start.";
    std::string testProcess = "powermgr";
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
    log[1] = log[1] + "powermgr";
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(msg, log, len);
    EXPECT_EQ(count, len) << msg << "DumpCatcherInterfacesTest004 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest004: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest005
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(powermgr),PID(foundation),PID(systemui)}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest005: start.";
    std::vector<string> testProcessName = { "powermgr", "foundation", "com.ohos.systemui" };
    string matchProcessName[] = { "powermgr", "foundation", "m.ohos.systemui" };
    std::vector<int> multiPid;
    std::vector<string> matchLog;
    int index = 0;
    for (string oneProcessName : testProcessName) {
        int testPid = GetProcessPid(oneProcessName);
        if (testPid == 0) {
            GTEST_LOG_(INFO) << "process:" << oneProcessName << " pid is empty, skip";
            index++;
            continue;
        }
        multiPid.emplace_back(testPid);
        matchLog.emplace_back("Tid:" + std::to_string(testPid));
        matchLog.emplace_back("Name:" + matchProcessName[index]);
        index++;
    }

    // It is recommended that the number of effective pids be greater than 1,
    // otherwise the testing purpose will not be achieved
    EXPECT_GT(multiPid.size(), 1) << "DumpCatcherInterfacesTest005 Failed";

    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << "ret:" << ret;

    int matchLogCount = matchLog.size();
    auto matchLogArray = std::make_unique<string[]>(matchLogCount);
    index = 0;
    for (string info : matchLog) {
        matchLogArray[index] = info;
        index++;
    }
    int count = GetKeywordsNum(msg, matchLogArray.get(), matchLogCount);
    EXPECT_EQ(count, matchLogCount) << msg << "DumpCatcherInterfacesTest005 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest005: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest006
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(powermgr), -11}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest006: start.";
    std::string testProcess = "powermgr";
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
    log[1] = log[1] + "powermgr";
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(msg, log, len);
    EXPECT_EQ(count, len) << msg << "DumpCatcherInterfacesTest006 Failed";
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
    int applyPid = 99999;
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
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(powermgr), 9999}
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest008: start.";
    std::string apply = "powermgr";
    int applyPid1 = GetProcessPid(apply);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = 99999;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    string log[] = { "Tid:", "Name:", "Failed"};
    log[0] = log[0] + std::to_string(applyPid1);
    log[1] = log[1] + apply;
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(msg, log, len);
    EXPECT_EQ(count, len) << msg << "DumpCatcherInterfacesTest008 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest008: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest014
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(powermgr), PID(foundation)}
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest014: start.";
    std::string testProcess1 = "powermgr";
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
    int len = sizeof(log) / sizeof(log[0]);
    int count = GetKeywordsNum(msg, log, len);
    EXPECT_EQ(count, len) << msg << "DumpCatcherInterfacesTest014 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest014: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest015
 * @tc.desc: test DumpCatch API: PID(test hap), TID(test hap main thread)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest015: start.";
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
            DfxDumpCatcher dumplog;
            std::string msg = "";
            bool ret = dumplog.DumpCatch(g_testPid, g_testPid, msg);
            GTEST_LOG_(INFO) << ret;
            string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn"};
            log[0] += std::to_string(g_testPid);
            log[1] += TRUNCATE_TEST_BUNDLE_NAME;
            int len = sizeof(log) / sizeof(log[0]);
            int count = GetKeywordsNum(msg, log, len);
            GTEST_LOG_(INFO) << msg;
            EXPECT_EQ(count, len) << msg << "DumpCatcherInterfacesTest015 Failed";
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest015: end.";
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest016
 * @tc.desc: test DumpCatch API: PID(test hap), TID(-1)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest016: start.";
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
            DfxDumpCatcher dumplog;
            std::string msg = "";
            bool ret = dumplog.DumpCatch(g_testPid, -1, msg);
            GTEST_LOG_(INFO) << ret;
            EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest016 Failed";
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest016: end.";
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest017
 * @tc.desc: test DumpCatch API: PID(-1), TID(-1)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest017: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(-1, -1, msg);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "DumpCatcherInterfacesTest017 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest017: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest018
 * @tc.desc: test DumpCatchFd API: PID(getpid()), TID(gettid())
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
 * @tc.desc: test DumpCatchFd API: PID(getpid()), TID(0)
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
 * @tc.desc: test DumpCatchFd API: PID(getpid()), TID(-1)
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
 * @tc.desc: test DumpCatchFd API: PID(powermgr), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest021: start.";
    std::string apply = "powermgr";
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
 * @tc.desc: test DumpCatchFd API: PID(powermgr), TID(powermgr main thread)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest022: start.";
    std::string apply = "powermgr";
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
 * @tc.desc: test DumpCatchFd API: PID(powermgr), TID(-1)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest023: start.";
    std::string apply = "powermgr";
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
 * @tc.desc: test DumpCatchFd API: PID(powermgr), TID(9999)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest024: start.";
    std::string apply = "powermgr";
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
 * @tc.desc: test DumpCatchFd API: PID(getpid()), TID(9999)
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
 * @tc.desc: test DumpCatchFd API: PID(getpid()), TID(child thread)
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
 * @tc.desc: test DumpCatchFd API: PID(child process), TID(child thread of child process)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest027, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest027: start.";
    ForkMultiThreadProcess();
    std::vector<int> tids;
    std::vector<int> nstids;
    bool isSuccess = GetTidsByPid(g_processId, tids, nstids);
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
    } else {
        int childTid = tids[1]; // 1 : child thread
        GTEST_LOG_(INFO) << "dump remote process, "  << " pid:" << g_processId << ", tid:" << childTid;
        DfxDumpCatcher dumplog;
        std::string msg = "";
        bool ret = dumplog.DumpCatchFd(g_processId, childTid, msg, 1);
        GTEST_LOG_(INFO) << ret;
        EXPECT_TRUE(ret) << "DumpCatcherInterfacesTest027 Failed";
        GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest027: end.";
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest028
 * @tc.desc: test DumpCatchFd API: PID(getpid()), TID(child thread) and config FrameNum
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest028, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest028: start.";
    RecursiveMultiThreadConstructor();
    DfxDumpCatcher dumplog;
    std::string msg = "";
    GTEST_LOG_(INFO) << "dump local process, "  << " tid:" << g_threadId;
    bool ret = dumplog.DumpCatchFd(getpid(), g_threadId, msg, 1, 10); // 10 means backtrace frames is 10
    GTEST_LOG_(INFO) << "message:"  << msg;
    GTEST_LOG_(INFO) << ret;
    EXPECT_TRUE(msg.find("#09") != std::string::npos);
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest028 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest028: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest029
 * @tc.desc: test DumpCatchFd API: PID(getpid()), TID(child thread) and DEFAULT_MAX_FRAME_NUM
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest029, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest029: start.";
    RecursiveMultiThreadConstructor();
    usleep(CREATE_THREAD_TIMEOUT);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    GTEST_LOG_(INFO) << "dump local process, "  << " tid:" << g_threadId;
    bool ret = dumplog.DumpCatchFd(getpid(), g_threadId, msg, 1);
    GTEST_LOG_(INFO) << "message:"  << msg;
    GTEST_LOG_(INFO) << ret;
#if (defined(__aarch64__) || defined(__loongarch_lp64))
    std::string stackKeyword = std::string("#") + std::to_string(DEFAULT_MAX_LOCAL_FRAME_NUM - 1);
#else
    std::string stackKeyword = std::string("#") + std::to_string(DEFAULT_MAX_FRAME_NUM - 1);
#endif
    GTEST_LOG_(INFO) << "stackKeyword:"  << stackKeyword;
    EXPECT_TRUE(msg.find(stackKeyword.c_str()) != std::string::npos);
    EXPECT_EQ(ret, true) << "DumpCatcherInterfacesTest029 Failed";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest029: end.";
}

#ifndef is_ohos_lite
/**
 * @tc.name: DumpCatcherInterfacesTest030
 * @tc.desc: test DumpCatch remote API: PID(getpid()), TID(child thread)
 *     and maxFrameNums(DEFAULT_MAX_FRAME_NUM), isJson(true)
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest030, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest030: start.";
    int fd[2];
    EXPECT_TRUE(CreatePipeFd(fd));
    pid_t pid = fork();
    if (pid == 0) {
        NotifyProcStart(fd);
        std::this_thread::sleep_for(std::chrono::seconds(10));
        _exit(0);
    }
    WaitProcStart(fd);
    GTEST_LOG_(INFO) << "dump remote process, "  << " pid:" << pid << ", tid:" << 0;
    DfxDumpCatcher dumplog;
    DfxJsonFormatter format;
    string msg = "";
    bool ret = dumplog.DumpCatch(pid, 0, msg);
    EXPECT_TRUE(ret) << "DumpCatch remote msg Failed.";
    string jsonMsg = "";
    bool jsonRet = dumplog.DumpCatch(pid, 0, jsonMsg, DEFAULT_MAX_FRAME_NUM, true);
    std::cout << jsonMsg << std::endl;
    EXPECT_TRUE(jsonRet) << "DumpCatch remote json Failed.";
    string stackMsg = "";
    bool formatRet = format.FormatJsonStack(jsonMsg, stackMsg);
    EXPECT_TRUE(formatRet) << "FormatJsonStack Failed.";
    size_t pos = msg.find("#00");
    ASSERT_TRUE(pos != std::string::npos);
    msg = msg.erase(0, pos);
    pos = stackMsg.find("#00");
    ASSERT_TRUE(pos != std::string::npos);
    stackMsg = stackMsg.erase(0, pos);
    EXPECT_EQ(stackMsg == msg, true) << "stackMsg: " << stackMsg << "msg: " << msg << "stackMsg != msg";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest030: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest031
 * @tc.desc: test FormatJsonStack API
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest031, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest031: start.";
    int fd[2];
    EXPECT_TRUE(CreatePipeFd(fd));
    pid_t pid = fork();
    if (pid == 0) {
        NotifyProcStart(fd);
        std::this_thread::sleep_for(std::chrono::seconds(10));
        _exit(0);
    }
    WaitProcStart(fd);
    GTEST_LOG_(INFO) << "dump remote process, "  << " pid:" << pid << ", tid:" << 0;
    DfxDumpCatcher dumplog;
    string msg = "";
    bool ret = dumplog.DumpCatch(pid, 0, msg);
    EXPECT_TRUE(ret) << "DumpCatch remote msg Failed.";
    string jsonMsg = "";
    bool jsonRet = dumplog.DumpCatch(pid, 0, jsonMsg, DEFAULT_MAX_FRAME_NUM, true);
    std::cout << jsonMsg << std::endl;
    EXPECT_TRUE(jsonRet) << "DumpCatch remote json Failed.";

    auto jsonStack = RemoveJsonStackSymbol(jsonMsg);
    EXPECT_TRUE(!jsonStack.empty());
    ASSERT_FALSE(CheckJsonStackSymbol(jsonStack));

    string stackMsg = "";
    DfxJsonFormatter format;
    bool formatRet = format.FormatJsonStack(jsonStack, stackMsg, true, "");
    EXPECT_TRUE(formatRet) << "FormatJsonStack Failed.";
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest031: stackMsg : " << stackMsg;
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest031: end.";
}
#endif

#ifndef is_ohos_lite
/**
 * @tc.name: DumpCatcherInterfacesTest032
 * @tc.desc: test DfxJsonFormatter
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest032, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest032: start.";
    DfxJsonFormatter format;
    string outStackStr = "";
    string errorJsonMsg = "{\"test\"}";
    bool formatRet = format.FormatJsonStack(errorJsonMsg, outStackStr);
    EXPECT_FALSE(formatRet);

    outStackStr = "";
    string noThreadJsonMsg = "[{\"tid\" : \"1\"}]";
    formatRet = format.FormatJsonStack(noThreadJsonMsg, outStackStr);
    EXPECT_TRUE(formatRet);

    outStackStr = "";
    string noTidJsonMsg = "[{\"thread_name\" : \"test\"}]";
    formatRet = format.FormatJsonStack(noTidJsonMsg, outStackStr);
    EXPECT_TRUE(formatRet);

    outStackStr = "";
    string jsJsonMsg = R"~([{"frames":[{"buildId":"", "file":"/system/lib/ld-musl-arm.so.1",
        "offset":0, "pc":"000fdf4c", "symbol":""}, {"line":"1", "file":"/system/lib/ld-musl-arm.so.1",
        "offset":628, "pc":"000ff7f4", "symbol":"__pthread_cond_timedwait_time64"}],
        "thread_name":"OS_SignalHandle", "tid":1608}])~";
    formatRet = format.FormatJsonStack(jsJsonMsg, outStackStr);
    EXPECT_TRUE(formatRet);
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest032: end.";
}
#endif

/**
@tc.name: DumpCatcherInterfacesTest033
@tc.desc: testDump after crashed
@tc.type: FUNC
*/
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest033, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest033: start.";
    int32_t fd = RequestFileDescriptor(FaultLoggerType::CPP_CRASH);
    ASSERT_GT(fd, 0);
    close(fd);
    pid_t pid = fork();
    if (pid == 0) {
        GTEST_LOG_(INFO) << "dump remote process, " << "pid:" << getppid() << ", tid:" << 0;
        DfxDumpCatcher dumplog;
        string msg = "";
        EXPECT_FALSE(dumplog.DumpCatch(getppid(), 0, msg));
        constexpr int validTime = 1;
        sleep(validTime);
        msg = "";
        EXPECT_TRUE(dumplog.DumpCatch(getppid(), 0, msg));
        CheckAndExit(HasFailure());
    } else if (pid < 0) {
        GTEST_LOG_(INFO) << "Fail in fork.";
    } else {
        int status;
        wait(&status);
        ASSERT_EQ(status, 0);
    }
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest033: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest035
 * @tc.desc: test DumpCatchWithTimeout API: PID(test hap)
 * @tc.type: FUNC
 * @tc.require: IB1XY4
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest035, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest035: start.";
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
            DfxDumpCatcher dumplog;
            std::string msg = "";
            auto result = dumplog.DumpCatchWithTimeout(g_testPid, msg);
            GTEST_LOG_(INFO) << result.second;
            EXPECT_TRUE(result.first == 0) << "DumpCatcherInterfacesTest035 Failed";
            string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn", "Name:OS_DfxWatchdog" };
            log[0] += std::to_string(g_testPid);
            log[1] += TRUNCATE_TEST_BUNDLE_NAME;
            int len = sizeof(log) / sizeof(log[0]);
            int count = GetKeywordsNum(msg, log, len);
            EXPECT_EQ(count, len) << "DumpCatcherInterfacesTest035 Failed";
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest035: end.";
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest036
 * @tc.desc: test DumpCatchWithTimeout API: PID(test hap), TIMEOUT(1000)
 * @tc.type: FUNC
 * @tc.require: IB1XY4
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest036, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest036: start.";
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
            DfxDumpCatcher dumplog;
            std::string msg = "";
            int timeout = 1000;
            auto result = dumplog.DumpCatchWithTimeout(g_testPid, msg, timeout);
            GTEST_LOG_(INFO) << result.second;
            EXPECT_TRUE(result.first == -1) << "DumpCatcherInterfacesTest036 Failed";
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest036: end.";
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest037
 * @tc.desc: test DumpCatchWithTimeout API: PID(nonexistent)
 * @tc.type: FUNC
 * @tc.require: IB1XY4
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest037, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest037: start.";
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
            DfxDumpCatcher dumplog;
            std::string msg = "";
            int nonexistPid = 123456;
            auto result = dumplog.DumpCatchWithTimeout(nonexistPid, msg);
            GTEST_LOG_(INFO) << result.second;
            EXPECT_TRUE(result.first == -1) << "DumpCatcherInterfacesTest037 Failed";
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest037: end.";
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest038
 * @tc.desc: test DumpCatchWithTimeout API: PID(test hap), TIMEOUT(2000)
 * @tc.type: FUNC
 * @tc.require: IB1XY4
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest038, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest038: start.";
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
            DfxDumpCatcher dumplog;
            std::string msg = "";
            int timeout = 2000;
            auto result = dumplog.DumpCatchWithTimeout(g_testPid, msg, timeout);
            GTEST_LOG_(INFO) << result.second;
            EXPECT_TRUE(result.first == 0) << "DumpCatcherInterfacesTest038 Failed";
            string log[] = { "Tid:", "Name:", "#00", "/system/bin/appspawn", "Name:OS_DfxWatchdog" };
            log[0] += std::to_string(g_testPid);
            log[1] += TRUNCATE_TEST_BUNDLE_NAME;
            int len = sizeof(log) / sizeof(log[0]);
            int count = GetKeywordsNum(msg, log, len);
            EXPECT_EQ(count, len) << "DumpCatcherInterfacesTest038 Failed";
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest038: end.";
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest039
 * @tc.desc: test DumpCatchWithTimeout API: PID(test hap) and SIGSTOP the process
 * @tc.type: FUNC
 * @tc.require: IB1XY4
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest039, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest039: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
    } else {
        isSuccess = g_testPid != 0;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        } else {
            isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
            if (!isSuccess) {
                ASSERT_FALSE(isSuccess);
                GTEST_LOG_(ERROR) << "Error process comm";
            } else {
                std::string stopProcessCmd = "kill -s SIGSTOP $(pidof com.example.myapplication)";
                ExecuteCommands(stopProcessCmd);
                DfxDumpCatcher dumplog;
                std::string msg = "";
                auto result = dumplog.DumpCatchWithTimeout(g_testPid, msg);
                std::string startProcessCmd = "kill -s SIGCONT $(pidof com.example.myapplication)";
                ExecuteCommands(startProcessCmd);
                GTEST_LOG_(INFO) << result.second;
                ASSERT_TRUE(result.first == 1);
                GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest039: end.";
            }
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest040
 * @tc.desc: test DumpCatchWithTimeout API: PID(test hap) and stop the faultloggerd
 * @tc.type: FUNC
 * @tc.require: IB1XY4
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest040, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest040: start.";
    std::string res = ExecuteCommands("uname");
    bool isSuccess = res.find("Linux") == std::string::npos;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
    } else {
        isSuccess = g_testPid != 0;
        if (!isSuccess) {
            ASSERT_FALSE(isSuccess);
            GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        } else {
            isSuccess = CheckProcessComm(g_testPid, TRUNCATE_TEST_BUNDLE_NAME);
            if (!isSuccess) {
                ASSERT_FALSE(isSuccess);
                GTEST_LOG_(ERROR) << "Error process comm";
            } else {
                DfxDumpCatcher dumplog;
                std::string msg = "";
                std::string stopFaultloggerdCmd = "service_control stop faultloggerd";
                ExecuteCommands(stopFaultloggerdCmd);
                auto result = dumplog.DumpCatchWithTimeout(g_testPid, msg);
                std::string startFaultloggerdCmd = "service_control start faultloggerd";
                ExecuteCommands(startFaultloggerdCmd);
                GTEST_LOG_(INFO) << result.second;
                EXPECT_TRUE(result.first == -1);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest040: end.";
            }
        }
    }
}

/**
 * @tc.name: DumpCatcherInterfacesTest041
 * @tc.desc: test dumpcatch self scenario
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest041, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest041: start.";
    auto sleep = [] {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    };
    for (int i = 0; i < 10; i++) {
        std::thread t(sleep);
        t.detach();
    }
    DfxDumpCatcher dump;
    std::string stack;
    bool result = dump.DumpCatch(getpid(), 0, stack);
    ASSERT_EQ(result, true);

    result = dump.DumpCatch(getpid(), getpid(), stack);
    ASSERT_EQ(result, true);

    std::vector<int> pids;
    result = dump.DumpCatchMultiPid(pids, stack);
    ASSERT_EQ(result, false);
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest041: end.";
}

/**
 * @tc.name: DumpCatcherInterfacesTest042
 * @tc.desc: test dumpcatch FormatKernelStack
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest042, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest042: start.";
    std::string msg = "";
    std::string formattedStack = "";
    ASSERT_FALSE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, false));
    ASSERT_FALSE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, true));
    msg = "Thread info: name=example_name, tid=12345, key=value., key2=value2., key3=value3., pid=0";
#if defined(__aarch64__)
    ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, false));
    ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, true));

    // get kernel stack
    ASSERT_EQ(DfxGetKernelStack(gettid(), msg), 0);

    // not thread stat
    ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, false));
    GTEST_LOG_(INFO) << "formattedStack is not json :\n" << formattedStack;
    ASSERT_FALSE(formattedStack.empty());
    ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, true));
    GTEST_LOG_(INFO) << "formattedStack is json :\n" << formattedStack;
    ASSERT_EQ(formattedStack, std::string("null\n"));

    // get thread stat
    ProcessInfo info;
    ASSERT_TRUE(ParseProcInfo(gettid(), info));
    msg.append(FomatProcessInfoToString(info)).append("\n");
    ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, false));
    GTEST_LOG_(INFO) << "formattedStack is not json :\n" << formattedStack;
    ASSERT_FALSE(formattedStack.empty());
    ASSERT_TRUE(DfxJsonFormatter::FormatKernelStack(msg, formattedStack, true));
    GTEST_LOG_(INFO) << "formattedStack is json :\n" << formattedStack;
    ASSERT_FALSE(formattedStack.empty());
#endif
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest042: end.";
}

#if defined(__aarch64__)
static bool IsPidDir(const std::filesystem::directory_entry& entry)
{
    if (!entry.is_directory()) {
        return false;
    }
    const std::string fileName = entry.path().filename();
    return std::all_of(fileName.begin(), fileName.end(), [](char c) { return std::isdigit(c); });
}

static bool CheckProcessStart(const std::string& procName, pid_t& pid)
{
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!IsPidDir(entry)) {
            continue;
        }
        std::ifstream commFile(entry.path() / "comm");
        if (!commFile) {
            continue;
        }
        std::string processName;
        if (std::getline(commFile, processName)) {
            processName.erase(processName.find_last_not_of("\n") + 1);
            if (processName == procName) {
                pid = std::stoi(entry.path().filename());
                return true;
            }
        }
    }
    return false;
}

static int GetParentPid(int pid)
{
    std::ifstream statFile("/proc/" + std::to_string(pid) + "/status");
    if (!statFile) {
        GTEST_LOG_(ERROR) << "GetParentPid: No process status file" << pid;
        return -1;
    }

    if (!statFile.is_open()) {
        GTEST_LOG_(ERROR) << "GetParentPid: open fail";
        return -1;
    }

    int ppid = -1;
    const size_t compareSize = 5;
    std::string line;
    while (std::getline(statFile, line)) {
        if (line.compare(0, compareSize, "PPid:") == 0) {
            std::istringstream iss(line.substr(compareSize));
            iss >> ppid;
            break;
        }
    }
    return ppid;
}
#endif

#if defined(__aarch64__)
/**
 * @tc.name: DumpCatcherInterfacesTest043
 * @tc.desc: test dumpcatch no parse symbol
 * @tc.type: FUNC
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest043, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest043: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "DumpCatcherInterfacesTest043: fork fail";
    } else if (pid == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // 5 : sleep 5s
        _exit(0);
    }
    const int checkCnt = 500; // 500 : check processdump start in 500ms
    const int waitForkPidStartTime = 50; // 50 : 50ms
    const int hungUpProcessdumpTime = 2880; // 2880 : 2.88s
    int timeStart = static_cast<int>(GetAbsTimeMilliSeconds());
    pid_t pidCheck = fork();
    if (pidCheck < 0) {
        GTEST_LOG_(ERROR) << "DumpCatcherInterfacesTest043: fork fail";
    } else if (pidCheck == 0) {
        for (int i = 0; i < checkCnt; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            pid_t processdumpPid = 0;
            if (!CheckProcessStart("processdump", processdumpPid) ||
                pid != GetParentPid(GetParentPid(processdumpPid))) {
                continue;
            }
            GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest043: processdump " << processdumpPid << " has been forked";
            if (processdumpPid > 0) {
                kill(processdumpPid, SIGSTOP);
                int now = static_cast<int>(GetAbsTimeMilliSeconds());
                int timeWait = hungUpProcessdumpTime - (now - timeStart - waitForkPidStartTime);
                timeWait = timeWait > 0 ? timeWait : 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(timeWait));
                kill(processdumpPid, SIGCONT);
            }
            break;
        }
        _exit(0);
    }
    DfxDumpCatcher dump;
    std::string stack;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitForkPidStartTime));
    dump.DumpCatch(pid, 0, stack);
    GTEST_LOG_(INFO) << stack;
    kill(pid, SIGKILL);
    kill(pidCheck, SIGKILL);
    bool result = (stack.find("no enough time to parse symbol") != std::string::npos) ||
        (stack.find("poll timeout") != std::string::npos);
    ASSERT_TRUE(result);
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest043: end.";
}
#endif

/**
 * @tc.name: DumpCatcherInterfacesTest044
 * @tc.desc: test DumpCatchWithTimeout dump self
 * @tc.type: FUNC
 * @tc.require: ICAOEX
 */
HWTEST_F(DumpCatcherInterfacesTest, DumpCatcherInterfacesTest044, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest044: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    int currentPid = getpid();
    auto result = dumplog.DumpCatchWithTimeout(currentPid, msg);
    GTEST_LOG_(INFO) << result.second;
    EXPECT_TRUE(result.first == 0);
    GTEST_LOG_(INFO) << "DumpCatcherInterfacesTest044: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
