/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This file contains faultlog daemon system test cases. */

#include "faultloggerd_system_test.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <securec.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "dfx_dump_catcher.h"
#include "directory_ex.h"
#include "file_ex.h"
#include "syscall.h"

/* This files contains faultlog st test case modules. */

using namespace OHOS::HiviewDFX;
using namespace testing::ext;
using namespace std;

namespace {
static const int BMS_UID = 1000;
static const int OTHER_UID = 10000;
static const int NUMBER_ONE = 1;
static const int NUMBER_TWO = 2;
static const int NUMBER_THREE = 3;
static const int NUMBER_FOUR = 4;
static const int NUMBER_SIXTY = 60;
static const int NUMBER_FIFTY = 50;
static const string DEFAULT_PID_MAX = "32768";
static const string DEFAULT_TID_MAX = "8825";
static mutex g_mutex;
}

void FaultLoggerdSystemTest::SetUpTestCase(void)
{
    chmod("/data/crasher_c", 0755); // 0755 : -rwxr-xr-x
    chmod("/data/crasher_cpp", 0755); // 0755 : -rwxr-xr-x
    chmod("/data/test_faultloggerd_pre", 0755); // 0755 : -rwxr-xr-x
}

void FaultLoggerdSystemTest::TearDownTestCase(void)
{
}

void FaultLoggerdSystemTest::SetUp(void)
{
}

void FaultLoggerdSystemTest::TearDown(void)
{
}

void FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(int type)
{
    int rootuid = 0;
    int sysuid = BMS_UID;
    if (type == NUMBER_ONE) {
        setuid(sysuid);
        system(("kill -9 " + std::to_string(FaultLoggerdSystemTest::loopSysPid)).c_str());
    } else if (type == NUMBER_TWO) {
        setuid(rootuid);
        system(("kill -9 " + std::to_string(FaultLoggerdSystemTest::loopRootPid)).c_str());
    } else if (type == NUMBER_THREE) {
        setuid(rootuid);
        system(("kill -9 " + std::to_string(FaultLoggerdSystemTest::loopAppPid)).c_str());
    } else {
        setuid(sysuid);
        system(("kill -9 " + std::to_string(FaultLoggerdSystemTest::unsigLoopSysPid)).c_str());
    }
}

std::string FaultLoggerdSystemTest::rootTid[ARRAY_SIZE_HUNDRED] = { "", };
std::string FaultLoggerdSystemTest::appTid[ARRAY_SIZE_HUNDRED] = { "", };
std::string FaultLoggerdSystemTest::sysTid[ARRAY_SIZE_HUNDRED] = { "", };
std::string FaultLoggerdSystemTest::testTid[ARRAY_SIZE_HUNDRED] = { "", };
int FaultLoggerdSystemTest::loopSysPid = 0;
int FaultLoggerdSystemTest::loopRootPid = 0;
int FaultLoggerdSystemTest::loopCppPid = 0;
int FaultLoggerdSystemTest::loopAppPid = 0;
int FaultLoggerdSystemTest::count = 0;
char FaultLoggerdSystemTest::resultBufShell[ARRAY_SIZE_HUNDRED] = { 0, };
unsigned int FaultLoggerdSystemTest::unsigLoopSysPid = 0;

std::string FaultLoggerdSystemTest::GetPidMax()
{
    const string path = "/proc/sys/kernel/pid_max";
    std::ifstream file(path);
    if (!file) {
        GTEST_LOG_(INFO) << "file not exists";
        return DEFAULT_PID_MAX;
    } else {
        std::string pidMax;
        file >> pidMax;
        return pidMax;
    }
    return DEFAULT_PID_MAX;
}

std::string FaultLoggerdSystemTest::GetTidMax()
{
    const string path = "/proc/sys/kernel/threads-max";
    std::ifstream file(path);
    if (!file) {
        GTEST_LOG_(INFO) << "file not exists";
        return DEFAULT_TID_MAX;
    } else {
        std::string tidMax;
        file >> tidMax;
        return tidMax;
    }
    return DEFAULT_TID_MAX;
}

std::string FaultLoggerdSystemTest::ProcessDumpCommands(const std::string cmds)
{
    GTEST_LOG_(INFO) << "threadCMD = " << cmds;
    FILE *procFileInfo = nullptr;
    std::string cmdLog = "";
    procFileInfo = popen(cmds.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        return cmdLog;
    }
    while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
        cmdLog = cmdLog + resultBufShell;
    }
    pclose(procFileInfo);
    return cmdLog;
}

// commandStatus: 0 C    1 CPP
std::string FaultLoggerdSystemTest::ForkAndRunCommands(const std::vector<std::string>& cmds, int commandStatus)
{
    pid_t pid = fork();
    if (pid < 0) {
        return "";
    } else if (pid == 0) {
        std::vector<char *> argv;
        for (const auto &arg : cmds) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(0);
        printf("ForkAndRunCommands %d \n", getpid());
        if (commandStatus == 0) {
            execv("/data/crasher_c", &argv[0]);
        } else {
            execv("/data/crasher_cpp", &argv[0]);
        }
    }

    constexpr time_t maxWaitingTime = NUMBER_SIXTY; // 60 seconds
    time_t remainedTime = maxWaitingTime;
    while (remainedTime > 0) {
        time_t startTime = time(nullptr);
        int status = 0;
        waitpid(pid, &status, WNOHANG);
        if (WIFEXITED(status)) {
            break;
        }
        int sleepSecond = 1;
        sleep(sleepSecond);
        time_t duration = time(nullptr) - startTime;
        remainedTime = (remainedTime > duration) ? (remainedTime - duration) : 0;
    }
    printf("forked pid:%d \n", pid);
    return std::to_string(pid);
}

int FaultLoggerdSystemTest::CountLines(std::string FileName)
{
    ifstream ReadFile;
    std::string tmpuseValue;
    ReadFile.open(FileName.c_str(), ios::in);
    if (ReadFile.fail()) {
        return 0;
    } else {
        int n = 0;
        while (getline(ReadFile, tmpuseValue, '\n')) {
            n++;
        }
        ReadFile.close();
        return n;
    }
}

bool FaultLoggerdSystemTest::IsDigit(std::string pid)
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

static std::string GetLogFileName(const std::string& prefix)
{
    std::string filePath;
    std::vector<std::string> files;
    OHOS::GetDirFiles("/data/log/faultlog/temp/", files);
    for (const auto& file : files) {
        if (file.find(prefix) != std::string::npos) {
            filePath = file;
        }
    }
    return filePath;
}

// commandStatus: 0 C    1 CPP
std::string FaultLoggerdSystemTest::GetfileNamePrefix(std::string ErrorCMD, int commandStatus)
{
    std::vector<std::string> cmds {"crasher_c", ErrorCMD};
    std::string pid = FaultLoggerdSystemTest::ForkAndRunCommands(cmds, commandStatus);
    int sleepSecond = 5;
    sleep(sleepSecond);
    std::string prefix = "cppcrash-" + pid;
    std::string filePath = GetLogFileName(prefix);
    return filePath + " " + pid;
}

// commandStatus: 0 C    1 CPP
std::string FaultLoggerdSystemTest::GetstackfileNamePrefix(std::string ErrorCMD, int commandStatus)
{
    std::vector<std::string> cmds { "crasher_c", ErrorCMD };
    std::string pid = FaultLoggerdSystemTest::ForkAndRunCommands(cmds, commandStatus);
    int sleepSecond = 5;
    sleep(sleepSecond);
    std::string prefix = "stacktrace-" + pid;
    std::string filePath = GetLogFileName(prefix);
    return filePath + " " + pid;
}

int FaultLoggerdSystemTest::CheckKeywords(std::string& filePath, std::string *keywords, int length, int minRegIdx)
{
    ifstream file;
    file.open(filePath.c_str(), ios::in);
    long lines = FaultLoggerdSystemTest::CountLines(filePath);
    std::vector<string> t(lines * NUMBER_FOUR);
    int i = 0;
    int j = 0;
    string::size_type idx;
    int count = 0;
    int maxRegIdx = minRegIdx + REGISTERS_NUM + 1;
    while (!file.eof()) {
        file >> t.at(i);
        idx = t.at(i).find(keywords[j]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << t.at(i);
            if (minRegIdx != -1 && j > minRegIdx && j < maxRegIdx) { // -1 : do not check register value
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
    return count == length ? 0 : 1;
}

int FaultLoggerdSystemTest::CheckCountNum(std::string& filePath, std::string& pid, std::string& ErrorCMD)
{
    std::map<std::string, std::string> CmdKey = {
#if defined(__LP64__)
        { std::string("triSIGILL"),  std::string("SIGTRAP") },
        { std::string("triSIGTRAP"), std::string("SIGILL") },
#else
        { std::string("triSIGILL"),  std::string("SIGILL") },
        { std::string("triSIGTRAP"), std::string("SIGTRAP") },
#endif
        { std::string("triSIGSEGV"), std::string("SIGSEGV") },
        { std::string("MaxStack"), std::string("SIGSEGV") },
        { std::string("MaxMethod"), std::string("SIGSEGV") },
        { std::string("STACKTRACE"), std::string("Tid") },
        { std::string("STACKOF"), std::string("SIGSEGV") },
    };

    std::map<std::string, std::string>::iterator key;
    key = CmdKey.find(ErrorCMD);
    if (key != CmdKey.end()) {
        ErrorCMD = key->second;
    }
    string log[] = {
        "Pid:", "Uid", ":crasher", ErrorCMD, "Tid:", "#00", "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    log[0] = log[0] + pid;
    int minRegIdx = 6;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), minRegIdx);
}

int FaultLoggerdSystemTest::CheckCountNumAbort(std::string& filePath, std::string& pid)
{
    string log[] = {
        "Pid:", "Uid", ":crasher", "SIGABRT", "LastFatalMessage:", "ABORT!", "Tid:", "#00", "Registers:",
        REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    log[0] = log[0] + pid;
    int minRegIdx = 8;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), minRegIdx);
}

int FaultLoggerdSystemTest::CheckCountNumPCZero(std::string& filePath, std::string& pid)
{
    string log[] = {
        "Pid:", "Uid", ":crasher", "SIGSEGV", "Tid:", "#00", "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    log[0] = log[0] + pid;
    int minRegIdx = 6;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), minRegIdx);
}

int FaultLoggerdSystemTest::CheckCountNumOverStack(std::string& filePath, std::string& pid)
{
    string log[] = {
        "Pid:", "Uid", ":crasher", "SIGSEGV", "Tid:", "#56", "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    log[0] = log[0] + pid;
    int minRegIdx = 6;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), minRegIdx);
}

int FaultLoggerdSystemTest::CheckCountNumMultiThread(std::string& filePath, std::string& pid)
{
    string log[] = {
        "Pid:", "Uid", ":crasher", "SIGSEGV", "Tid:", "#00",
        "Registers:", REGISTERS, "FaultStack:", "Maps:",
        "/crasher"
    };
    log[0] = log[0] + pid;
    int minRegIdx = 6;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), minRegIdx);
}

std::string FaultLoggerdSystemTest::GetStackTop(void)
{
    ifstream spFile;
    spFile.open("sp");
    std::string sp;
    spFile >> sp;
    GTEST_LOG_(INFO) << "sp:" << sp;
    spFile.close();
    int ret = remove("sp");
    if (ret != 0) {
        printf("remove failed!");
    }
    return sp;
}

int FaultLoggerdSystemTest::CheckCountNumStackTop(std::string& filePath, std::string& pid, std::string& ErrorCMD)
{
    std::string sp = FaultLoggerdSystemTest::GetStackTop();
    std::map<std::string, std::string> CmdKey = {
#if defined(__LP64__)
        { std::string("StackTop"), std::string("SIGTRAP") },
#else
        { std::string("StackTop"), std::string("SIGILL") },
#endif
    };
    std::map<std::string, std::string>::iterator key;
    key = CmdKey.find(ErrorCMD);
    if (key != CmdKey.end()) {
        ErrorCMD = key->second;
    }
    string log[] = {
        "Pid:", "Uid", ":crasher", ErrorCMD, "Tid:", "#00", "Registers:", REGISTERS, "FaultStack:", "Maps:", "/crasher"
    };
    log[0] = log[0] + pid;
    int minRegIdx = 6;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), minRegIdx);
}

int FaultLoggerdSystemTest::CheckStacktraceCountNum(std::string& filePath, std::string& pid)
{
    string log[] = {
        "Tid:", ":crasher", "#00",
    };
    log[0] = log[0] + pid;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), -1); // -1 : do not check register value
}

std::string FaultLoggerdSystemTest::ForkAndCommands(int crasherType, int udid)
{
    setuid(udid);
    if (crasherType == 0) {
        system("/data/crasher_c thread-Loop &");
    } else {
        system("/data/crasher_cpp thread-Loop &");
    }
    std::string procCMD = "pgrep 'crasher'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMD;
    FILE *procFileInfo = nullptr;
    procFileInfo = popen(procCMD.c_str(), "r");
    if (procFileInfo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    if (udid == 0) { // root
        int i = 0;
        while ((fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) && (i < NUMBER_SIXTY)) {
            std::string pidLog = resultBufShell;
            GTEST_LOG_(INFO) << "pidList[" << i << "]" << pidLog << ".";
            if (IsDigit(pidLog) == false) {
                continue;
            }
            if (loopSysPid == atoi(pidLog.c_str())) {
                GTEST_LOG_(INFO) << loopSysPid;
            } else {
                if (crasherType == 1) {
                    FaultLoggerdSystemTest::loopCppPid = atoi(pidLog.c_str());
                } else {
                    FaultLoggerdSystemTest::loopRootPid = atoi(pidLog.c_str());
                }
            }
        }
        GTEST_LOG_(INFO) << "Root ID: " << FaultLoggerdSystemTest::loopRootPid;
    } else if (udid == BMS_UID) { // sys
        while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
            std::string pidLog = resultBufShell;
            loopSysPid = atoi(pidLog.c_str());
            GTEST_LOG_(INFO) << "System ID: " << loopSysPid;
        }
    } else { // app
        while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
            std::string pidLog = resultBufShell;
            if (loopSysPid == atoi(pidLog.c_str())) {
                GTEST_LOG_(INFO) << loopSysPid;
            } else if (loopRootPid == atoi(pidLog.c_str())) {
                GTEST_LOG_(INFO) << loopRootPid;
            } else {
                loopAppPid = atoi(pidLog.c_str());
            }
        }
        GTEST_LOG_(INFO) << "APP ID: " << loopAppPid;
    }
    pclose(procFileInfo);
    return std::to_string(loopSysPid);
}

// create crasher loop for: 1. system; 2. root; 3.app; 4. root+cpp
void FaultLoggerdSystemTest::StartCrasherLoop(int type)
{
    if (type == NUMBER_ONE) {
        int crasherType = 0;
        int uidSetting = BMS_UID;
        setuid(uidSetting);
        FaultLoggerdSystemTest::ForkAndCommands(crasherType, uidSetting);
        if (loopSysPid == 0) {
            exit(0);
        }

        int sleepSecond = 5;
        usleep(sleepSecond);
        std::string procCMD = "ls /proc/" + std::to_string(loopSysPid) + "/task";
        FILE *procFileInfo = nullptr;
        procFileInfo = popen(procCMD.c_str(), "r");
        if (procFileInfo == nullptr) {
            perror("popen execute failed");
            exit(1);
        }
        int sysTidCount = 0;
        while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
            sysTid[sysTidCount] = resultBufShell;
            GTEST_LOG_(INFO) << "procFileInfo print info = " << resultBufShell;
            sysTidCount = sysTidCount + 1;
        }
        int otheruid = OTHER_UID;
        setuid(otheruid);
        pclose(procFileInfo);
    } else if (type == NUMBER_TWO) {
        int crasherType = 0;
        int rootuid = 0;
        setuid(rootuid);
        FaultLoggerdSystemTest::ForkAndCommands(crasherType, rootuid);
        if (loopRootPid == 0) {
            exit(0);
        }

        int sleepSecond = 5;
        usleep(sleepSecond);
        std::string procCMD = "ls /proc/" + std::to_string(loopRootPid) + "/task";
        FILE *procFileInfo = nullptr;
        procFileInfo = popen(procCMD.c_str(), "r");
        if (procFileInfo == nullptr) {
            perror("popen execute failed");
            exit(1);
        }
        int rootTidCount = 0;
        while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
            rootTid[rootTidCount] = resultBufShell;
            GTEST_LOG_(INFO) << "procFileInfo print info = " << resultBufShell;
            rootTidCount = rootTidCount + 1;
        }
        int otheruid = OTHER_UID;
        setuid(otheruid);
        pclose(procFileInfo);
    } else if (type == NUMBER_THREE) {
        int crasherType = 0;
        int uidSetting = OTHER_UID;
        setuid(uidSetting);
        FaultLoggerdSystemTest::ForkAndCommands(crasherType, uidSetting);
        if (loopAppPid == 0) {
            exit(0);
        }

        int sleepSecond = 5;
        usleep(sleepSecond);
        std::string procCMD = "ls /proc/" + std::to_string(loopAppPid) + "/task";
        FILE *procFileInfo = nullptr;
        procFileInfo = popen(procCMD.c_str(), "r");
        if (procFileInfo == nullptr) {
            perror("popen execute failed");
            exit(1);
        }
        int appTidCount = 0;
        while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
            appTid[appTidCount] = resultBufShell;
            GTEST_LOG_(INFO) << "procFileInfo print info = " << resultBufShell;
            appTidCount = appTidCount + 1;
        }
        pclose(procFileInfo);
    } else if (type == NUMBER_FOUR) {
        int crasherType = 1;
        int otheruid = OTHER_UID;
        setuid(otheruid);
        FaultLoggerdSystemTest::ForkAndCommands(crasherType, otheruid);
        DfxDumpCatcher dumplog;
        std::string msg = "";
        dumplog.DumpCatch(loopAppPid, 0, msg);
        int sleepSecond = 5;
        sleep(sleepSecond);
        std::string procCMD = "ls /proc/" + std::to_string(loopAppPid) + "/task";
        FILE *procFileInfo = nullptr;

        procFileInfo = popen(procCMD.c_str(), "r");
        if (procFileInfo == nullptr) {
            perror("popen execute failed");
            exit(1);
        }
        while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
            GTEST_LOG_(INFO) << "procFileInfo print info = " << resultBufShell;
        }
        pclose(procFileInfo);
    }
}

void FaultLoggerdSystemTest::StartCrasherLoopForUnsingPidAndTid(int crasherType)
{
    int sysTidCount = 0;
    int uidSetting = BMS_UID;
    setuid(uidSetting);
    if (crasherType == 0) {
        system("/data/crasher_c thread-Loop &");
    } else {
        system("/data/crasher_cpp thread-Loop &");
    }
    std::string procCMDOne = "pgrep 'crasher'";
    GTEST_LOG_(INFO) << "threadCMD = " << procCMDOne;
    FILE *procFileInfoOne = nullptr;
    procFileInfoOne = popen(procCMDOne.c_str(), "r");
    if (procFileInfoOne == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfoOne) != nullptr) {
        std::string pidLog = resultBufShell;
        unsigLoopSysPid = atoi(pidLog.c_str());
        GTEST_LOG_(INFO) << "System ID: " << unsigLoopSysPid;
    }
    pclose(procFileInfoOne);
    if (unsigLoopSysPid == 0) {
        exit(0);
    }
    int sleepSecond = 5;
    usleep(sleepSecond);
    std::string procCMDTwo = "ls /proc/" + std::to_string(unsigLoopSysPid) + "/task";
    FILE *procFileInfoTwo = nullptr;
    procFileInfoTwo = popen(procCMDTwo.c_str(), "r");
    if (procFileInfoTwo == nullptr) {
        perror("popen execute failed");
        exit(1);
    }
    while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfoTwo) != nullptr) {
        sysTid[sysTidCount] = resultBufShell;
        GTEST_LOG_(INFO) << "procFileInfoTwo print info = " << resultBufShell;
        sysTidCount = sysTidCount + 1;
    }
    pclose(procFileInfoTwo);
}

void FaultLoggerdSystemTest::Trim(std::string & str)
{
    std::string blanks("\f\v\r\t\n ");
    str.erase(0, str.find_first_not_of(blanks));
    str.erase(str.find_last_not_of(blanks) + 1);
}

int FaultLoggerdSystemTest::CheckCountNumKill11(std::string& filePath, std::string& pid)
{
    string log[] = {
        "Pid:", "Uid", ":foundation", "SIGSEGV", "Tid:", "#00", "Registers:", REGISTERS,
        "FaultStack:","Maps:"
    };
    log[0] = log[0] + pid;
    int minRegIdx = 6;
    return CheckKeywords(filePath, log, sizeof(log) / sizeof(log[0]), minRegIdx);
}

int FaultLoggerdSystemTest::crashThread(int threadID)
{
    GTEST_LOG_(INFO) << "threadID :" << threadID;
    int i;
    for (i = 0; i < NUMBER_FIFTY; i++) {
        system("./crasher_c CrashTest &");
    }
    sleep(1);
    return 0;
}

int FaultLoggerdSystemTest::getApplyPid(std::string applyName)
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
    while (fgets(resultBufShell, sizeof(resultBufShell), procFileInfo) != nullptr) {
        applyPid = resultBufShell;
        GTEST_LOG_(INFO) << "applyPid: " << applyPid;
    }
    pclose(procFileInfo);
    int intApplyPid = std::atoi(applyPid.c_str());
    return intApplyPid;
}

std::string FaultLoggerdSystemTest::GetCmdResultFromPopen(const std::string& cmd)
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

int FaultLoggerdSystemTest::GetServicePid(const std::string& serviceName)
{
    std::string cmd = "pidof " + serviceName;
    std::string pidStr = GetCmdResultFromPopen(cmd);
    int32_t pid = 0;
    std::stringstream pidStream(pidStr);
    pidStream >> pid;
    printf("the pid of service(%s) is %s \n", serviceName.c_str(), pidStr.c_str());
    return pid;
}

int FaultLoggerdSystemTest::LaunchTestHap(const std::string& abilityName, const std::string& bundleName)
{
    std::string launchCmd = "/system/bin/aa start -a " + abilityName + " -b " + bundleName;
    (void)GetCmdResultFromPopen(launchCmd);
    sleep(2); // 2 : sleep 2s
    return GetServicePid(bundleName);
}

void FaultLoggerdSystemTest::dumpCatchThread(int threadID)
{
    std::string accountmgr = "accountmgr";
    int accountmgrPid = FaultLoggerdSystemTest::getApplyPid(accountmgr);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(accountmgrPid, 0, msg);
    GTEST_LOG_(INFO) << "ret:" << ret;
    if (ret == true) {
        FaultLoggerdSystemTest::count++;
    }
}

namespace {
#ifdef __pre__
/**
* @tc.name: FaultLoggerdSystemTest0010_pre
* @tc.desc: test CPP crasher application: Multithreading
* @tc.type:
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0010_pre, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0010_pre: start.";
    for (int i = 0; i < 10; i++) {
        g_mutex.lock();
        std::thread (FaultLoggerdSystemTest::dumpCatchThread, i).join();
        sleep(NUMBER_TWO);
        g_mutex.unlock();
    }
    EXPECT_EQ(FaultLoggerdSystemTest::count, 10) << "FaultLoggerdSystemTest0010_pre Failed";
    if (count == 10) {
        std::ofstream fout;
        fout.open("result", ios::app);
        fout << "sucess!" << std::endl;
        fout.close();
    }
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0010_pre: end.";

}
#endif

#ifndef __pre__
static const int NUMBER_TEN = 10;

/**
* @tc.name: FaultLoggerdSystemTest0010
* @tc.desc: test CPP crasher application: Multi process and Multithreading
* @tc.type:
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0010: start.";
    for (int i = 0; i < 10; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            exit(-1);
        } else if (pid == 0) {
            execl("/data/test_faultloggerd_pre", "test_faultloggerd_pre", \
                "--gtest_filter=FaultLoggerdSystemTest.FaultLoggerdSystemTest0010_pre", NULL);
        } else {
            wait(NULL);
        }
    }
    std::string filePath = "result";
    int lines = FaultLoggerdSystemTest::CountLines(filePath);
    GTEST_LOG_(INFO) << lines;
    int ret = remove("result");
    if (ret != 0) {
        printf("remove failed!");
    }
    EXPECT_EQ(lines, 10) << "FaultLoggerdSystemTest0010_pre Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0010: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0012
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(app),PID(accountmgr)}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0012: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    int uidSetting = 0;
    setuid(uidSetting);
    std::string apply = "accountmgr";
    int applyPid1 = FaultLoggerdSystemTest::getApplyPid(apply);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = FaultLoggerdSystemTest::loopAppPid;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = {"Tid:", "Name:", "Tid:", "Name:crasher_c"};
    log[0] = log[0] + std::to_string(applyPid1);
    log[1] = log[1] + apply;
    log[2] = log[2] + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < 4; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int expectNum = sizeof(log) / sizeof(log[0]);
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0012 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    sleep(NUMBER_TWO);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0012: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0013
 * @tc.desc: test DumpCatchMultiPid API: multiPid{0,0}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0013: start.";
    int applyPid1 = 0;
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = 0;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0013 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0013: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0014
 * @tc.desc: test DumpCatchMultiPid API: multiPid{-11,-11}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0014: start.";
    int applyPid1 = -11;
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = -11;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0013 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0014: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0015
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr),0}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0015: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    int uidSetting = 0;
    setuid(uidSetting);
    std::string apply = "accountmgr";
    int applyPid1 = FaultLoggerdSystemTest::getApplyPid(apply);
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
    log[1] = log[1] + apply;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < 3; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int expectNum = sizeof(log) / sizeof(log[0]);
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0015 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    sleep(NUMBER_TWO);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0015: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0016
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr),PID(app),PID(foundation)}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0016: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    int uidSetting = 0;
    setuid(uidSetting);
    std::string apply1 = "accountmgr";
    int applyPid1 = FaultLoggerdSystemTest::getApplyPid(apply1);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = FaultLoggerdSystemTest::loopAppPid;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::string apply3 = "foundation";
    int applyPid3 = FaultLoggerdSystemTest::getApplyPid(apply3);
    GTEST_LOG_(INFO) << "applyPid3:" << applyPid3;
    std::vector<int> multiPid {applyPid1, applyPid2, applyPid3};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "Name:", "Tid:", "Name:crasher_c", "Tid:", "Name:" };
    log[0] = log[0] + std::to_string(applyPid1);
    log[1] = log[1] + apply1;
    log[2] = log[2] + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    log[4] = log[4] + std::to_string(applyPid3);
    log[5] = log[5] + apply3;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < 6; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int expectNum = sizeof(log) / sizeof(log[0]);
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0016 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    sleep(NUMBER_TWO);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0016: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0017
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr),-11}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0017: start.";
    std::string apply = "accountmgr";
    int applyPid1 = FaultLoggerdSystemTest::getApplyPid(apply);
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = -11;
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
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < 3; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int expectNum = sizeof(log) / sizeof(log[0]);
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0017 Failed";
    sleep(NUMBER_TWO);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0017: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0018
 * @tc.desc: test DumpCatchMultiPid API: multiPid{9999,9999}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0018: start.";
    int uidSetting = 0;
    setuid(uidSetting);
    int applyPid1 = 9999;
    GTEST_LOG_(INFO) << "applyPid1:" << applyPid1;
    int applyPid2 = 9999;
    GTEST_LOG_(INFO) << "applyPid2:" << applyPid2;
    std::vector<int> multiPid {applyPid1, applyPid2};
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMultiPid(multiPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0018 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0018: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0019
 * @tc.desc: test DumpCatchMultiPid API: multiPid{PID(accountmgr),9999}
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0019: start.";
    std::string apply = "accountmgr";
    int applyPid1 = FaultLoggerdSystemTest::getApplyPid(apply);
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
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < 3; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int expectNum = sizeof(log) / sizeof(log[0]);
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0019 Failed";
    sleep(NUMBER_TWO);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0019: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0020
 * @tc.desc: test DumpCatchFrame API: PID(test_faul), TID(test_faul)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0020: start.";
    std::string apply = "test_faul";
    int testPid = FaultLoggerdSystemTest::getApplyPid(apply);
    GTEST_LOG_(INFO) << testPid;
    DfxDumpCatcher dumplog(getpid());
    std::string msg = "";
    std::vector<NativeFrame> frameV;
    bool ret = dumplog.InitFrameCatcher();
    EXPECT_EQ(ret, true);
    ret = dumplog.ReleaseThread(gettid());
    EXPECT_EQ(ret, true);
    ret = dumplog.CatchFrame(gettid(), frameV, true);
    EXPECT_EQ(ret, true);
    dumplog.DestroyFrameCatcher();
    EXPECT_GT(frameV.size(), 0) << "FaultLoggerdSystemTest0020 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0020: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0021
 * @tc.desc: test DumpCatchFrame API: PID(test_faul), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0021: start.";
    std::string apply = "test_faul";
    int testPid = FaultLoggerdSystemTest::getApplyPid(apply);
    GTEST_LOG_(INFO) << testPid;
    DfxDumpCatcher dumplog;
    std::vector<NativeFrame> frameV;
    bool ret = dumplog.CatchFrame(testPid, frameV, true);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0021 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0021: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0024
 * @tc.desc: test DumpCatchFrame API: app PID(-11), TID(test_faul)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0024: start.";
    std::string apply = "test_faul";
    int testPid = FaultLoggerdSystemTest::getApplyPid(apply);
    GTEST_LOG_(INFO) << testPid;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    std::vector<NativeFrame> frameV;
    bool ret = dumplog.CatchFrame(-99, frameV);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0024 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0024: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0025
 * @tc.desc: test DumpCatchFrame API: app PID(test_faul), TID(-11)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0025, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0025: start.";
    std::string apply = "test_faul";
    int testPid = FaultLoggerdSystemTest::getApplyPid(apply);
    GTEST_LOG_(INFO) << testPid;
    DfxDumpCatcher dumplog;
    std::string msg = "";
    std::vector<NativeFrame> frameV;
    bool ret = dumplog.CatchFrame(-1, frameV);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0025 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0025: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0026
 * @tc.desc: test DumpCatchFrame API: app PID(-11), TID(-11)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0026, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0026: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    std::vector<NativeFrame> frameV;
    bool ret = dumplog.CatchFrame(-11, frameV, true);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0026 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0026: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0027
 * @tc.desc: test DumpCatchMix API: PID(systemui pid), TID(0)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0027, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0027: start.";
    std::string testBundleName = "com.ohos.photos";
    std::string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(testPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "Name:com.ohos.photos", "#00", "/system/bin/appspawn",
        "Name:DfxWatchdog", "Name:GC_WorkerThread", "Name:ace.bg.1"};
    log[0] += std::to_string(testPid);
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

    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0027 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0027: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0028
 * @tc.desc: test DumpCatchMix API: PID(systemui pid), TID(systemui pid)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0028, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0028: start.";
    std::string testBundleName = "com.ohos.photos";
    std::string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(testPid, testPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    string log[] = { "Tid:", "Name:com.ohos.photos", "#00", "/system/bin/appspawn"};
    log[0] += std::to_string(testPid);
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int i = 0; i < 4; i++) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int expectNum = sizeof(log) / sizeof(log[0]);
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0028 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0028: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0029
 * @tc.desc: test DumpCatchMix API: PID(hap pid), TID(-1)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0029, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0029: start.";
    std::string testBundleName = "com.ohos.photos";
    std::string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(testPid, -1, msg);
    sleep(2);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0029 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0029: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0030
 * @tc.desc: test DumpCatchMix API: PID(-1), TID(-1)
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0030, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0030: start.";
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatchMix(-1, -1, msg);
    sleep(2);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0030 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0030: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0031
 * @tc.desc: test dumpcatcher command: -m -p systemui
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0031, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0031: start.";
    std::string testBundleName = "com.ohos.photos";
    std::string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    std::string procCMD = "dumpcatcher -m -p " + std::to_string(testPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = { "Tid:", "Name:com.ohos.photos", "#00", "/system/bin/appspawn",
        "Name:DfxWatchdog", "Name:GC_WorkerThread", "Name:ace.bg.1"};
    log[0] += std::to_string(testPid);
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < expectNum; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0031 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0031: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0032
 * @tc.desc: test dumpcatcher command: -m -p systemui tid mainthread
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0032, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0032: start.";
    std::string testBundleName = "com.ohos.photos";
    std::string testAbiltyName = testBundleName + ".MainAbility";
    int testPid = LaunchTestHap(testAbiltyName, testBundleName);
    std::string procCMD = "dumpcatcher -m -p " + std::to_string(testPid) +
        " -t " + std::to_string(testPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = { "Tid:", "Name:com.ohos.photos", "#00", "/system/bin/appspawn"};
    log[0] += std::to_string(testPid);
    string::size_type idx;
    int expectNum = sizeof(log) / sizeof(log[0]);
    for (int i = 0; i < 4; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, expectNum) << "FaultLoggerdSystemTest0032 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0032: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0033
 * @tc.desc: test dumpcatcher command: -m -p systemui tid -1
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0033, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0033: start.";
    std::string systemui = "com.ohos.systemui";
    int systemuiPid = FaultLoggerdSystemTest::GetServicePid(systemui);
    std::string procCMD = "dumpcatcher -m -p " + std::to_string(systemuiPid) + " -t -1";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest0033 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0033: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0034
 * @tc.desc: test dumpcatcher command: -m -p -1 tid -1
 * @tc.type: FUNC
 * @tc.require: issueI5PJ9O
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0034, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0034: start.";
    std::string systemui = "com.ohos.systemui";
    std::string procCMD = "dumpcatcher -m -p -1 -t -1";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest0034 Failed";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0034: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0122
 * @tc.desc: test C crasher application: StackTop
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0122, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0122: start.";
    std::string cmd = "StackTop";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0122 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumStackTop(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0122 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0122: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0123
 * @tc.desc: test CPP crasher application: StackTop
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0123, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0123: start.";
    std::string cmd = "StackTop";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0123 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumStackTop(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0123 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0123: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0001
* @tc.desc: test C crasher application: triSIGILL
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0001: start.";
    std::string cmd = "triSIGILL";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0001 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0001 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0001: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0002
* @tc.desc: test CPP crasher application: triSIGILL
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0002: start.";
    std::string cmd = "triSIGILL";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0002 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0002 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0002: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0003
* @tc.desc: test C crasher application: triSIGSEGV
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0003: start.";
    std::string cmd = "triSIGSEGV";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0003 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0003 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0003: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0004
* @tc.desc: test CPP crasher application: triSIGSEGV
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0004: start.";
    std::string cmd = "triSIGSEGV";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0004 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0004 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0004: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0005
* @tc.desc: test C crasher application: triSIGTRAP
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0005: start.";
    std::string cmd = "triSIGTRAP";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0005 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0005 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0005: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0006
* @tc.desc: test CPP crasher application: triSIGTRAP
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0006: start.";
    std::string cmd = "triSIGTRAP";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0006 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0006 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0006: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0007
* @tc.desc: test C crasher application: triSIGABRT
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0007: start.";
    std::string cmd = "triSIGABRT";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0007 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumAbort(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0007 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0007: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest0008
* @tc.desc: test CPP crasher application: triSIGABRT
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0008: start.";
    std::string cmd = "triSIGABRT";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest0008 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumAbort(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest0008 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0008: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0009
 * @tc.desc: test C crasher application: 50 Abnormal signal
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0009: start.";
    std::string rmFiles = "rm -rf /data/log/faultlog/temp/*";
    system(rmFiles.c_str());
    int i;
    for (i = 0; i < 50; i++) {
        system("/data/crasher_c CrashTest &");
    }

    int sleepSecond = 10;
    sleep(sleepSecond);
    if (i == 50) {
        std::vector<std::string> files;
        OHOS::GetDirFiles("/data/log/faultlog/temp/", files);
        GTEST_LOG_(INFO) << files.size();
        if (files.size() == 50) {
            GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0009: end.";
        }
    } else {
        EXPECT_EQ(0, 1) << "FaultLoggerdSystemTest0009 Failed";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest001
 * @tc.desc: test C crasher application: SIGFPE
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest001: start.";

    std::string cmd = "SIGFPE";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest001 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;

        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;

        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;

        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest001 Failed";

        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest001: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest001_level0
 * @tc.desc: test C crasher application: SIGFPE
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest001_level0, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest001_level0: start.";

    std::string cmd = "SIGFPE";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest001_level0 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;

        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;

        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;

        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest001_level0 Failed";

        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest001_level0: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest002
 * @tc.desc: test C crasher application: SIGILL
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest002, TestSize.Level2)
{

    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest002: start.";

    std::string cmd = "SIGILL";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest002 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;

        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;

        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;

        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest002 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest002: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest003
 * @tc.desc: test C crasher application: SIGSEGV
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest003, TestSize.Level2)
{

    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest003: start.";

    std::string cmd = "SIGSEGV";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest003 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;

        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;

        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest003 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest003: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest004
 * @tc.desc: test C crasher application: SIGTRAP
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest004: start.";
    std::string cmd = "SIGTRAP";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest004 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;

        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        bool testResult = false;
        if (ret == 0) {
            testResult = true;
        }
        EXPECT_EQ(true, testResult) << "ProcessDfxRequestTest004 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest004: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest005
 * @tc.desc: test C crasher application: SIGABRT
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest005: start.";
    std::string cmd = "SIGABRT";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest005 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;

        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumAbort(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;

        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest005 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest005: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest006
* @tc.desc: test C crasher application: SIGBUS
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest006: start.";
    std::string cmd = "SIGBUS";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest006 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;

        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest006 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest006: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest007
* @tc.desc: test C crasher application: STACKTRACE
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest007: start.";
    std::string cmd = "STACKTRACE";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest007: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest008
* @tc.desc: test C crasher application: triSIGTRAP
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest008: start.";
    std::string cmd = "triSIGTRAP";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest008 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest008 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest008: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest009
* @tc.desc: test C crasher application: triSIGSEGV
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest009, TestSize.Level2)
{

    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest009: start.";

    std::string cmd = "triSIGSEGV";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest009 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;

        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;

        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;

        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest009 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest009: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest010
* @tc.desc: test C crasher application: MaxStack
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest010: start.";
    std::string cmd = "MaxStack";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest010 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest010 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest010: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest011
* @tc.desc: test C crasher application: MaxMethod
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest011: start.";
    std::string cmd = "MaxMethod";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest011 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest011 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest011: end.";
    }
}



/**
 * @tc.name: FaultLoggerdSystemTest013
 * @tc.desc: test CPP crasher application: SIGFPE
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest013: start.";
    std::string cmd = "SIGFPE";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest013 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest013 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest013: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest014
 * @tc.desc: test CPP crasher application: SIGILL
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest014: start.";
    std::string cmd = "SIGILL";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest014 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest014 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest014: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest015
 * @tc.desc: test CPP crasher application: SIGSEGV
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest015, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest015: start.";
    std::string cmd = "SIGSEGV";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest015 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest015 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest015: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest016
 * @tc.desc: test CPP crasher application: SIGTRAP
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest016: start.";
    std::string cmd = "SIGTRAP";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest016 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest016 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest016: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest017
 * @tc.desc: test CPP crasher application: SIGABRT
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest017, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest017: start.";
    std::string cmd = "SIGABRT";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest017 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumAbort(filePathStr, pidStr);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest017 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest017: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest018
* @tc.desc: test CPP crasher application: SIGBUS
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest018, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest018: start.";
    std::string cmd = "SIGBUS";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest018 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest018 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest018: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest019
* @tc.desc: test CPP crasher application: STACKTRACE
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest019, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest019: start.";
    std::string cmd = "STACKTRACE";
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest019: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest020
* @tc.desc: test CPP crasher application: triSIGTRAP
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest020, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest020: start.";
    std::string cmd = "triSIGTRAP";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest020 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest020 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest020: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest021
* @tc.desc: test CPP crasher application: triSIGSEGV
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest021, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest021: start.";
    std::string cmd = "triSIGSEGV";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest021 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest021 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest021: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest022
* @tc.desc: test CPPcrasher application: MaxStack
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest022, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest022: start.";
    std::string cmd = "MaxStack";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest022 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);

        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest022 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest022: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest023
* @tc.desc: test CPP crasher application: MaxMethod
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest023, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest023: start.";
    std::string cmd = "MaxMethod";
    int cppTest = 1;
    string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest023 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest023 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest023: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest025
 * @tc.desc: test DumpCatch API: app PID(app), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest025, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest025: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    GTEST_LOG_(INFO) << "appPid: " << FaultLoggerdSystemTest::loopAppPid;
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest025 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest025: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest026
 * @tc.desc: test DumpCatch API: app PID(app), TID(PID)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest026, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest026: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, FaultLoggerdSystemTest::loopAppPid, msg);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest026 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest026: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest026_level0
 * @tc.desc: test DumpCatch API: app PID(app), TID(PID)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest026_level0, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest026_level0: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, FaultLoggerdSystemTest::loopAppPid, msg);
    GTEST_LOG_(INFO) << ret;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest026_level0 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest026_level0: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest027
 * @tc.desc: test DumpCatch API: app PID(app), TID(<>PID)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest027, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest027: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    int tid = std::stoi(appTid[0]);
    if (FaultLoggerdSystemTest::loopAppPid == std::stoi(FaultLoggerdSystemTest::appTid[0])) {
        tid = std::stoi(appTid[1]);
    }
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, tid, msg);
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest027 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest027: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest028
 * @tc.desc: test DumpCatch API: app PID(system), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest028, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest028: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopSysPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest028 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest028: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0104
 * @tc.desc: test DumpCatch API: app PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0104, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0104: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    int uidSetting = OTHER_UID;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopRootPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest0104 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0104: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest029
 * @tc.desc: test DumpCatch API: app PID(9999), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest029, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest029: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(9999, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest029 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest029: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest030
 * @tc.desc: test DumpCatch API: app PID(app), TID(9999)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest030, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest030: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, 9999, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest030 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest030: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest031
 * @tc.desc: test DumpCatch API: app PID(app), TID(system)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest031, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest031 start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, FaultLoggerdSystemTest::loopSysPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest031 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest031: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest032
 * @tc.desc: test DumpCatch API: app PID(Null), TID(app)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest032, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest032 start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    int uidSetting = OTHER_UID;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(0, FaultLoggerdSystemTest::loopAppPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest032 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest032: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest034
 * @tc.desc: test DumpCatch API: PID(-11), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest034, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest034 start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(-11, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest034 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest034: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest035
 * @tc.desc: test DumpCatch API: PID(root), TID(-11)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest035, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest035 start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopRootPid, -11, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest035 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest035: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest036
 * @tc.desc: test DumpCatch API: system PID(system), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest036, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest036: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    int uidSetting = BMS_UID;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopSysPid, 0, msg);
    GTEST_LOG_(INFO) << "loopSysPid : \n" << FaultLoggerdSystemTest::loopSysPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::loopSysPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int x = 0; x < 5; x = x + 1) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int otheruid = OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest036 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest036: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0100
 * @tc.desc: test DumpCatch API: root PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0100, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0100: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    int uidSetting = 0;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopRootPid, 0, msg);
    GTEST_LOG_(INFO) << "loopRootPid : \n" << FaultLoggerdSystemTest::loopRootPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::loopRootPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int x = 0; x < 5; x = x + 1) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int otheruid = OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest0100 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0100: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest037
 * @tc.desc: test DumpCatch API: system PID(app), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest037, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest037: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    int uidSetting = BMS_UID;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest::loopAppPid : \n" << FaultLoggerdSystemTest::loopAppPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::loopAppPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int x = 0; x < 5; x = x + 1) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int otheruid = OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest037 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest037: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0101
 * @tc.desc: test DumpCatch API: system PID(root), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0101, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0101: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    int uidSetting = BMS_UID;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopRootPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest::loopRootPid : \n" << FaultLoggerdSystemTest::loopRootPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::loopRootPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int x = 0; x < 5; x = x + 1) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int otheruid = OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest0101 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0101: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0102
 * @tc.desc: test DumpCatch API: root PID(system), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0102, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0102: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    int uidSetting = 0;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopSysPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest::loopSysPid : \n" << FaultLoggerdSystemTest::loopSysPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::loopSysPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int x = 0; x < 5; x = x + 1) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int otheruid = OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest0102 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0102: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0103
 * @tc.desc: test DumpCatch API: root PID(app), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0103, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0103: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    int uidSetting = 0;
    setuid(uidSetting);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest::loopAppPid : \n" << FaultLoggerdSystemTest::loopAppPid;
    string log[] = { "Tid:", "#00", "/data/crasher", "Name:SubTestThread", "usleep"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::loopAppPid) + ", Name:crasher";
    GTEST_LOG_(INFO) << "ret : \n" << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    string::size_type idx;
    int j = 0;
    int count = 0;
    for (int x = 0; x < 5; x = x + 1) {
        idx = msg.find(log[j]);
        GTEST_LOG_(INFO) << log[j];
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    int otheruid = OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest0103 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0103: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest038
 * @tc.desc: test DumpCatch API: app PID(app), TID(root)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest038, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest038 start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, FaultLoggerdSystemTest::loopRootPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest038 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest038: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest039
 * @tc.desc: test DumpCatch API: PID(root), TID(app)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest039, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest039 start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopRootPid, FaultLoggerdSystemTest::loopAppPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest039 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest039: end.";
}


/**
 * @tc.name: FaultLoggerdSystemTest040
 * @tc.desc: test dumpcatcher command: dumpcatcher -p apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest040, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest040: start uid:" << getuid();
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest040 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest040: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest040_level0
 * @tc.desc: test dumpcatcher command: dumpcatcher -p apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest040_level0, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest040_level0: start uid:" << getuid();
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest040_level0 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest040_level0: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest041
 * @tc.desc: test dumpcatcher command: dumpcatcher -p apppid -t apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest041, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest041: start uid:" << getuid();
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid) + " -t "+
        std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest041 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest041: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest042
 * @tc.desc: test dumpcatcher command: dumpcatcher -p apppid -t apptid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest042, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest042: start uid:" << getuid();
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    int tid = std::stoi(FaultLoggerdSystemTest::appTid[0]);
    if (FaultLoggerdSystemTest::loopAppPid == std::stoi(FaultLoggerdSystemTest::appTid[0])) {
        tid = std::stoi(FaultLoggerdSystemTest::appTid[1]);
    }
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid) + " -t "+
        std::to_string(tid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest042 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest042: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest043
 * @tc.desc: test dumpcatcher command: -p systempid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest043, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest043: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopSysPid) + " -t "
        + std::to_string(FaultLoggerdSystemTest::loopSysPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest043 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest043: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0109
 * @tc.desc: test dumpcatcher command: -p rootpid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0109, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0109: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopRootPid) + " -t "
        + std::to_string(FaultLoggerdSystemTest::loopRootPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest0109 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0109: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest044
 * @tc.desc: test dumpcatcher command: -p 9999 -t apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest044, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest044: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p 9999 -t "+ std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest044 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest044: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest045
 * @tc.desc: test dumpcatcher command: -p apppid -t 9999
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest045, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest045: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid) + " -t 9999";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest045 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest045: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest046
 * @tc.desc: test dumpcatcher command: -p apppid -t systempid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest046, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest046: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid) + " -t "
        + std::to_string(FaultLoggerdSystemTest::loopSysPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest046 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest046: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest047
 * @tc.desc: test dumpcatcher command: -p systempid -t apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest047, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest047: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopSysPid) + " -t "
        + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest047 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest047: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest048
 * @tc.desc: test dumpcatcher command: -p  -t apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest048, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest048: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p  -t " + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest048 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest048: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest049
 * @tc.desc: test dumpcatcher command: -p apppid -t
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest049, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest049: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid) + " -t ";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Usage:", "dump the stacktrace"};
    string::size_type idx;
    for (int i = 0; i < 2; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 2) << "FaultLoggerdSystemTest049 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest049: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest050
 * @tc.desc: test dumpcatcher command: -p -11 -t apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest050, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest050: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p -11 -t " + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest050 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest050: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest051
 * @tc.desc: test dumpcatcher command: -p apppid -t -11
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest051, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest051: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid) + " -t -11";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest051 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest051: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest052
 * @tc.desc: test dumpcatcher command: -p systempid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest052, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest052: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    int uidSetting = BMS_UID;
    setuid(uidSetting);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopSysPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + std::to_string(FaultLoggerdSystemTest::loopSysPid);
    string::size_type idx;
    for (int i = 0; i < 5; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    int otheruid =OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest052 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest052: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0105
 * @tc.desc: test dumpcatcher command: -p rootpid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0105, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0105: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    int uidSetting = 0;
    setuid(uidSetting);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopRootPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + std::to_string(FaultLoggerdSystemTest::loopRootPid);
    string::size_type idx;
    for (int i = 0; i < 5; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    int otheruid =OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest0105 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0105: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest053
 * @tc.desc: test dumpcatcher command: -p apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest053, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest053: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    int uidSetting = BMS_UID;
    setuid(uidSetting);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string::size_type idx;
    for (int i = 0; i < 5; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest053 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest053: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0106
 * @tc.desc: test dumpcatcher command: -p apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0106, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0106: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    int uidSetting = BMS_UID;
    setuid(uidSetting);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopRootPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher"};
    log[0] = log[0] + std::to_string(FaultLoggerdSystemTest::loopRootPid);
    string::size_type idx;
    for (int i = 0; i < 5; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 5) << "FaultLoggerdSystemTest0106 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0106: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0107
 * @tc.desc: test dumpcatcher command: -p apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0107, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0107: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    int uidSetting = 0;
    setuid(uidSetting);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    int apptid1 = std::stoi(appTid[0]);
    int apptid2 = std::stoi(appTid[1]);
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher", ""};
    log[0] = log[0] + std::to_string(apptid1);
    log[5] = log[5] + std::to_string(apptid2);
    string::size_type idx;
    for (int i = 0; i < 6; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 6) << "FaultLoggerdSystemTest0107 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0107: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0108
 * @tc.desc: test dumpcatcher command: -p sytempid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0108, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0108: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(1);
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    int uidSetting = 0;
    setuid(uidSetting);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopSysPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    int systid1 = std::stoi(sysTid[0]);
    int systid2 = std::stoi(sysTid[1]);
    string log[] = {"", "Name:crasher", "Name:SubTestThread", "#00", "/data/crasher", ""};
    log[0] = log[0] + std::to_string(systid1);
    log[5] = log[5] + std::to_string(systid2);
    string::size_type idx;
    for (int i = 0; i < 6; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    int otheruid = OTHER_UID;
    setuid(otheruid);
    EXPECT_EQ(count, 6) << "FaultLoggerdSystemTest0108 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(1);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0108: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest054
 * @tc.desc: test dumpcatcher command: -p apppid -t rootpid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest054, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest054: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid) + " -t " +
        std::to_string(FaultLoggerdSystemTest::loopRootPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest054 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest054: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest055
 * @tc.desc: test dumpcatcher command: -p rootpid, -t apppid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest055, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest055: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(2);
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopRootPid) + " -t " +
        std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest055 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(2);
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest055: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0114
 * @tc.desc: test dumpcatcher command: -p pid-max, -t threads_max
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0114, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0114: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p " + GetPidMax() + " -t " + GetTidMax();
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest0114 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0114: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0115
 * @tc.desc: test dumpcatcher command: -p 65535, -t 65535
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0115, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0115: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p 65535 -t 65535";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest0115 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0115: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0116
 * @tc.desc: test dumpcatcher command: -p 65536, -t 65536
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0116, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0116: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p 65536 -t 65536";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest0116 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0116: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0117
 * @tc.desc: test dumpcatcher command: -p 65534, -t 65534
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0117, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest117: start.";
    FaultLoggerdSystemTest::StartCrasherLoop(3);
    std::string procCMD = "dumpcatcher -p 65534 -t 65534";
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest0117 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0117: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest056
 * @tc.desc: test CPP DumpCatch API: PID(apppid), TID(0)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest056, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest056: start uid:" << getuid();
    FaultLoggerdSystemTest::StartCrasherLoop(4);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::loopAppPid, 0, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << "dump log : \n" << msg;
    EXPECT_EQ(ret, false) << "FaultLoggerdSystemTest056 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest056: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest057
 * @tc.desc: test dumpcatcher command: -p rootpid
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest057, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest057: start uid:" << getuid();
    FaultLoggerdSystemTest::StartCrasherLoop(4);
    std::string procCMD = "dumpcatcher -p " + std::to_string(FaultLoggerdSystemTest::loopAppPid);
    string procDumpLog = FaultLoggerdSystemTest::ProcessDumpCommands(procCMD);
    GTEST_LOG_(INFO) << "procDumpLog: " << procDumpLog;
    int count = 0;
    string log[] = {"Failed"};
    string::size_type idx;
    for (int i = 0; i < 1; i++) {
        idx = procDumpLog.find(log[i]);
        if (idx != string::npos) {
            GTEST_LOG_(INFO) << count;
            GTEST_LOG_(INFO) << log[i];
            count++;
        }
    }
    EXPECT_EQ(count, 1) << "FaultLoggerdSystemTest057 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(3);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest057: end.";
}

/**
* @tc.name: FaultLoggerdSystemTest058
* @tc.desc: test C crasher application: STACKOF
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest058, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest058: start.";
    std::string cmd = "STACKOF";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest058 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest058 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest058: end.";
    }
}

/**
* @tc.name: FaultLoggerdSystemTest059
* @tc.desc: test CPP crasher application: STACKOF
* @tc.type: FUNC
*/
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest059, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest059: start.";
    std::string cmd = "STACKOF";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "ProcessDfxRequestTest059 Failed";
    } else {
        GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
        char filePath[ARRAY_SIZE_HUNDRED];
        char pid[ARRAY_SIZE_HUNDRED];
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "ProcessDfxRequestTest059 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest059: end.";
    }
}
/**
 * @tc.name: FaultLoggerdSystemTest024
 * @tc.desc: test CPP crasher application: OOM
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest024, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest024: start.";
    std::string cmd = "OOM";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest024 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 1) << "ProcessDfxRequestTest024 Failed";
        GTEST_LOG_(INFO) << "ProcessDfxRequestTest024: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest012
 * @tc.desc: test C crasher application: OOM
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest012: start.";
    std::string cmd = "OOM";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest012 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNum(filePathStr, pidStr, cmd);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 1) << "FaultLoggerdSystemTest012 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest012: end.";
    }
}
/**
 * @tc.name: FaultLoggerdSystemTest0110
 * @tc.desc: test CPP crasher application: PCZero
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0110, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0110: start.";
    std::string cmd = "PCZero";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0110 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumPCZero(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0110 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0110: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0111
 * @tc.desc: test C crasher application: PCZero
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0111, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0111: start.";
    std::string cmd = "PCZero";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0111 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumPCZero(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0111 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0111: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0112
 * @tc.desc: test C crasher application: MTCrash
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0112, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0112: start.";
    std::string cmd = "MTCrash";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0112 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumMultiThread(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0112 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0112: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0113
 * @tc.desc: test CPP crasher application: MTCrash
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0113, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0113: start.";
    std::string cmd = "MTCrash";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0113 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumMultiThread(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0113 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0113: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0118
 * @tc.desc: test CPP crasher application: StackOver64
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0118, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0118: start.";
    std::string cmd = "StackOver64";
    int cppTest = 1;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cppTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0118 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumOverStack(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0118 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0118: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0119
 * @tc.desc: test C crasher application: StackOver64
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0119, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0119: start.";
    std::string cmd = "StackOver64";
    int cTest = 0;
    std::string filePathPid = FaultLoggerdSystemTest::GetfileNamePrefix(cmd, cTest);

    GTEST_LOG_(INFO) << "current filePath and pid = \n" << filePathPid;
    if (filePathPid.size() < NUMBER_TEN) {
        EXPECT_EQ(true, false) << "FaultLoggerdSystemTest0119 Failed";
    } else {
        char filePath[ARRAY_SIZE_HUNDRED] = { 0, };
        char pid[ARRAY_SIZE_HUNDRED]  = { 0, };
        int res = sscanf_s(filePathPid.c_str(), "%s %s", filePath, sizeof(filePath) - 1, pid, sizeof(pid) - 1);
        if (res <= 0) {
            GTEST_LOG_(INFO) << "sscanf_s failed.";
        }
        GTEST_LOG_(INFO) << "current filepath: \n" << filePath;
        GTEST_LOG_(INFO) << "current pid: \n" << pid;
        std::string filePathStr = filePath;
        std::string pidStr = pid;
        int ret = FaultLoggerdSystemTest::CheckCountNumOverStack(filePathStr, pidStr);
        GTEST_LOG_(INFO) << "current ret value: \n" << ret;
        EXPECT_EQ(ret, 0) << "FaultLoggerdSystemTest0119 Failed";
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0119: end.";
    }
}

/**
 * @tc.name: FaultLoggerdSystemTest0120
 * @tc.desc: test DumpCatch API: app unsigned PID(systempid), unsigned TID(systempid)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest,  FaultLoggerdSystemTest0120, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0120: start.";
    FaultLoggerdSystemTest::StartCrasherLoopForUnsingPidAndTid(0);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::unsigLoopSysPid, FaultLoggerdSystemTest::unsigLoopSysPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    int count = 0;
    string log[] = {"Tid:", "Name:crasher", "#00", "/data/crasher"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::unsigLoopSysPid);
    string::size_type idx;
    int j = 0;
    for (int x = 0; x < 4; x = x + 1) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 4) << "FaultLoggerdSystemTest0120 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(4);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0120: end.";
}

/**
 * @tc.name: FaultLoggerdSystemTest0121
 * @tc.desc: test DumpCatch API: app unsigned PID(systempid), unsigned TID(systempid)
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest,  FaultLoggerdSystemTest0121, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0121: start.";
    FaultLoggerdSystemTest::StartCrasherLoopForUnsingPidAndTid(1);
    DfxDumpCatcher dumplog;
    std::string msg = "";
    bool ret = dumplog.DumpCatch(FaultLoggerdSystemTest::unsigLoopSysPid, FaultLoggerdSystemTest::unsigLoopSysPid, msg);
    GTEST_LOG_(INFO) << ret;
    GTEST_LOG_(INFO) << msg;
    int count = 0;
    string log[] = {"Tid:", "Name:crasher", "#00", "/data/crasher"};
    log[0] = log[0] +std::to_string(FaultLoggerdSystemTest::unsigLoopSysPid);
    string::size_type idx;
    int j = 0;
    for (int x = 0; x < 4; x = x + 1) {
        idx = msg.find(log[j]);
        if (idx != string::npos) {
            count++;
        }
        j++;
    }
    GTEST_LOG_(INFO) << count;
    EXPECT_EQ(count, 4) << "FaultLoggerdSystemTest0121 Failed";
    FaultLoggerdSystemTest::KillCrasherLoopForSomeCase(4);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0121: end.";
}

static void CrashInChildThread()
{
    printf("CrashInChildThread(): TID = %ld\n", (long) gettid());
    raise(SIGSEGV);
}

static int RunInNewPidNs(void* arg)
{
    (void)arg;
    printf("RunInNewPidNs(): PID = %ld\n", (long) getpid());
    printf("RunInNewPidNs(): TID = %ld\n", (long) gettid());
    printf("RunInNewPidNs(): PPID = %ld\n", (long) getppid());
    std::thread childThread(CrashInChildThread);
    childThread.join();
    _exit(0);
}

/**
 * @tc.name: FaultLoggerdSystemTest0200
 * @tc.desc: test crash in process with pid namespace
 * @tc.type: FUNC
 */
HWTEST_F (FaultLoggerdSystemTest, FaultLoggerdSystemTest0200, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0200: start.";
    const int stackSz = 1024 * 1024 * 1024; // 1M
    void* cloneStack = mmap(NULL, stackSz,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 1, 0);
    if (cloneStack == nullptr) {
        FAIL();
    }
    cloneStack = (void *)(((uint8_t *)cloneStack) + stackSz - 1);
    int childPid = clone(RunInNewPidNs, cloneStack, CLONE_NEWPID | SIGCHLD, nullptr);
    if (childPid <= 0) {
        GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0200: Failed to clone new process. errno:" << errno;
        return;
    }
    // wait for log generation
    sleep(NUMBER_FOUR);
    std::string prefix = "cppcrash-" + std::to_string(childPid);
    std::string fileName = GetLogFileName(prefix);
    EXPECT_NE(0, fileName.size());
    printf("PidNs Crash File:%s\n", fileName.c_str());
    string log[] = {
        "Pid:", "Uid", "SIGSEGV", "Tid:", "#00",
        "Registers:", "FaultStack:", "Maps:"
    };
    int minRegIdx = 6;
    CheckKeywords(fileName, log, sizeof(log) / sizeof(log[0]), minRegIdx);
    GTEST_LOG_(INFO) << "FaultLoggerdSystemTest0200: end.";
}
#endif
}
