/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include <cerrno>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <securec.h>
#include <string>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "faultloggerd_client.h"
#include "dfx_test_util.h"
#include "directory_ex.h"
#include "dfx_define.h"
#include "dfx_util.h"
#include "procinfo.h"

using namespace testing::ext;
using namespace std;

#define NSPID_PATH "/data/nspid"

namespace OHOS {
namespace HiviewDFX {
class FaultLoggerdSystemTest : public testing::Test {
public:
    static void SetUpTestCase(void);
};

void FaultLoggerdSystemTest::SetUpTestCase(void)
{
    chmod("/data/crasher_c", 0755); // 0755 : -rwxr-xr-x
    chmod("/data/crasher_cpp", 0755); // 0755 : -rwxr-xr-x
}

namespace {
static const int CPPCRASH_FILENAME_MIN_LENGTH = 36; // 36 : length of /data/log/faultlog/temp/cppcrash-x-x
static const int SIGNAL_TEST_NUM = 40;
}

static pid_t ForkAndExecuteCrasher(const string& option, const CrasherType type)
{
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Fork failed";
        return pid;
    } else if (pid == 0) {
        if (type == CRASHER_C) {
            execl("/data/crasher_c", "crasher_c", option.c_str(), nullptr);
        } else {
            execl("/data/crasher_cpp", "crasher_cpp", option.c_str(), nullptr);
        }
    }

    int status = 0;
    waitpid(pid, &status, WNOHANG);
    GTEST_LOG_(INFO) << "forked pid:" << pid;
    return pid;
}

static pid_t TriggerCrasherAndGetFileName(const string& option, const CrasherType type, string& crashFileName,
                                          int waitSec = 1, const std::string& tempPath = TEMP_DIR)
{
    auto pid = ForkAndExecuteCrasher(option, type);
    int recheckCount = 0;

    crashFileName = WaitCreateCrashFile("cppcrash", pid);
    if (!crashFileName.empty()) {
        return pid;
    }
    // 6: means recheck times
    while (recheckCount < 6) {
        crashFileName = GetCppCrashFileName(pid, tempPath);
        if (crashFileName.size() > 0) {
            GTEST_LOG_(INFO) << "get crash file:" << crashFileName;
            break;
        }
        GTEST_LOG_(INFO) << "recheck crash file, pid" << pid;
        recheckCount++;
        sleep(waitSec);
    }
    return pid;
}

static bool CheckCountNum(const string& filePath, const pid_t& pid, const string& option)
{
    map<string, string> optionReasonMap = {
#if defined(__LP64__)
        { string("triSIGTRAP"), string("SIGILL") },
#else
        { string("triSIGTRAP"), string("SIGTRAP") },
#endif
        { string("triSIGILL"), string("SIGILL") },
        { string("triSIGSEGV"), string("SIGSEGV") },
        { string("MaxStack"), string("SIGSEGV") },
        { string("MaxMethod"), string("SIGSEGV") },
        { string("STACKOF"), string("SIGSEGV") },
        { string("OOM"), string("SIGABRT") },
    };
    string reason = option;
    auto iter = optionReasonMap.find(option);
    if (iter != optionReasonMap.end()) {
        GTEST_LOG_(INFO) << "optionReasonMap matched";
        reason = iter->second;
    }
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", reason, "Tid:", "#00", "Registers:", REGISTERS, "FaultStack:",
        "Maps:", "/crasher"
    };
    int minRegIdx = 6; // 6 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckCountNumAbort(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGABRT", "LastFatalMessage:", "ABORT!", "Tid:", "#00",
        "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    int minRegIdx = 8; // 8 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}


static bool CheckCountNumNullpointer(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "NULL", "pointer", "dereference", "Tid:", "#00",
        "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    int minRegIdx = 9; // 7 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckCountNumStackOverFlow(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "stack-buffer-overflow", "Tid:", "#00",
        "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    int minRegIdx = 7; // 7 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckCountNumPCZero(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "Tid:", "#00", "Registers:", REGISTERS, "FaultStack:",
        "Maps:", "/crasher"
    };
    int minRegIdx = 6; // 6 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckCountNumOverStack(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "Tid:", "#56", "Registers:", REGISTERS, "FaultStack:",
        "Maps:", "/crasher"
    };
    int minRegIdx = 6; // 6 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckCountNumMultiThread(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "Tid:", "#00",
        "Registers:", REGISTERS, "FaultStack:", "Maps:",
        "/crasher"
    };
    int minRegIdx = 6; // 6 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static string GetStackTop(void)
{
    ifstream spFile;
    spFile.open("/data/sp");
    string sp;
    spFile >> sp;
    spFile.close();
    int ret = remove("/data/sp");
    if (ret != 0) {
        printf("remove failed!");
    }
    int leftZero = REGISTER_FORMAT_LENGTH - sp.length();
    while (leftZero > 0) {
        sp = "0" + sp;
        leftZero--;
    }
    GTEST_LOG_(INFO) << "sp:" << sp;
    return sp;
}

static void WriteRealPid(int realPid)
{
    ofstream file;
    file.open(NSPID_PATH);
    file << std::to_string(realPid);
    file.close();
}

static int ReadRealPid(void)
{
    ifstream file;
    file.open(NSPID_PATH);
    string pid;
    file >> pid;
    file.close();
    int ret = remove(NSPID_PATH);
    if (ret != 0) {
        printf("remove failed!");
    }
    int realPid = atoi(pid.c_str());
    GTEST_LOG_(INFO) << "real pid:" << realPid;
    return realPid;
}

static bool CheckCountNumStackTop(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "Tid:", "#00", "Registers:", REGISTERS, "FaultStack:",
        "Maps:", "/crasher"
    };
    string sp = GetStackTop();
    for (auto& keyword : log) {
        if (keyword == "sp:") {
            keyword += sp;
        }
    }
    int minRegIdx = 6; // 6 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckCppCrashAllLabelKeywords(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Timestamp:", "Pid:" + to_string(pid), "Uid:", "Process", "Reason:", "LastFatalMessage:", "Fault", "thread",
        "info:", "Tid:", "#00", "Registers:", REGISTERS, "Memory", "near", "registers:", "FaultStack:", "Maps:",
        "/crasher"
    };
    int minRegIdx = 11; // 11 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

#if defined(__aarch64__)
static bool CheckCppCrashAsyncStackEnableKeywords(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Timestamp:", "Pid:" + to_string(pid), "Uid:", "Process", "Reason:", "Fault", "thread", "info:",
        "Tid:", "#00", "SubmitterStacktrace", "Registers:", REGISTERS, "Memory", "near", "registers:",
        "FaultStack:", "Maps:", "/crasher"
    };
    int minRegIdx = 11; // 11 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckCppCrashAsyncStackDisableKeywords(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Timestamp:", "Pid:" + to_string(pid), "Uid:", "Process", "Reason:", "Fault", "thread", "info:",
        "Tid:", "#00", "Registers:", REGISTERS, "Memory", "near", "registers:",
        "FaultStack:", "Maps:", "/crasher"
    };
    int minRegIdx = 10; // 10 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    if (CheckKeyWords(filePath, log, expectNum, minRegIdx) != expectNum) {
        return false;
    }
    string key[] = {
        "SubmitterStacktrace"
    };
    return CheckKeyWords(filePath, key, 1, -1) == 0;
}

static bool CheckTestGetCrashObj(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "LastFatalMessage:", "crashObject.",
        "Tid:", "#00", "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    int minRegIdx = 8; // 8 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

static bool CheckTestGetCrashObjMemory(const string& filePath, const pid_t& pid)
{
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "SIGSEGV", "Tid:", "#00", "Registers:",
        REGISTERS, "ExtraCrashInfo(Memory", "FaultStack:", "Maps:", "/crasher"
    };
    int minRegIdx = 6; // 6 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}
#endif

static void RequestMemory(char* msg)
{
    const size_t count = 5;
    for (size_t i = 0; i < count; i++) {
        usleep(2000); // 2000 : sleep 2ms
        const int32_t initAllocSz = 11;
        void* p = malloc(initAllocSz);
        int ret = memcpy_s(p, initAllocSz, msg, initAllocSz - 1);
        if (ret < 0) {
            ASSERT_GT(ret, 0);
        }
        free(p);
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest001
 * @tc.desc: test C crasher application: SIGFPE
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest001: start.";
    string cmd = "SIGFPE";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "ProcessDfxRequestTest001 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest001: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest002
 * @tc.desc: test CPP crasher application: SIGFPE
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest002: start.";
    string cmd = "SIGFPE";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest002 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest002: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest003
 * @tc.desc: test C crasher application: SIGILL
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest003: start.";
    string cmd = "SIGILL";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest003 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest003: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest004
 * @tc.desc: test CPP crasher application: SIGILL
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest004: start.";
    string cmd = "SIGILL";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest004 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest004: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest005
* @tc.desc: test C crasher application: triSIGILL
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest005: start.";
    string cmd = "triSIGILL";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest005 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest005: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest006
* @tc.desc: test CPP crasher application: triSIGILL
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest006: start.";
    string cmd = "triSIGILL";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest006 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest006: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest007
 * @tc.desc: test C crasher application: SIGSEGV
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest007: start.";
    string cmd = "SIGSEGV";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest007 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest007: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest008
 * @tc.desc: test CPP crasher application: SIGSEGV
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest008: start.";
    string cmd = "SIGSEGV";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest008 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest008: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest009
* @tc.desc: test C crasher application: triSIGSEGV
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest009: start.";
    string cmd = "triSIGSEGV";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest009 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest009: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest010
* @tc.desc: test CPP crasher application: triSIGSEGV
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest010: start.";
    string cmd = "triSIGSEGV";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest010 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest010: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest011
 * @tc.desc: test C crasher application: SIGTRAP
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest011: start.";
    string cmd = "SIGTRAP";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest011 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest011: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest012
 * @tc.desc: test CPP crasher application: SIGTRAP
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest012: start.";
    string cmd = "SIGTRAP";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest012 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest012: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest013
* @tc.desc: test C crasher application: triSIGTRAP
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest013: start.";
    string cmd = "triSIGTRAP";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest013 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest013: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest014
* @tc.desc: test CPP crasher application: triSIGTRAP
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest014: start.";
    string cmd = "triSIGTRAP";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest014 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest014: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest015
 * @tc.desc: test C crasher application: SIGABRT
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest015: start.";
    string cmd = "SIGABRT";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumAbort(fileName, pid)) << "FaultLoggerdSystemTest015 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest015: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest016
 * @tc.desc: test CPP crasher application: SIGABRT
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest016: start.";
    string cmd = "SIGABRT";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumAbort(fileName, pid)) << "FaultLoggerdSystemTest016 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest016: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest017
* @tc.desc: test C crasher application: triSIGABRT
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest017: start.";
    string cmd = "triSIGABRT";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumAbort(fileName, pid)) << "FaultLoggerdSystemTest017 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest017: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest018
* @tc.desc: test CPP crasher application: triSIGABRT
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest018: start.";
    string cmd = "triSIGABRT";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumAbort(fileName, pid)) << "FaultLoggerdSystemTest018 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest018: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest019
* @tc.desc: test C crasher application: SIGBUS
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest019: start.";
    string cmd = "SIGBUS";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest019 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest019: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest020
* @tc.desc: test CPP crasher application: SIGBUS
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest020: start.";
    string cmd = "SIGBUS";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest020 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest020: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest021
* @tc.desc: test C crasher application: MaxStack
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest021: start.";
    string cmd = "MaxStack";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest021 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest021: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest022
* @tc.desc: test CPPcrasher application: MaxStack
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest022: start.";
    string cmd = "MaxStack";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest022 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest022: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest023
* @tc.desc: test C crasher application: MaxMethod
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest023: start.";
    string cmd = "MaxMethod";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest023 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest023: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest024
* @tc.desc: test CPP crasher application: MaxMethod
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest024: start.";
    string cmd = "MaxMethod";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest024 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest024: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest025
* @tc.desc: test C crasher application: STACKOF
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest025, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest025: start.";
    string cmd = "STACKOF";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest025 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest025: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest026
* @tc.desc: test CPP crasher application: STACKOF
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest026, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest026: start.";
    string cmd = "STACKOF";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest026 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest026: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest027
 * @tc.desc: test CPP crasher application: OOM
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest027, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest027: start.";
    string cmd = "OOM";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest027 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest027: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest028
 * @tc.desc: test C crasher application: OOM
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest028, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest028: start.";
    string cmd = "OOM";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest028 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest028: end.";
}
/**
 * @tc.name: FaultLoggerdSystemTest029
 * @tc.desc: test CPP crasher application: PCZero
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest029, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest029: start.";
    string cmd = "PCZero";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumPCZero(fileName, pid)) << "FaultLoggerdSystemTest029 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest029: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest030
 * @tc.desc: test C crasher application: PCZero
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest030, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest030: start.";
    string cmd = "PCZero";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumPCZero(fileName, pid)) << "FaultLoggerdSystemTest030 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest030: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest031
 * @tc.desc: test C crasher application: MTCrash
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest031, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest031: start.";
    string cmd = "MTCrash";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName, 2); // 2 : sleep 2s for waiting cppcrash file
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumMultiThread(fileName, pid)) << "FaultLoggerdSystemTest031 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest031: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest032
 * @tc.desc: test CPP crasher application: MTCrash
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest032, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest032: start.";
    string cmd = "MTCrash";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName, 2); // 2 : sleep 2s for waiting cppcrash file
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumMultiThread(fileName, pid)) << "FaultLoggerdSystemTest032 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest032: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest033
 * @tc.desc: test CPP crasher application: StackOver64
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest033, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest033: start.";
    string cmd = "StackOver64";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumOverStack(fileName, pid)) << "FaultLoggerdSystemTest033 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest033: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest034
 * @tc.desc: test C crasher application: StackOver64
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest034, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest034: start.";
    string cmd = "StackOver64";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumOverStack(fileName, pid)) << "FaultLoggerdSystemTest034 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest034: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest035
 * @tc.desc: test C crasher application: StackTop
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest035, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest035: start.";
    string cmd = "StackTop";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumStackTop(fileName, pid)) << "FaultLoggerdSystemTest035 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest035: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest036
 * @tc.desc: test CPP crasher application: StackTop
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest036, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest036: start.";
    string cmd = "StackTop";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumStackTop(fileName, pid)) << "FaultLoggerdSystemTest036 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest036: end.";
}

void GenerateCrashLogFiles()
{
    for (int i = 0; i < SIGNAL_TEST_NUM; i++) {
        system("/data/crasher_c CrashTest &");
    }
    sleep(10); // 10 : sleep for 10 seconds
}

/**
 * @tc.name: FaultLoggerdSystemTest101
 * @tc.desc: test C crasher application: 40 Abnormal signal
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest101, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0009: start.";
    string clearTempFilesCmd = "rm -rf /data/log/faultlog/temp/*";
    system(clearTempFilesCmd.c_str());
    GenerateCrashLogFiles();
    vector<string> files;
    OHOS::GetDirFiles("/data/log/faultlog/temp/", files);
    GTEST_LOG_(INFO) << files.size();
    EXPECT_EQ(files.size(), SIGNAL_TEST_NUM) << "FaultLoggerdSystemTest101 Failed";
}

static void CrashInChildThread()
{
    GTEST_LOG_(INFO) << "CrashInChildThread(): TID = " << gettid();
    raise(SIGSEGV);
}

static int RunInNewPidNs(void* arg)
{
    (void)arg;
    int pid = fork();
    if (pid == 0) {
        struct ProcInfo g_nsProcInfo;
        GetProcStatus(g_nsProcInfo);
        WriteRealPid(g_nsProcInfo.pid);
        GTEST_LOG_(INFO) << "RunInNewPidNs(): real pid = " << g_nsProcInfo.pid;
        GTEST_LOG_(INFO) << "RunInNewPidNs(): PID = " << getpid();
        GTEST_LOG_(INFO) << "RunInNewPidNs(): TID = " << gettid();
        thread childThread(CrashInChildThread);
        childThread.join();
    } else if (pid < 0) {
        GTEST_LOG_(ERROR) << "RunInNewPidNs(): fork fail";
    } else {
        waitpid(pid, nullptr, 0);
        sleep(3); // 3 : 3 second wait for crash dump finish
    }
    _exit(0);
}

/**
 * @tc.name: FaultLoggerdSystemTest102
 * @tc.desc: test crash in process with pid namespace
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest102, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest102: start.";
    const int stackSz = 1024 * 1024 * 1024; // 1M
    void* cloneStack = mmap(nullptr, stackSz,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 1, 0);
    if (cloneStack == nullptr) {
        FAIL();
    }
    cloneStack = static_cast<void *>(static_cast<uint8_t *>(cloneStack) + stackSz - 1);
    int childPid = clone(RunInNewPidNs, cloneStack, SIGCHLD, nullptr);
    bool isSuccess = childPid > 0;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest102: Failed to clone new process. errno:" << errno;
    } else {
        // wait for get realpid, too long will not monitor create crash file
        sleep(1);
        int readPid = ReadRealPid();
        string fileName = WaitCreateCrashFile("cppcrash", readPid);
        EXPECT_NE(0, fileName.size());
        printf("PidNs Crash File:%s\n", fileName.c_str());
        string log[] = {
            "Pid:", "Uid", "SIGSEGV", "Tid:", "#00",
            "Registers:", REGISTERS, "FaultStack:", "Maps:"
        };
        int minRegIdx = 5; // 5 : index of first REGISTERS - 1
        int expectNum = sizeof(log) / sizeof(log[0]);
        int count = CheckKeyWords(fileName, log, expectNum, minRegIdx);
        EXPECT_EQ(count, expectNum);
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest102: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest103
 * @tc.desc: test the aging mechanism of the temp directory
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest103, TestSize.Level2)
{
    string clearTempFilesCmd = "rm -rf /data/log/faultlog/temp/*";
    system(clearTempFilesCmd.c_str());
    system("/data/crasher_c SIGSEGV"); // trigger aging mechanism
    sleep(1); // 1 : sleep for 1 seconds
    vector<string> files;
    OHOS::GetDirFiles("/data/log/faultlog/temp/", files);
    string oldcrash = "";
    if (!files.empty()) {
        oldcrash = files[0];
    }
    GTEST_LOG_(INFO) << oldcrash;
    files.clear();
    for (int i = 0; i < SIGNAL_TEST_NUM; i++) { // 25 : the count of crash file
        system("/data/crasher_c SIGSEGV");
    }
    OHOS::GetDirFiles("/data/log/faultlog/temp/", files);
    for (size_t i = 0; i < files.size(); i++) {
        if (files[i] == oldcrash) {
            FAIL();
        }
    }
    EXPECT_EQ(files.size(), 20) << "FaultLoggerdSystemTest103 Failed";
}

/**
 * @tc.name: FaultLoggerdSystemTest0104
 * @tc.desc: test crash log build-id
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest104, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest104: start.";
    string cmd = "SIGSEGV";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    std::ifstream file;
    file.open(fileName.c_str(), std::ios::in);
    while (!file.eof()) {
        string s;
        file >> s;
        if (s.find("/data/crasher_c") != string::npos) {
            string buildId;
            size_t leftBraceIdx = s.find('(');
            size_t rightBraceIdx = s.find(')');
            if (leftBraceIdx != string::npos && rightBraceIdx != string::npos) {
                buildId = s.substr(leftBraceIdx + 1, rightBraceIdx - leftBraceIdx - 1);
                GTEST_LOG_(INFO) << "build-id = " << buildId;
            }
            EXPECT_FALSE(buildId.empty()) << "FaultLoggerdSystemTest104 Failed";
            break;
        }
    }
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest104: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest105
 * @tc.desc: test C crasher application: SIGABRT, and check all label keywords
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest105, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest105: start.";
    string cmd = "SIGABRT";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAllLabelKeywords(fileName, pid)) << "FaultLoggerdSystemTest105 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest105: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest106
 * @tc.desc: test CPP crasher application: NullPointerDeref0
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest106, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest106: start.";
    string cmd = "NullPointerDeref0";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumNullpointer(fileName, pid)) << "FaultLoggerdSystemTest106 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest106: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest107
 * @tc.desc: test CPP crasher application: STACKOF
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest107, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest107: start.";
    string cmd = "STACKOF";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumStackOverFlow(fileName, pid)) << "FaultLoggerdSystemTest107 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest107: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest108
 * @tc.desc: test Cpp crasher application: StackCorruption, and check all label keywords
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest108, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest108: start.";
    string cmd = "StackCorruption";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAllLabelKeywords(fileName, pid)) << "FaultLoggerdSystemTest108 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest108: end.";
}

#if defined(PROCESSDUMP_MINIDEBUGINFO)
static bool CheckMinidebugSymbols(const string& filePath, const pid_t& pid, const string& option)
{
    map<string, string> optionSymbolMap = {
        { string("triSIGSEGV"), string("SegmentFaultException") },
        { string("triSIGARBT"), string("Abort") }
    };
    string symbol;
    auto iter = optionSymbolMap.find(option);
    if (iter != optionSymbolMap.end()) {
        GTEST_LOG_(INFO) << "optionSymbolMap matched";
        symbol = iter->second;
    }
    string log[] = {
        "Pid:" + to_string(pid), "Uid", ":crasher", "Tid:", "#00",
        symbol, "ParseAndDoCrash", "main", REGISTERS
    };
    int minRegIdx = 7; // 7 : index of first REGISTERS - 1
    int expectNum = sizeof(log) / sizeof(log[0]);
    return CheckKeyWords(filePath, log, expectNum, minRegIdx) == expectNum;
}

/**
 * @tc.name: FaultLoggerdSystemTest109
 * @tc.desc: trigger crasher_c SIGSEGV and check minidebug synbols
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest109, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest109: start.";
    string cmd = "triSIGSEGV";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckMinidebugSymbols(fileName, pid, cmd)) << "FaultLoggerdSystemTest109 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest109: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest110
 * @tc.desc: trigger crasher_cpp SIGSEGV and check minidebug synbols
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest110, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest110: start.";
    string cmd = "triSIGSEGV";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckMinidebugSymbols(fileName, pid, cmd)) << "FaultLoggerdSystemTest110 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest110: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest111
 * @tc.desc: trigger crasher_c SIGABRT and check minidebug synbols
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest111, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest111: start.";
    string cmd = "triSIGABRT";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckMinidebugSymbols(fileName, pid, cmd)) << "FaultLoggerdSystemTest111 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest111: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest112
 * @tc.desc: trigger crasher_cpp SIGABRT and check minidebug synbols
 * @tc.type: FUNC
 */
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest112, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest112: start.";
    string cmd = "triSIGABRT";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckMinidebugSymbols(fileName, pid, cmd)) << "FaultLoggerdSystemTest112 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest112: end.";
}
#endif

/**
* @tc.name: FaultLoggerdSystemTest113
* @tc.desc: test fetch last fatal message from libc
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest113, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest113: start.";
    string cmd = "FatalMessage";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAllLabelKeywords(fileName, pid)) << "FaultLoggerdSystemTest113 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest113: end.";
}

#if defined(__aarch64__)
/**
* @tc.name: FaultLoggerdSystemTest115
* @tc.desc: Test async-stacktrace api enable in ffrt crash case
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest115, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest115: start.";
    string cmd = "CrashInFFRT true";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAsyncStackEnableKeywords(fileName, pid)) << "FaultLoggerdSystemTest115 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest115: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest116
* @tc.desc: Test async-stacktrace api enable in work callback crash case
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest116, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest116: start.";
    string cmd = "CrashInLibuvWork";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAsyncStackEnableKeywords(fileName, pid)) << "FaultLoggerdSystemTest116 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest116: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest117
* @tc.desc: Test async-stacktrace api enable in timer callback crash case
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest117, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest117: start.";
    string cmd = "CrashInLibuvTimer";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAsyncStackEnableKeywords(fileName, pid)) << "FaultLoggerdSystemTest117 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest117: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest118
* @tc.desc: Test async-stacktrace api enalbe in work callback done crash case
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest118, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest118: start.";
    string cmd = "CrashInLibuvWorkDone";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAsyncStackEnableKeywords(fileName, pid)) << "FaultLoggerdSystemTest118 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest118: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest119
* @tc.desc: Test async-stacktrace api disable in ffrt crash case
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest119, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest119: start.";
    string cmd = "CrashInFFRT false";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCppCrashAsyncStackDisableKeywords(fileName, pid)) << "FaultLoggerdSystemTest119 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest119: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest120
* @tc.desc: Test crash log to /log/crash when faultloggerd unstart case
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest120, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest120: start.";
    string crashDir = "/log/crash/";
    bool dirExists = std::filesystem::exists(crashDir);
    if (dirExists) {
        string clearCrashFilesCmd = "rm -rf /log/crash/*";
        system(clearCrashFilesCmd.c_str());
        string stopFaultLoggerd = "service_control stop faultloggerd";
        (void)ExecuteCommands(stopFaultLoggerd);
        string cmd = "SIGABRT";
        string fileName;
        pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName, 1, crashDir);

        string startFaultLoggerd = "service_control start faultloggerd";
        (void)ExecuteCommands(startFaultLoggerd);
        GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
        if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
            GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
            FAIL();
        }
        EXPECT_TRUE(CheckCountNum(fileName, pid, cmd)) << "FaultLoggerdSystemTest120 Failed";
    } else {
        EXPECT_FALSE(dirExists);
    }
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest120: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest121
* @tc.desc: Test get crash object
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest121, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest121: start.";
    string cmd = "TestGetCrashObj";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckTestGetCrashObj(fileName, pid)) << "FaultLoggerdSystemTest121 Failed";

    cmd = "TestGetCrashObjMemory";
    pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckTestGetCrashObjMemory(fileName, pid)) << "FaultLoggerdSystemTest121 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest121: end.";
}
#endif

/**
* @tc.name: FaultLoggerdSystemTest122
* @tc.desc: Test process exit after being killed
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest122, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest122: start.";
    InstallTestHap("/data/FaultloggerdJsTest.hap");
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";
    int pid = LaunchTestHap(testAbiltyName, testBundleName);
    if (pid > 0) {
        kill(pid, SIGABRT);
        sleep(2); // 2 : sleep 2s
        int newPid = GetProcessPid(TEST_BUNDLE_NAME);
        if (pid == newPid) {
            sleep(2);
            newPid = GetProcessPid(TEST_BUNDLE_NAME);
        }
        EXPECT_NE(pid, newPid) << "FaultLoggerdSystemTest122 Failed";
    } else {
        EXPECT_TRUE(pid <= 0) << "FaultLoggerdSystemTest122 Failed";
    }
    StopTestHap(TEST_BUNDLE_NAME);
    UninstallTestHap(TEST_BUNDLE_NAME);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest122: end.";
}

#if defined(__aarch64__)
/**
* @tc.name: FaultLoggerdSystemTest123
* @tc.desc: Test /log/crash files max num when faultloggerd unstart case
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest123, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest123: start.";
    string crashDir = "/log/crash/";
    bool dirExists = std::filesystem::exists(crashDir);
    if (dirExists) {
        string clearCrashFilesCmd = "rm -rf /log/crash/*";
        system(clearCrashFilesCmd.c_str());
        string stopFaultLoggerd = "service_control stop faultloggerd";
        (void)ExecuteCommands(stopFaultLoggerd);

        string cmd = "SIGABRT";
        string fileName;
        pid_t pid = -1;
        int maxFilesNum = 5;
        string startFaultLoggerd = "service_control start faultloggerd";
        for (int i = 0; i < (maxFilesNum + 1); ++i) {
            pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName, 1, crashDir);
            GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
            if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
                GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
                (void)ExecuteCommands(startFaultLoggerd);
                FAIL();
            }
        }
        (void)ExecuteCommands(startFaultLoggerd);
        std::vector<std::string> files;
        ReadDirFiles(crashDir, files);
        EXPECT_TRUE(files.size() <= maxFilesNum) << "FaultLoggerdSystemTest123 Failed";
    } else {
        EXPECT_FALSE(dirExists);
    }
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest123: end.";
}
#endif
/**
* @tc.name: FaultLoggerdSystemTest124
* @tc.desc: Test lock exit after being killed
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest124, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest124: start.";
    const size_t count = 50;
    const size_t threadSize = 10;
    const size_t msgSize = 10;
    char msg[msgSize] = {'a', 'b', 'c', 'd'};
    size_t sleepTime = 5000; // 5000 : sleep 5ms
    const size_t deviation = 50;
    for (size_t i = 0; i < count; i++) {
        GTEST_LOG_(INFO) << i;
        pid_t pid = fork();
        ASSERT_NE(pid, -1);
        if (pid == 0) {
            for (size_t j = 0; j < threadSize; j++) {
                std::thread shmThread(RequestMemory, msg);
                shmThread.detach();
                RequestMemory(msg);
            }
        } else {
            usleep(sleepTime);
            kill(pid, SIGSEGV);
            int status = 0;
            waitpid(pid, &status, 0);
            ASSERT_TRUE(WIFSIGNALED(status));
            int signal = WTERMSIG(status);
            ASSERT_EQ(signal, SIGSEGV);
            sleepTime = sleepTime + deviation;
        }
    }
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest124: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest125
* @tc.desc: Test block process exit debug function
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest125, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest125: start.";
    InstallTestHap("/data/FaultloggerdJsTest.hap");
    string testBundleName = TEST_BUNDLE_NAME;
    string testAbiltyName = testBundleName + ".MainAbility";

    int pid = LaunchTestHap(testAbiltyName, testBundleName);
    if (pid > 0) {
        system("param set faultloggerd.priv.block_crash_process.enabled true");
        kill(pid, SIGABRT);
        int newPid = GetProcessPid(TEST_BUNDLE_NAME);
        system("param set faultloggerd.priv.block_crash_process.enabled false");
        kill(pid, SIGKILL);
        EXPECT_TRUE(pid == newPid) << "FaultLoggerdSystemTest125 Failed";
    } else {
        EXPECT_TRUE(pid <= 0) << "FaultLoggerdSystemTest125 Failed";
    }
    StopTestHap(TEST_BUNDLE_NAME);
    UninstallTestHap(TEST_BUNDLE_NAME);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest125: end.";
}

#if defined(__aarch64__)
/**
* @tc.name: FaultLoggerdSystemTest126
* @tc.desc: Test process fd leak unwind
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest126, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest126: start.";
    pid_t pid = fork();
    if (pid < 0) {
        EXPECT_GT(pid, -1);
    } else if (pid == 0) {
        int fd = 0;
        while (true) {
            fd = open("/dev/null", O_RDONLY);
            if (fd == -1) {
                raise(SIGABRT);
                break;
            }
        }
    } else {
        auto fileName = WaitCreateCrashFile("cppcrash", pid);
        EXPECT_NE(0, fileName.size());
        string log[] = {
            "Pid:", "Uid", "SIGABRT", "Tid:", "#00",
            "Registers:", REGISTERS, "FaultStack:", "Maps:"
        };
        int minRegIdx = 5; // 5 : index of first REGISTERS - 1
        int expectNum = sizeof(log) / sizeof(log[0]);
        int count = CheckKeyWords(fileName, log, expectNum, minRegIdx);
        EXPECT_EQ(count, expectNum);
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest126: end.";
    }
}
#endif

/**
* @tc.name: FaultLoggerdSystemTest127
* @tc.desc: test  crasher application: SetLastFatalMessage
* @tc.type: FUNC
*/
HWTEST_F(FaultLoggerdSystemTest, FaultLoggerdSystemTest127, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest127: start.";
    string cmd = "SetLastFatalMessage";
    string fileName;
    pid_t pid = TriggerCrasherAndGetFileName(cmd, CRASHER_C, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumAbort(fileName, pid)) << "FaultLoggerdSystemTest127 Failed";

    pid = TriggerCrasherAndGetFileName(cmd, CRASHER_CPP, fileName);
    GTEST_LOG_(INFO) << "test pid(" << pid << ")"  << " cppcrash file name : " << fileName;
    if (pid < 0 || fileName.size() < CPPCRASH_FILENAME_MIN_LENGTH) {
        GTEST_LOG_(ERROR) << "Trigger Crash Failed.";
        FAIL();
    }
    EXPECT_TRUE(CheckCountNumAbort(fileName, pid)) << "FaultLoggerdSystemTest127 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest127: end.";
}
} // namespace HiviewDFX
} // namespace OHOS
