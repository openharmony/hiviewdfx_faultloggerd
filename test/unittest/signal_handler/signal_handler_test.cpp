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
#include <malloc.h>
#include <fcntl.h>
#include <securec.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>

#include "dfx_define.h"
#include "dfx_cutil.h"
#include "dfx_dumprequest.h"
#include "dfx_signal_handler.h"
#include "dfx_signalhandler_exception.h"
#include "dfx_test_util.h"
#include "info/fatal_message.h"
#include "dfx_lite_dump_request.h"
#include "safe_reader.h"

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
int g_pipeFd[] = {-1, -1};
class SignalHandlerTest : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
};

void SignalHandlerTest::SetUp()
{
    pipe(g_pipeFd);
    int newSize = 1024 * 1024; // 1MB
    fcntl(g_pipeFd[PIPE_WRITE], F_SETPIPE_SZ, newSize);
}

void SignalHandlerTest::TearDown()
{
    ClosePipeFd(g_pipeFd[PIPE_WRITE]);
    ClosePipeFd(g_pipeFd[PIPE_READ]);
}

static bool CheckCallbackCrashKeyWords(const string& filePath, pid_t pid, int sig)
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

static bool CheckDebugSignalFaultlog(const string& filePath, pid_t pid, int siCode)
{
    if (filePath.empty() || pid <= 0) {
        return false;
    }
    std::list<LineRule> rules;
    rules.push_back(LineRule(R"(^Build info:.*$)"));
    rules.push_back(LineRule(R"(^Process name:./test_signalhandler$)"));
    rules.push_back(LineRule(R"(^Process life time:.*$)"));
    rules.push_back(LineRule("^Pid:" + to_string(pid) + "$"));
    rules.push_back(LineRule(R"(^Uid:\d+$)"));
    rules.push_back(LineRule(R"(^Reason:Signal:DEBUG SIGNAL\((FDSAN|JEMALLOC|BADFD)\).*$)"));
    rules.push_back(LineRule(R"(^#\d+ pc [0-9a-f]+ .*$)", 5)); // match 5 times
    rules.push_back(LineRule(R"(^Registers:$)"));
    rules.push_back(LineRule(R"(^FaultStack:$)"));
    rules.push_back(LineRule(R"(^Maps:$)"));
    if (abs(siCode) != SIGLEAK_STACK_BADFD) {
        rules.push_back(LineRule(R"(^LastFatalMessage:.*$)"));
    }
    if (abs(siCode) != SIGLEAK_STACK_JEMALLOC) {
        rules.push_back(LineRule(R"(^OpenFiles:$)"));
        rules.push_back(LineRule(R"(^\d+->.*$)", 5)); // match 5 times
    }
    return CheckLineMatch(filePath, rules);
}

void ThreadInfo(char* buf, size_t len, void* context __attribute__((unused)))
{
    char mes[] = "this is extraCrashInfo of test thread";
    if (memcpy_s(buf, len, mes, sizeof(mes)) != 0) {
        GTEST_LOG_(INFO) << "Failed to set thread info";
    }
}

int TestThread(int threadId, int sig)
{
    std::string subThreadName = "SubTestThread" + to_string(threadId);
    prctl(PR_SET_NAME, subThreadName.c_str());
    SetThreadInfoCallback(ThreadInfo);
    int cashThreadId = 2;
    if (threadId == cashThreadId) {
        GTEST_LOG_(INFO) << subThreadName << " is ready to raise signo(" << sig <<")";
        raise(sig);
    }
    return 0;
}

static void SaveDebugMessage(int siCode, int64_t diffMs, const char *msg)
{
    const int signo = 42; // Custom stack capture signal and leak reuse
    siginfo_t info;
    info.si_signo = signo;
    info.si_code = siCode;

    debug_msg_t debug_message = {0, NULL};
    if (msg != nullptr) {
        const int numberOneThousand = 1000; // 1000 : second to millisecond convert ratio
        const int numberOneMillion = 1000000; // 1000000 : nanosecond to millisecond convert ratio
        struct timespec ts;
        (void)clock_gettime(CLOCK_REALTIME, &ts);

        uint64_t timestamp = static_cast<uint64_t>(ts.tv_sec) * numberOneThousand +
            static_cast<uint64_t>(ts.tv_sec) / numberOneMillion;
        if (diffMs < 0  && timestamp < -diffMs) {
            timestamp = 0;
        } else if (UINT64_MAX - timestamp < diffMs) {
            timestamp = UINT64_MAX;
        } else {
            timestamp += diffMs;
        }

        debug_message.timestamp = timestamp;
        debug_message.msg = msg;

        info.si_value.sival_ptr = &debug_message;
    }

    if (syscall(SYS_rt_tgsigqueueinfo, getpid(), syscall(SYS_gettid), signo, &info) == -1) {
        GTEST_LOG_(ERROR) << "send failed errno=" << errno;
    }
}

static bool SendSigTestDebugSignal(int siCode)
{
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
        return false;
    }

    if (pid == 0) {
        SaveDebugMessage(siCode, 0, "test123");
        sleep(5); // 5: wait for stacktrace generating
        _exit(0);
    }

    sleep(2); // 2 : wait for cppcrash generating
    return CheckDebugSignalFaultlog(GetDumpLogFileName("stacktrace", pid, TEMP_DIR), pid, siCode);
}

static void TestFdsan()
{
    fdsan_set_error_level(FDSAN_ERROR_LEVEL_WARN_ONCE);
    FILE *fp = fopen("/dev/null", "w+");
    if (fp == nullptr) {
        GTEST_LOG_(ERROR) << "fp is nullptr";
        return;
    }
    close(fileno(fp));
    uint64_t tag = fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, reinterpret_cast<uint64_t>(fp));
    fdsan_exchange_owner_tag(fileno(fp), tag, 0);
    return;
}

void TestBadfdThread(int maxCnt)
{
    for (int i = 0; i < maxCnt; i++) {
        sleep(2); // Delay 2s waiting for the next triggerable cycle
        SaveDebugMessage(-SIGLEAK_STACK_BADFD, 0, nullptr);
    }
}

/**
 * @tc.name: SignalHandlerTest001
 * @tc.desc: test thread cash SignalHandler signo(SIGILL)
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest001: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        SetThreadInfoCallback(ThreadInfo);
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGILL);
        auto filename = WaitCreateCrashFile("cppcrash", pid);
        bool ret = CheckCallbackCrashKeyWords(filename, pid, SIGILL);
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
        SetThreadInfoCallback(ThreadInfo);
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGBUS);
        auto filename = WaitCreateCrashFile("cppcrash", pid);
        bool ret = CheckCallbackCrashKeyWords(filename, pid, SIGBUS);
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
        SetThreadInfoCallback(ThreadInfo);
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGSEGV);
        auto filename = WaitCreateCrashFile("cppcrash", pid);
        bool ret = CheckCallbackCrashKeyWords(filename, pid, SIGSEGV);
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
        auto filename = WaitCreateCrashFile("cppcrash", pid);
        bool ret = CheckCallbackCrashKeyWords(filename, pid, SIGILL);
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
        auto filename = WaitCreateCrashFile("cppcrash", pid);
        bool ret = CheckCallbackCrashKeyWords(filename, pid, SIGBUS);
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
        auto filename = WaitCreateCrashFile("cppcrash", pid);
        bool ret = CheckCallbackCrashKeyWords(filename, pid, SIGSEGV);
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
            sleep(1);
        } else {
            usleep(10000); // 10000 : sleep 10ms
            GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill << process(" << pid << ")";
            GTEST_LOG_(INFO) << "signal:" << sig;
            kill(pid, sig);
            auto filename = WaitCreateCrashFile("cppcrash", pid);
            bool ret = CheckCrashKeyWords(filename, pid, sig);
            ASSERT_TRUE(ret);
        }
    }
    GTEST_LOG_(INFO) << "SignalHandlerTest007: end.";
}

int TestThread2(int threadId, int sig, int total, bool exitEarly)
{
    std::string subThreadName = "SubTestThread" + to_string(threadId);
    prctl(PR_SET_NAME, subThreadName.c_str());
    SetThreadInfoCallback(ThreadInfo);

    if (threadId == total - 1) {
        GTEST_LOG_(INFO) << subThreadName << " is ready to raise signo(" << sig <<")";
        raise(sig);
    }

    if (!exitEarly) {
        sleep(total - threadId);
    }
    SetThreadInfoCallback(ThreadInfo);

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
        auto file = WaitCreateCrashFile("cppcrash", pid);
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
        auto file = WaitCreateCrashFile("cppcrash", pid);
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
        SetThreadInfoCallback(ThreadInfo);
        int32_t freeAddr = 0x111;
        // trigger crash
        free(reinterpret_cast<void*>(freeAddr));
        // force crash if not crash in free
        abort();
    } else {
        auto file = WaitCreateCrashFile("cppcrash", pid);
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
        SetThreadInfoCallback(ThreadInfo);
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
        auto file = WaitCreateCrashFile("cppcrash", pid);
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
        auto file = WaitCreateCrashFile("cppcrash", pid);
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

uint64_t TestCallbackFunc()
{
    return 0;
}

/**
 * @tc.name: SignalHandlerTest015
 * @tc.desc: test DFX_SetAppRunningUniqueId
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest015, TestSize.Level2)
{
    bool isSuccess = true;
    if (!isSuccess) {
        ASSERT_FALSE(isSuccess);
    } else {
        /**
         * @tc.steps: step1.
         *            case: appRunningUniqueId == nullptr, len= 0
         * @tc.expected: ret == -1
         * */
        int ret = DFX_SetAppRunningUniqueId(nullptr, 0);
        ASSERT_EQ(ret, -1);

        /**
         * @tc.steps: step2.
         *            case: appRunningUniqueId == nullptr, len= MAX_APP_RUNNING_UNIQUE_ID_LEN
         * @tc.expected: ret == -1
         * */
        ret = DFX_SetAppRunningUniqueId(nullptr, MAX_APP_RUNNING_UNIQUE_ID_LEN);
        ASSERT_EQ(ret, -1);

        /**
         * @tc.steps: step3.
         *            case: appRunningUniqueId != nullptr, len= 0
         * @tc.expected: ret == 0
         * */
        constexpr char testId1[] = "App running unique test id";
        ret = DFX_SetAppRunningUniqueId(testId1, 0);
        ASSERT_EQ(ret, 0);

        /**
         * @tc.steps: step4.
         *            case: appRunningUniqueId != nullptr, len= strleng(appRunningUniqueId)
         * @tc.expected: ret == 0
         * */
        ret = DFX_SetAppRunningUniqueId(testId1, strlen(testId1));
        ASSERT_EQ(ret, 0);

        /**
         * @tc.steps: step5.
         *            case: appRunningUniqueId != nullptr, len= MAX_APP_RUNNING_UNIQUE_ID_LEN + 1
         * @tc.expected: ret == -1
         * */
        constexpr size_t testLen = MAX_APP_RUNNING_UNIQUE_ID_LEN + 1;
        ret = DFX_SetAppRunningUniqueId(testId1, testLen);
        ASSERT_EQ(ret, -1);

        /**
         * @tc.steps: step6.
         *            case: appRunningUniqueId != nullptr, len= MAX_APP_RUNNING_UNIQUE_ID_LEN
         * @tc.expected: ret == 0
         * */
        constexpr char testId2[MAX_APP_RUNNING_UNIQUE_ID_LEN] = "App running unique test id";
        ret = DFX_SetAppRunningUniqueId(testId2, MAX_APP_RUNNING_UNIQUE_ID_LEN);
        ASSERT_EQ(ret, -1);
    }
}

/**
 * @tc.name: SignalHandlerTest016
 * @tc.desc: test ReportException
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest016, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "SignalHandlerTest016: start.";
    DFX_SetAsyncStackCallback(TestCallbackFunc);

    struct CrashDumpException exception;
    exception.pid = 1;
    exception.uid = 1;
    exception.error = CRASH_SIGNAL_EMASKED;
    int ret = ReportException(&exception);
    ASSERT_NE(ret, -1);
    GTEST_LOG_(INFO) << "SignalHandlerTest016: end.";
}

/**
 * @tc.name: SignalHandlerTest017
 * @tc.desc: send sig SIGLEAK_STACK_FDSAN
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest017, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        bool ret = SendSigTestDebugSignal(-SIGLEAK_STACK_FDSAN);
        ASSERT_TRUE(ret);
    }
}

/**
 * @tc.name: SignalHandlerTest018
 * @tc.desc: send sig SIGLEAK_STACK_JEMALLOC
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest018, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        bool ret = SendSigTestDebugSignal(-SIGLEAK_STACK_JEMALLOC);
        ASSERT_TRUE(ret);
    }
}

/**
 * @tc.name: SignalHandlerTest019
 * @tc.desc: send sig SIGLEAK_STACK_BADFD
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest019, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        bool ret = SendSigTestDebugSignal(-SIGLEAK_STACK_BADFD);
        ASSERT_TRUE(ret);
    }
}

/**
 * @tc.name: SignalHandlerTest020
 * @tc.desc: test DEBUG SIGNAL time out, BADFD no timeout
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest020, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        int interestedSiCodeList[] = {
            -SIGLEAK_STACK_FDSAN, -SIGLEAK_STACK_JEMALLOC
        };
        for (int siCode : interestedSiCodeList) {
            pid_t pid = fork();
            if (pid < 0) {
                GTEST_LOG_(ERROR) << "Failed to fork new test process.";
            } else if (pid == 0) {
                constexpr int diffMs = -10000; // 10s
                SaveDebugMessage(siCode, diffMs, "test123");
                sleep(5); // 5: wait for stacktrace generating
                _exit(0);
            } else {
                auto fileName = WaitCreateCrashFile("stacktrace", pid);
                ASSERT_TRUE(fileName.empty());
            }
        }
    }
}

/**
 * @tc.name: SignalHandlerTest021
 * @tc.desc: test FDSAN
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest021, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        pid_t pid = fork();
        if (pid < 0) {
            GTEST_LOG_(ERROR) << "Failed to fork new test process.";
        } else if (pid == 0) {
            TestFdsan();
            sleep(5); // 5: wait for stacktrace generating
            _exit(0);
        } else {
            constexpr int siCode = -SIGLEAK_STACK_FDSAN;
            auto fileName = WaitCreateCrashFile("stacktrace", pid);
            bool ret = CheckDebugSignalFaultlog(fileName, pid, siCode);
            ASSERT_TRUE(ret);
        }
    }
}

/**
 * @tc.name: SignalHandlerTest022
 * @tc.desc: test BADFD
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest022, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        pid_t pid = fork();
        if (pid < 0) {
            GTEST_LOG_(ERROR) << "Failed to fork new test process.";
        } else if (pid == 0) {
            SaveDebugMessage(-SIGLEAK_STACK_BADFD, 0, nullptr);
            sleep(5); // 5: wait for stacktrace generating
            _exit(0);
        } else {
            constexpr int siCode = -SIGLEAK_STACK_BADFD;
            auto fileName = WaitCreateCrashFile("stacktrace", pid);
            bool ret = CheckDebugSignalFaultlog(fileName, pid, siCode);
            ASSERT_TRUE(ret);
        }
    }
}

/**
 * @tc.name: SignalHandlerTest023
 * @tc.desc: test BADFD
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest023, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        constexpr int maxCnt = 3; // Run the test 3 times
        std::thread testBadfdThread(TestBadfdThread, maxCnt);
        for (int i = 0; i < maxCnt; i++) {
            auto fileName = WaitCreateCrashFile("stacktrace", getpid());
            ASSERT_TRUE(!fileName.empty());
        }
        testBadfdThread.join();
    }
}

/**
 * @tc.name: SignalHandlerTest024
 * @tc.desc: test DFX_GetAppRunningUniqueId
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SignalHandlerTest024, TestSize.Level2)
{
    constexpr char testId[] = "App running unique id";
    int ret = DFX_SetAppRunningUniqueId(testId, strlen(testId));
    ASSERT_EQ(ret, 0);
    const char* runningId = DFX_GetAppRunningUniqueId();
    ASSERT_STREQ(runningId, testId);
}

/**
 * @tc.name: FdTableTest001
 * @tc.desc: Verify the fdtable structure
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, FdTableTest001, TestSize.Level2)
{
    std::string res = ExecuteCommands("uname");
    bool linuxKernel = res.find("Linux") != std::string::npos;
    if (linuxKernel) {
        ASSERT_TRUE(linuxKernel);
    } else {
        /**
         * @tc.steps: step1. open 128 fd files
         *            case: open success
         * @tc.expected: fp != nullptr
         * */
        constexpr int maxOpenNum = 128; // open fd table
        FILE* fdList[maxOpenNum] = {nullptr};
        for (int i = 0; i < maxOpenNum; i++) {
            fdList[i] = fopen("/dev/null", "r");
            ASSERT_NE(fdList[i], nullptr);
        }

        /**
         * @tc.steps: step2. open test fd
         *            case: open success
         * @tc.expected: fp != nullptr
         * */
        FILE *fp = fopen("/dev/null", "r");
        ASSERT_NE(fp, nullptr);

        /**
         * @tc.steps: step3. Clean up resources
         * */
        for (int i = 0; i < maxOpenNum; i++) {
            if (fdList[i] != nullptr) {
                fclose(fdList[i]);
                fdList[i] = nullptr;
            }
        }

        /**
         * @tc.steps: step4. Get fd tag
         * */
        uint64_t ownerTag = fdsan_get_owner_tag(fileno(fp));
        uint64_t tag = fdsan_get_tag_value(ownerTag);

        pid_t pid = fork();
        if (pid < 0) {
            GTEST_LOG_(ERROR) << "Failed to fork new process.";
        } else if (pid == 0) {
            // The child process has disabled fdsan detection by default
            fdsan_set_error_level(FDSAN_ERROR_LEVEL_WARN_ONCE);
            /**
             * @tc.steps: step5. Trigger fdsan
             * */
            close(fileno(fp));
        } else {
            /**
             * @tc.steps: step6. Waiting for the completion of stack grabbing
             * */
            sleep(3); // 3 : sleep 3 seconds

            string keywords[] = {
                to_string(fileno(fp)) + "->/dev/null", to_string(tag)
            };
            fclose(fp);

            /**
             * @tc.steps: step7. Check key words
             * @tc.expected: check success
             * */
            auto filePath = GetDumpLogFileName("stacktrace", pid, TEMP_DIR);
            ASSERT_FALSE(filePath.empty());

            int length = sizeof(keywords) / sizeof(keywords[0]);
            int minRegIdx = -1;
            ASSERT_EQ(CheckKeyWords(filePath, keywords, length, minRegIdx), length);
        }
    }
}

/**
 * @tc.name: SetCrashLogConfig001
 * @tc.desc: Verify the set crash Log config
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SetCrashLogConfig001, TestSize.Level2)
{
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new process.";
    } else if (pid == 0) {
        ASSERT_EQ(DFX_SetCrashLogConfig(0, 1), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(1, 10000), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(2, 1), 0);
        abort();
    } else {
        sleep(3); // 3 : sleep 3 seconds
        string keywords[] = {
            "configs:",
            "printing:true",
            "size:10000B",
            "printing:true",
        };
        auto filePath = GetDumpLogFileName("cppcrash", pid, TEMP_DIR);
        ASSERT_FALSE(filePath.empty());

        int length = sizeof(keywords) / sizeof(keywords[0]);
        int minRegIdx = -1;
        ASSERT_EQ(CheckKeyWords(filePath, keywords, length, minRegIdx), length);
    }
}

/**
 * @tc.name: SetCrashLogConfig002
 * @tc.desc: Verify the set crash Log config twice
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SetCrashLogConfig002, TestSize.Level2)
{
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new process.";
    } else if (pid == 0) {
        // once
        ASSERT_EQ(DFX_SetCrashLogConfig(0, 1), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(1, 10000), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(2, 1), 0);
        // twice
        ASSERT_EQ(DFX_SetCrashLogConfig(0, 0), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(1, 9999), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(2, 1), 0);
        abort();
    } else {
        sleep(3); // 3 : sleep 3 seconds
        string keywords[] = {
            "configs:",
            "size:9999B",
            "printing:true",
        };
        auto filePath = GetDumpLogFileName("cppcrash", pid, TEMP_DIR);
        ASSERT_FALSE(filePath.empty());

        int length = sizeof(keywords) / sizeof(keywords[0]);
        int minRegIdx = -1;
        ASSERT_EQ(CheckKeyWords(filePath, keywords, length, minRegIdx), length);
    }
}

/**
 * @tc.name: SetCrashLogConfig003
 * @tc.desc: Verify the set crash Log config three times
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, SetCrashLogConfig003, TestSize.Level2)
{
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new process.";
    } else if (pid == 0) {
        // once
        ASSERT_EQ(DFX_SetCrashLogConfig(0, 1), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(1, 10000), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(2, 1), 0);
        // twice
        ASSERT_EQ(DFX_SetCrashLogConfig(0, 0), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(1, 9999), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(2, 1), 0);
        // three times
        ASSERT_EQ(DFX_SetCrashLogConfig(0, 1), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(1, 1024), 0);
        ASSERT_EQ(DFX_SetCrashLogConfig(2, 0), 0);
        abort();
    } else {
        sleep(3); // 3 : sleep 3 seconds
        string keywords[] = {
            "configs:",
            "printing:true",
            "size:1024B",
        };
        auto filePath = GetDumpLogFileName("cppcrash", pid, TEMP_DIR);
        ASSERT_FALSE(filePath.empty());

        int length = sizeof(keywords) / sizeof(keywords[0]);
        int minRegIdx = -1;
        ASSERT_EQ(CheckKeyWords(filePath, keywords, length, minRegIdx), length);
    }
}

/**
 * @tc.name: DfxNotifyWatchdogThreadStart Test
 * @tc.desc: add testcase DfxNotifyWatchdogThreadStart
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxNotifyWatchdogThreadStart001, TestSize.Level2)
{
    EXPECT_TRUE(DfxNotifyWatchdogThreadStart() == 0);
}

/**
 * @tc.name: DfxLiteDumperTest001
 * @tc.desc: add testcase liteDump mmap success
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxLiteDumperTest001, TestSize.Level2)
{
    EXPECT_EQ(GetProcId(nullptr, "Pid:"), -1);
    EXPECT_EQ(GetProcId(PROC_SELF_STATUS_PATH, NULL), -1);
    EXPECT_GT(GetProcId(PROC_SELF_STATUS_PATH, "Pid:"), 0);
    EXPECT_GT(GetProcId("/proc/thread-self/status", "Pid:"), 0);

    UpdateSanBoxProcess(nullptr);
    EXPECT_TRUE(MMapMemoryOnce());
    ProcessDumpRequest request {};
    UpdateSanBoxProcess(&request);
    request.pid = getpid();
    EXPECT_TRUE(CollectStat(&request));
    EXPECT_TRUE(CollectStatm(&request));
    EXPECT_TRUE(CollectStack(&request));

    EXPECT_FALSE(CollectStat(&request));
    EXPECT_FALSE(CollectStatm(&request));

    ResetLiteDump();
    request.pid = 99999;
    EXPECT_FALSE(CollectStat(&request));
    EXPECT_FALSE(CollectStatm(&request));
    EXPECT_TRUE(CollectStack(&request));
    UnmapMemoryOnce();
}

/**
 * @tc.name: DfxLiteDumperTest002
 * @tc.desc: add testcase liteDump not map
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxLiteDumperTest002, TestSize.Level2)
{
    ProcessDumpRequest request {};
    UpdateSanBoxProcess(&request);
    request.pid = getpid();
    EXPECT_FALSE(CollectStat(&request));
    EXPECT_FALSE(CollectStatm(&request));
    EXPECT_FALSE(CollectStack(&request));
    UnmapMemoryOnce();
}

/**
 * @tc.name: DfxLiteDumperTest003
 * @tc.desc: add testcase liteDump lite carsh handler
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxLiteDumperTest003, TestSize.Level2)
{
    ProcessDumpRequest request {};
    request.pid = getpid();
    request.uid = 200000;
    EXPECT_FALSE(LiteCrashHandler(&request));
}

#if defined(__aarch64__)
/**
 * @tc.name: DfxLiteDumperTest004
 * @tc.desc: add test case liteDump NearRegisters
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxLiteDumperTest004, TestSize.Level2)
{
    ProcessDumpRequest request {};
    request.pid = getpid();
    EXPECT_TRUE(CollectMemoryNearRegisters(g_pipeFd[PIPE_WRITE], &request.context));
    ClosePipeFd(g_pipeFd[PIPE_WRITE]);
    constexpr int totalSize = 50 * 2 + 32 * 31 * 8 + 64 * 2 * 8;
    std::vector<uint8_t> vec(totalSize);
    EXPECT_EQ(read(g_pipeFd[PIPE_READ], vec.data(), vec.size()), totalSize);
}
#endif

/**
 * @tc.name: DfxLiteDumperTest005
 * @tc.desc: add test case liteDump maps
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxLiteDumperTest005, TestSize.Level2)
{
    EXPECT_FALSE(CollectMaps(-1, PROC_SELF_MAPS_PATH));
    EXPECT_FALSE(CollectMaps(-1, nullptr));
    EXPECT_FALSE(CollectMaps(g_pipeFd[PIPE_WRITE], "/proc/99999/maps"));
    EXPECT_TRUE(CollectMaps(g_pipeFd[PIPE_WRITE], PROC_SELF_MAPS_PATH)); // not contain MapsTag
    ClosePipeFd(g_pipeFd[PIPE_WRITE]);
    std::string str;
    char buf[LINE_BUF_SIZE];
    while (read(g_pipeFd[PIPE_READ], buf, sizeof(buf)) > 0) {
        str += buf;
    }
}

/**
 * @tc.name: DfxLiteDumperTest006
 * @tc.desc: add test case liteDump open files
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxLiteDumperTest006, TestSize.Level2)
{
    char buf[LINE_BUF_SIZE];
    EXPECT_TRUE(CollectOpenFiles(g_pipeFd[PIPE_WRITE], (uint64_t)fdsan_get_fd_table()));
    ClosePipeFd(g_pipeFd[PIPE_WRITE]);
    std::string str;
    while (read(g_pipeFd[PIPE_READ], buf, sizeof(buf)) > 0) {
        str += buf;
    }
    EXPECT_TRUE(str.find("OpenFiles") != std::string::npos);
    DeInitPipe();
}

/**
 * @tc.name: DfxLiteDumperTest007
 * @tc.desc: add test case liteDump DumpPrviProcess
 * @tc.type: FUNC
 */
HWTEST_F(SignalHandlerTest, DfxLiteDumperTest007, TestSize.Level2)
{
    ProcessDumpRequest request;
    int signo = 11;
    EXPECT_FALSE(DumpPrviProcess(signo, nullptr));
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    request.pid = getpid();
    GetProcessName(request.processName, sizeof(request.processName));
    EXPECT_TRUE(DumpPrviProcess(signo, &request));
}
} // namespace HiviewDFX
} // namepsace OHOS
