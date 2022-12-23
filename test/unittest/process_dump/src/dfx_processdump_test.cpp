/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "dfx_processdump_test.h"

#include <fstream>
#include <map>
#include <csignal>
#include <string>
#include <unistd.h>
#include <vector>

#include "dfx_config.h"
#include "dfx_cutil.h"
#include "dfx_define.h"
#include "dfx_util.h"
#include "directory_ex.h"
#include "multithread_constructor.h"
#include "process_dumper.h"

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

static const int BUF_LEN = 100;

void DfxProcessDumpTest::SetUpTestCase(void)
{
}

void DfxProcessDumpTest::TearDownTestCase(void)
{
}

void DfxProcessDumpTest::SetUp(void)
{
}

void DfxProcessDumpTest::TearDown(void)
{
}

static pid_t CreateMultiThreadProcess(int threadNum)
{
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        (void)MultiThreadConstructor(threadNum);
    }
    return pid;
}

static string GetCppCrashFileName(pid_t pid)
{
    if (pid <= 0) {
        return "";
    }
    vector<string> files;
    OHOS::GetDirFiles("/data/log/faultlog/temp", files);
    string fileNamePrefix = "cppcrash-" + to_string(pid);
    for (const auto& file : files) {
        if (file.find(fileNamePrefix) != string::npos) {
            return file;
        }
    }
    return "";
}

static int CountLines(const string& filename)
{
    ifstream readStream;
    string tempData;
    readStream.open(filename.c_str(), ios::in);
    if (readStream.fail()) {
        return 0;
    } else {
        int n = 0;
        while (getline(readStream, tempData, '\n')) {
            n++;
        }
        readStream.close();
        return n;
    }
}

static int CheckKeyWords(const string& filePath, string *keywords, int length)
{
    ifstream file;
    file.open(filePath.c_str(), ios::in);
    long lines = CountLines(filePath);
    vector<string> t(lines * 4); // 4 : max string blocks of one line
    int i = 0;
    int j = 0;
    string::size_type idx;
    int count = 0;
    int minRegIdx = 6; // 6 : index of REGISTERS
    int maxRegIdx = minRegIdx + REGISTERS_NUM + 1;
    while (!file.eof()) {
        file >> t.at(i);
        idx = t.at(i).find(keywords[j]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << t.at(i);
            if (j > minRegIdx && j < maxRegIdx) {
                if (t.at(i).size() < REGISTERS_LENGTH) {
                    count--;
                }
            }
            count++;
            j++;
            if (j == length) {
                break;
            }
            continue;
        }
        i++;
    }
    file.close();
    GTEST_LOG_(INFO) << count << " keys matched.";
    return count;
}

static bool CheckCppCrashKeyWords(const string& filePath, pid_t pid, int sig)
{
    if (filePath.empty() || pid <= 0) {
        return false;
    }
    map<int, string> sigKey = {
        { SIGILL, string("SIGILL") },
        { SIGTRAP, string("SIGTRAP") },
        { SIGABRT, string("SIGABRT") },
        { SIGBUS, string("SIGBUS") },
        { SIGFPE, string("SIGFPE") },
        { SIGSEGV, string("SIGSEGV") },
        { SIGSTKFLT, string("SIGSTKFLT") },
        { SIGSYS, string("SIGSYS") },
    };
    string sigKeyword = "";
    map<int, string>::iterator iter = sigKey.find(sig);
    if (iter != sigKey.end()) {
        sigKeyword = iter->second;
    }
    string keywords[] = {
        "Pid:" + to_string(pid), "Uid:", "test_processdump", sigKeyword, "Tid:", "#00", "Registers:", REGISTERS,
        "FaultStack:", "Maps:", "test_processdump"
    };
    int length = sizeof(keywords) / sizeof(keywords[0]);
    return CheckKeyWords(filePath, keywords, length) == length;
}

static string GetCmdResultFromPopen(const string& cmd)
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
    string result = "";
    while (!feof(fp)) {
        if (fgets(buffer, bufSize - 1, fp) != nullptr) {
            result += buffer;
        }
    }
    pclose(fp);
    return result;
}

static int GetProcessPid(string applyName)
{
    string procCMD = "pgrep '" + applyName + "'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMD;
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        return -1;
    }
    string applyPid;
    char resultBuf[BUF_LEN] = { 0, };
    while (fgets(resultBuf, sizeof(resultBuf), procFileInfo) != nullptr) {
        applyPid = resultBuf;
        GTEST_LOG_(INFO) << "applyPid: " << applyPid;
    }
    pclose(procFileInfo);
    return atoi(applyPid.c_str());
}

static int GetServicePid(const std::string& serviceName)
{
    string cmd = "pidof " + serviceName;
    string pidStr = GetCmdResultFromPopen(cmd);
    int32_t pid = 0;
    std::stringstream pidStream(pidStr);
    pidStream >> pid;
    printf("the pid of service(%s) is %s \n", serviceName.c_str(), pidStr.c_str());
    return pid;
}

static string ProcessDumpCommands(const std::string cmds)
{
    GTEST_LOG_(INFO) << "threadCMD = " << cmds;
    FILE *procFileInfo = nullptr;
    std::string cmdLog = "";
    procFileInfo = popen(cmds.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        return cmdLog;
    }
    char resultBuf[BUF_LEN] = { 0, };
    while (fgets(resultBuf, sizeof(resultBuf), procFileInfo) != nullptr) {
        cmdLog += resultBuf;
    }
    pclose(procFileInfo);
    return cmdLog;
}

static int LaunchTestHap(const string& abilityName, const string& bundleName)
{
    string launchCmd = "/system/bin/aa start -a " + abilityName + " -b " + bundleName;
    (void)GetCmdResultFromPopen(launchCmd);
    sleep(2); // 2 : sleep 2s
    return GetServicePid(bundleName);
}

/**
 * @tc.name: DfxProcessDumpTest001
 * @tc.desc: test dumpcatcher -p [native process]
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest001: start.";
    int testPid = GetProcessPid("accountmgr");
    string testCommand = "dumpcatcher -p " + to_string(testPid);
    string dumpRes = ProcessDumpCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "accountmgr";
    string::size_type idx;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        idx = dumpRes.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    ASSERT_EQ(count, len);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest001: end.";
}

/**
 * @tc.name: DfxProcessDumpTest002
 * @tc.desc: test dumpcatcher -p -t [native process]
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest002: start.";
    int testPid = GetProcessPid("accountmgr");
    string testCommand = "dumpcatcher -p " + to_string(testPid) + " -t " + to_string(testPid);
    string dumpRes = ProcessDumpCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "accountmgr";
    string::size_type idx;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        idx = dumpRes.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    ASSERT_EQ(count, len);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest002: end.";
}

/**
 * @tc.name: DfxProcessDumpTest003
 * @tc.desc: test dumpcatcher -p [application process]
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest003: start.";
    string testBundleName = "ohos.samples.distributedcalc";
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    string testCommand = "dumpcatcher -p " + to_string(testPid);
    string dumpRes = ProcessDumpCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "ohos.samples.di";
    string::size_type idx;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        idx = dumpRes.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    ASSERT_EQ(count, len);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest003: end.";
}

/**
 * @tc.name: DfxProcessDumpTest004
 * @tc.desc: test dumpcatcher -p -t [application process]
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest004: start.";
    string testBundleName = "ohos.samples.distributedcalc";
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    string testCommand = "dumpcatcher -p " + to_string(testPid) + " -t " + to_string(testPid);
    string dumpRes = ProcessDumpCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "ohos.samples.di";
    string::size_type idx;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        idx = dumpRes.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    ASSERT_EQ(count, len);
    // kill(testPid, SIGKILL);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest004: end.";
}

/**
 * @tc.name: DfxProcessDumpTest005
 * @tc.desc: test SIGILL crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest005: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGILL);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGILL);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest005: end.";
}

/**
 * @tc.name: DfxProcessDumpTest006
 * @tc.desc: test SIGTRAP crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest006: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGTRAP);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGTRAP);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest006: end.";
}

/**
 * @tc.name: DfxProcessDumpTest007
 * @tc.desc: test SIGABRT crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest007: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGABRT);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGABRT);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest007: end.";
}

/**
 * @tc.name: DfxProcessDumpTest008
 * @tc.desc: test SIGBUS crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest008: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGBUS);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGBUS);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest008: end.";
}

/**
 * @tc.name: DfxProcessDumpTest009
 * @tc.desc: test SIGFPE crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest009: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGFPE);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGFPE);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest009: end.";
}

/**
 * @tc.name: DfxProcessDumpTest010
 * @tc.desc: test SIGSEGV crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest010: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGSEGV);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGSEGV);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest010: end.";
}

/**
 * @tc.name: DfxProcessDumpTest011
 * @tc.desc: test SIGSTKFLT crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest011: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGSTKFLT);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGSTKFLT);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest011: end.";
}

/**
 * @tc.name: DfxProcessDumpTest012
 * @tc.desc: test SIGSYS crash
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest012: start.";
    pid_t testProcess = CreateMultiThreadProcess(10); // 10 : create a process with ten threads
    sleep(1);
    kill(testProcess, SIGSYS);
    sleep(3); // 3 : wait 3s to generate cpp crash file
    bool ret = CheckCppCrashKeyWords(GetCppCrashFileName(testProcess), testProcess, SIGSYS);
    ASSERT_TRUE(ret);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest012: end.";
}

/**
 * @tc.name: DfxProcessDumpTest013
 * @tc.desc: test dumpcatcher -p [namespace application process]
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest013: start.";
    string testBundleName = "ohos.samples.clock";
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    if (testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    ProcInfo procinfo;
    GTEST_LOG_(INFO) << "ppid = " << GetProcStatusByPid(testPid, procinfo);
    string testCommand = "dumpcatcher -p " + to_string(testPid);
    string dumpRes = ProcessDumpCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "ohos.samples.cl";
    string::size_type idx;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        idx = dumpRes.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    ASSERT_EQ(count, len);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest013: end.";
}

/**
 * @tc.name: DfxProcessDumpTest014
 * @tc.desc: test dumpcatcher -p -t [namespace application process]
 * @tc.type: FUNC
 */
HWTEST_F(DfxProcessDumpTest, DfxProcessDumpTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxProcessDumpTest014: start.";
    string testBundleName = "ohos.samples.clock";
    string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    if (testPid == 0) {
        GTEST_LOG_(ERROR) << "Failed to launch target hap.";
        return;
    }
    string testCommand = "dumpcatcher -p " + to_string(testPid) + " -t " + to_string(testPid);
    string dumpRes = ProcessDumpCommands(testCommand);
    GTEST_LOG_(INFO) << dumpRes;
    int count = 0;
    string log[] = {"Pid:", "Name:", "#00", "#01", "#02"};
    log[0] = log[0] + to_string(testPid);
    log[1] = log[1] + "ohos.samples.cl";
    string::size_type idx;
    int len = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < len; i++) {
        idx = dumpRes.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    ASSERT_EQ(count, len);
    kill(testPid, SIGKILL);
    GTEST_LOG_(INFO) << "DfxProcessDumpTest014: end.";
}
