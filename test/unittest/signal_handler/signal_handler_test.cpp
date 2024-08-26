/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

#include <gtest/gtest.h>
#include <csignal>
#include <map>
#include <securec.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/prctl.h>

#include "dfx_define.h"
#include "dfx_signal_handler.h"
#include "dfx_signalhandler_exception.h"
#include "dfx_test_util.h"

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class SignalHandlerTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();
};

void SignalHandlerTest::SetUpTestCase()
{}

void SignalHandlerTest::TearDownTestCase()
{}

void SignalHandlerTest::SetUp()
{}

void SignalHandlerTest::TearDown()
{}

extern "C" void SetThreadInfoCallback(ThreadInfoCallBack func) __attribute__((weak));
extern "C" void DFX_InstallSignalHandler(void) __attribute__((weak));
extern "C" void SetAsyncStackCallbackFunc(void* func) __attribute__((weak));
extern "C" int DFX_SetAppRunningUniqueId(const char* appRunningId, size_t len) __attribute__((weak));
static bool CheckThreadCrashKeyWords(const string& filePath, pid_t pid, int sig)
{
    if (filePath.empty() || pid <= 0) {
        return false;
    }
    map<int, string> sigKey = {
        { SIGILL, string("SIGILL") },
        { SIGBUS, string("SIGBUS") },
        { SIGSEGV, string("SIGSEGV") },
    };
    string sigKeyword = "";
    map<int, string>::iterator iter = sigKey.find(sig);
    if (iter != sigKey.end()) {
        sigKeyword = iter->second;
    }
    string keywords[] = {
        "Pid:" + to_string(pid), "Uid:", "name:./test_signalhandler", sigKeyword,
        "Tid:", "#00", "Registers:", "FaultStack:", "Maps:", "test_signalhandler"
    };
    int length = sizeof(keywords) / sizeof(keywords[0]);
    int minRegIdx = -1;
    return CheckKeyWords(filePath, keywords, length, minRegIdx) == length;
}
static bool CheckCrashKeyWords(const string& filePath, pid_t pid, int sig)
{
    if (filePath.empty() || pid <= 0) {
        return false;
    }
    map<int, string> sigKey = {
        { SIGILL, string("SIGILL") },
        { SIGBUS, string("SIGBUS") },
        { SIGSEGV, string("SIGSEGV") },
        { SIGABRT, string("SIGABRT") },
        { SIGFPE, string("SIGFPE") },
        { SIGSTKFLT, string("SIGSTKFLT") },
        { SIGSYS, string("SIGSYS") },
        { SIGTRAP, string("SIGTRAP") },
    };
    string sigKeyword = "";
    map<int, string>::iterator iter = sigKey.find(sig);
    if (iter != sigKey.end()) {
        sigKeyword = iter->second;
    }
    string keywords[] = {
        "Pid:" + to_string(pid), "Uid:", "name:./test_signalhandler", sigKeyword,
        "Tid:", "#00", "Registers:", "FaultStack:", "Maps:", "test_signalhandler"
    };
    int length = sizeof(keywords) / sizeof(keywords[0]);
    int minRegIdx = -1;
    return CheckKeyWords(filePath, keywords, length, minRegIdx) == length;
}

void ThreadInfo(char* buf, size_t len, void* context __attribute__((unused)))
{
    char mes[] = "this is cash information of test thread";
    if (memcpy_s(buf, len, mes, sizeof(mes)) != 0) {
        GTEST_LOG_(INFO) << "Failed to set thread info";
    }
}

int TestThread(int threadId, int sig)
{
    std::string subThreadName = "SubTestThread" + to_string(threadId);
    prctl(PR_SET_NAME, subThreadName.c_str());
    if (SetThreadInfoCallback != nullptr) {
        SetThreadInfoCallback(ThreadInfo);
    }
    int cashThreadId = 2;
    if (threadId == cashThreadId) {
        GTEST_LOG_(INFO) << subThreadName << " is ready to raise signo(" << sig <<")";
        raise(sig);
    }
    return 0;
}

/**
 * @tc.name: SignalHandlerTest001
 * @tc.desc: test thread cash SignalHandler signo(SIGILL)
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest001: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        if (SetThreadInfoCallback != nullptr) {
            SetThreadInfoCallback(ThreadInfo);
        }
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGILL);
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckThreadCrashKeyWords(GetCppCrashFileName(pid), pid, SIGILL);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest001: end.";
}

/**
 * @tc.name: SignalHandlerTest002
 * @tc.desc: test thread cash SignalHandler signo(SIGBUS)
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest002: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        if (SetThreadInfoCallback != nullptr) {
            SetThreadInfoCallback(ThreadInfo);
        }
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGBUS);
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckThreadCrashKeyWords(GetCppCrashFileName(pid), pid, SIGBUS);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest002: end.";
}

/**
 * @tc.name: SignalHandlerTest003
 * @tc.desc: test thread cash SignalHandler signo(SIGSEGV)
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest003: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        if (SetThreadInfoCallback != nullptr) {
            SetThreadInfoCallback(ThreadInfo);
        }
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGSEGV);
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckThreadCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest003: end.";
}

/**
 * @tc.name: SignalHandlerTest004
 * @tc.desc: test thread crash SignalHandler in multi-thread situation signo(SIGILL)
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest004: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        std::thread (TestThread, 1, SIGILL).join(); // 1 : first thread
        std::thread (TestThread, 2, SIGILL).join(); // 2 : second thread
        _exit(0);
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckThreadCrashKeyWords(GetCppCrashFileName(pid), pid, SIGILL);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest004: end.";
}

/**
 * @tc.name: SignalHandlerTest005
 * @tc.desc: test thread crash SignalHandler in multi-thread situation signo(SIGBUS)
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest005: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        std::thread (TestThread, 1, SIGBUS).join(); // 1 : first thread
        std::thread (TestThread, 2, SIGBUS).join(); // 2 : second thread
        _exit(0);
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckThreadCrashKeyWords(GetCppCrashFileName(pid), pid, SIGBUS);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest005: end.";
}

/**
 * @tc.name: SignalHandlerTest006
 * @tc.desc: test thread crash SignalHandler in multi-thread situation signo(SIGSEGV)
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest006: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        std::thread (TestThread, 1, SIGSEGV).join(); // 1 : first thread
        std::thread (TestThread, 2, SIGSEGV).join(); // 2 : second thread
        _exit(0);
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckThreadCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest006: end.";
}

/**
 * @tc.name: SignalHandlerTest007
 * @tc.desc: test DFX_InstallSignalHandler interface
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest007: start.";
    int interestedSignalList[] = {
        SIGABRT, SIGBUS, SIGFPE,
        SIGSEGV, SIGSTKFLT, SIGSYS, SIGTRAP
    };
    for (int sig : interestedSignalList) {
        pid_t pid = fork();
        if (pid < 0) {
            GTEST_LOG_(ERROR) << "Failed to fork new test process.";
        } else if (pid == 0) {
            if (DFX_InstallSignalHandler != nullptr) {
                DFX_InstallSignalHandler();
            }
            sleep(1);
        } else {
            usleep(10000); // 10000 : sleep 10ms
            GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill << process(" << pid << ")";
            GTEST_LOG_(INFO) << "signal:" << sig;
            kill(pid, sig);
            sleep(2); // 2 : wait for cppcrash generating
            bool ret = CheckCrashKeyWords(GetCppCrashFileName(pid), pid, sig);
            ASSERT_TRUE(ret);
        }
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest007: end.";
}

int TestThread2(int threadId, int sig, int total, bool exitEarly)
{
    std::string subThreadName = "SubTestThread" + to_string(threadId);
    prctl(PR_SET_NAME, subThreadName.c_str());
    if (SetThreadInfoCallback != nullptr) {
        SetThreadInfoCallback(ThreadInfo);
    }
    if (threadId == total - 1) {
        GTEST_LOG_(INFO) << subThreadName << " is ready to raise signo(" << sig <<")";
        raise(sig);
    }

    if (!exitEarly) {
        sleep(total - threadId);
    }
    if (SetThreadInfoCallback != nullptr) {
        SetThreadInfoCallback(ThreadInfo);
    }
    return 0;
}

/**
 * @tc.name: SignalHandlerTest008
 * @tc.desc: test add 36 thread info callback and crash thread has no callback
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest008: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        std::vector<std::thread> threads;
        const int testThreadCount = 36;
        for (int i = 0; i < testThreadCount; i++) {
            threads.push_back(std::thread(TestThread2, i, SIGSEGV, testThreadCount, false));
        }

        for (auto& thread : threads) {
            thread.join();
        }
        _exit(0);
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        auto file = GetCppCrashFileName(pid);
        ASSERT_FALSE(file.empty());
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest008: end.";
}

/**
 * @tc.name: SignalHandlerTest009
 * @tc.desc: test add 36 thread info callback and crash thread has the callback
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest009, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest009: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        std::vector<std::thread> threads;
        const int testThreadCount = 36;
        for (int i = 0; i < testThreadCount; i++) {
            bool exitEarly = false;
            if (i % 2 == 0) {
                exitEarly =  true;
            }
            threads.push_back(std::thread (TestThread2, i, SIGSEGV, testThreadCount, exitEarly));
        }

        for (auto& thread : threads) {
            thread.join();
        }
        _exit(0);
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        auto file = GetCppCrashFileName(pid);
        ASSERT_FALSE(file.empty());
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest009: end.";
}

/**
 * @tc.name: SignalHandlerTest010
 * @tc.desc: test crash when free a invalid address
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest010, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest010: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        if (SetThreadInfoCallback != nullptr) {
            SetThreadInfoCallback(ThreadInfo);
        }
        int32_t freeAddr = 0x111;
        // trigger crash
        free(reinterpret_cast<void*>(freeAddr));
        // force crash if not crash in free
        abort();
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        auto file = GetCppCrashFileName(pid);
        ASSERT_FALSE(file.empty());
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest010: end.";
}

/**
 * @tc.name: SignalHandlerTest011
 * @tc.desc: test crash when realloc a invalid address
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest011, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest011: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        int32_t initAllocSz = 10;
        int32_t reallocSz = 20;
        if (SetThreadInfoCallback != nullptr) {
            SetThreadInfoCallback(ThreadInfo);
        }
        // alloc a buffer
        int8_t* addr = reinterpret_cast<int8_t*>(malloc(initAllocSz));
        // overwrite the control block
        int8_t* newAddr = addr - initAllocSz;
        (void)memset_s(newAddr, initAllocSz, 0, initAllocSz);
        addr = reinterpret_cast<int8_t*>(realloc(reinterpret_cast<void*>(addr), reallocSz));
        free(addr);
        // force crash if not crash in realloc
        abort();
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        auto file = GetCppCrashFileName(pid);
        ASSERT_FALSE(file.empty());
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest011: end.";
}

/**
 * @tc.name: SignalHandlerTest012
 * @tc.desc: test crash when realloc a invalid address without threadInfo callback
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest012, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest012: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        int32_t initAllocSz = 10;
        int32_t reallocSz = 20;
        // alloc a buffer
        int8_t* addr = reinterpret_cast<int8_t*>(malloc(initAllocSz));
        // overwrite the control block
        int8_t* newAddr = addr - initAllocSz;
        (void)memset_s(newAddr, initAllocSz, 0, initAllocSz);
        addr = reinterpret_cast<int8_t*>(realloc(reinterpret_cast<void*>(addr), reallocSz));
        free(addr);
        // force crash if not crash in realloc
        abort();
    } else {
        sleep(2); // 2 : wait for cppcrash generating
        auto file = GetCppCrashFileName(pid);
        ASSERT_FALSE(file.empty());
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest012: end.";
}

/**
 * @tc.name: SignalHandlerTest013
 * @tc.desc: test add 100 thread info callback and do nothing
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest013, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest013: start.";
    std::vector<std::thread> threads;
    const int testThreadCount = 100;
    for (int i = 0; i < testThreadCount - 1; i++) {
        threads.push_back(std::thread (TestThread2, i, SIGSEGV, testThreadCount, true));
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    auto file = GetCppCrashFileName(getpid());
    ASSERT_TRUE(file.empty());
    GTEST_LOG_(INFO) << "SignalHandlerTest013: end.";
}

void TestCallbackFunc()
{}

/**
 * @tc.name: SignalHandlerTest014
 * @tc.desc: test SetAsumcStaclCallbackFunc
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest014, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest014: start.";
    if (SetAsyncStackCallbackFunc != nullptr) {
        SetAsyncStackCallbackFunc(reinterpret_cast<void*>(TestCallbackFunc));
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest014: end.";
}

/**
 * @tc.name: SignalHandlerTest015
 * @tc.desc: test DFX_SetAppRunningUniqueId
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest015, TestSize.Level2)
{
    bool isSuccess = DFX_SetAppRunningUniqueId != nullptr;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
        return;
    }
    /**
     * @tc.steps: step1.
     *            case: appRunningId == nullptr, len= 0
     * @tc.expected: ret == -1
     * */
    int ret = DFX_SetAppRunningUniqueId(nullptr, 0);
    ASSERT_EQ(ret, -1);

    /**
     * @tc.steps: step2.
     *            case: appRunningId == nullptr, len= MAX_APP_RUNNING_UNIQUE_ID_LEN
     * @tc.expected: ret == -1
     * */
    ret = DFX_SetAppRunningUniqueId(nullptr, MAX_APP_RUNNING_UNIQUE_ID_LEN);
    ASSERT_EQ(ret, -1);

    /**
     * @tc.steps: step3.
     *            case: appRunningId != nullptr, len= 0
     * @tc.expected: ret == 0
     * */
    constexpr char testId1[] = "App running unique test id";
    ret = DFX_SetAppRunningUniqueId(testId1, 0);
    ASSERT_EQ(ret, 0);

    /**
     * @tc.steps: step4.
     *            case: appRunningId != nullptr, len= strleng(appRunningId)
     * @tc.expected: ret == 0
     * */
    ret = DFX_SetAppRunningUniqueId(testId1, strlen(testId1));
    ASSERT_EQ(ret, 0);

    /**
     * @tc.steps: step5.
     *            case: appRunningId != nullptr, len= MAX_APP_RUNNING_UNIQUE_ID_LEN + 1
     * @tc.expected: ret == -1
     * */
    constexpr size_t testLen = MAX_APP_RUNNING_UNIQUE_ID_LEN + 1;
    ret = DFX_SetAppRunningUniqueId(testId1, testLen);
    ASSERT_EQ(ret, -1);

    /**
     * @tc.steps: step6.
     *            case: appRunningId != nullptr, len= MAX_APP_RUNNING_UNIQUE_ID_LEN
     * @tc.expected: ret == 0
     * */
    constexpr char testId2[MAX_APP_RUNNING_UNIQUE_ID_LEN] = "App running unique test id";
    ret = DFX_SetAppRunningUniqueId(testId2, MAX_APP_RUNNING_UNIQUE_ID_LEN);
    ASSERT_EQ(ret, -1);
}

/**
 * @tc.name: SignalHandlerTest016
 * @tc.desc: test ReportException
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest016: start.";
    struct CrashDumpException exception;
    exception.pid = 1;
    exception.uid = 1;
    exception.error = CRASH_SIGNAL_EMASKED;
    int ret = ReportException(exception);
    ASSERT_NE(ret, -1);
    GTEST_LOG_(INFO) << "SignalHandlerTest016: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
