/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include <ctime>
#include <map>
#include <securec.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/prctl.h>
#include <sys/time.h>

#include "dfx_define.h"
#include "dfx_signal_local_handler.h"
#include "dfx_socket_request.h"
#include "dfx_test_util.h"
#include "dfx_allocator.h"
#include "dfx_crash_local_handler.h"
#include "faultloggerd_client.h"

#define MALLOC_TEST_TIMES  1000
#define MALLOC_TEST_SMALL_SIZE 16
#define MALLOC_TEST_BIG_SIZE 2000
#define MILLISEC_PER_SECOND  1000
#define FAULTLOGGERD_UID 1202

using namespace testing;
using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class LocalHandlerTest : public testing::Test {};

static bool CheckLocalCrashKeyWords(const string& filePath, pid_t pid, int sig)
{
    if (filePath.empty() || pid <= 0) {
        return false;
    }
    map<int, string> sigKey = {
        { SIGILL, string("SIGILL") },
        { SIGABRT, string("SIGABRT") },
        { SIGBUS, string("SIGBUS") },
        { SIGSEGV, string("SIGSEGV") },
    };
    string sigKeyword = "";
    map<int, string>::iterator iter = sigKey.find(sig);
    if (iter != sigKey.end()) {
        sigKeyword = iter->second;
    }
#ifdef __aarch64__
    string keywords[] = {
        "Pid:" + to_string(pid), "Uid:", "name:./test_localhandler",
        sigKeyword, "Tid:", "#00", "x0:", "test_localhandler"
    };
#else
    string keywords[] = {
        "Pid:" + to_string(pid), "Uid:", "name:./test_localhandler",
        sigKeyword, "Tid:", "#00", "test_localhandler"
    };
#endif

    int length = sizeof(keywords) / sizeof(keywords[0]);
    int minRegIdx = -1;
    return CheckKeyWords(filePath, keywords, length, minRegIdx) == length;
}

/**
 * @tc.name: LocalHandlerTest001
 * @tc.desc: test crashlocalhandler signo(SIGILL)
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest001, TestSize.Level0)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest001: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        DFX_InstallLocalSignalHandler();
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGILL);
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGILL);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "LocalHandlerTest001: end.";
}

/**
 * @tc.name: LocalHandlerTest002
 * @tc.desc: test crashlocalhandler signo(SIGABRT)
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest002: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        DFX_InstallLocalSignalHandler();
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGABRT);
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGABRT);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "LocalHandlerTest002: end.";
}

/**
 * @tc.name: LocalHandlerTest003
 * @tc.desc: test crashlocalhandler signo(SIGBUS)
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest003: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        DFX_InstallLocalSignalHandler();
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGBUS);
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGBUS);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "LocalHandlerTest003: end.";
}

/**
 * @tc.name: LocalHandlerTest004
 * @tc.desc: test crashlocalhandler signo(SIGSEGV)
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest004: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        DFX_InstallLocalSignalHandler();
        sleep(1);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to kill process(" << pid << ")";
        kill(pid, SIGSEGV);
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "LocalHandlerTest004: end.";
}

/**
 * @tc.name: LocalHandlerTest005
 * @tc.desc: test crashlocalhandler signo(SIGSEGV) by execl
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest005: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        DFX_InstallLocalSignalHandler();
        sleep(1);
        raise(SIGSEGV);
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to wait process(" << pid << ")";
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "LocalHandlerTest005: end.";
}

/**
 * @tc.name: LocalHandlerTest006
 * @tc.desc: test crashlocalhandler signo(SIGSEGV) by execl
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest006: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        siginfo_t siginfo {
            .si_signo = SIGSEGV
        };
        DFX_SignalLocalHandler(SIGSEGV, &siginfo, nullptr);
    } else {
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to wait process(" << pid << ")";
        sleep(2); // 2 : wait for cppcrash generating
        ASSERT_TRUE(CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV));
    }
    GTEST_LOG_(INFO) << "LocalHandlerTest006: end.";
}

/**
 * @tc.name: LocalHandlerTest007
 * @tc.desc: test crashlocalhandler signo(SIGSEGV) by execl
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest007: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        siginfo_t siginfo {
            .si_signo = SIGSEGV
        };
        DFX_GetCrashFdFunc([](const struct ProcessDumpRequest* request)
            {return RequestFileDescriptor((int)FaultLoggerType::CPP_CRASH);});
        DFX_SignalLocalHandler(SIGSEGV, &siginfo, nullptr);
    } else {
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to wait process(" << pid << ")";
        sleep(2); // 2 : wait for cppcrash generating
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV);
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "LocalHandlerTest007: end.";
}

/**
 * @tc.name: LocalHandlerTest008
 * @tc.desc: test crashlocalhandler signo(SIGSEGV) by execl
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, LocalHandlerTest008, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "LocalHandlerTest008: start.";
    struct ProcessDumpRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    request.type = DUMP_TYPE_CPP_CRASH;
    request.tid = gettid();
    request.pid = getpid();
    request.uid = FAULTLOGGERD_UID;
    struct timespec ts;
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    request.timeStamp = ((uint64_t)ts.tv_sec * MILLISEC_PER_SECOND) +
        (((uint64_t)ts.tv_nsec) / (MILLISEC_PER_SECOND * MILLISEC_PER_SECOND));
    CrashLocalHandler(&request);
    std::string filename = GetCppCrashFileName(request.pid);
    ASSERT_FALSE(filename.empty());
    GTEST_LOG_(INFO) << "LocalHandlerTest008: end.";
}

/**
 * @tc.name: DfxAllocatorTest001
 * @tc.desc: test dfxAllocator isDfxAllocatorMem
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, DfxAllocatorTest001, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxAllocatorTest001: start.";
    void* p = malloc(MALLOC_TEST_SMALL_SIZE);
    int ret = 0;
    if (p) {
        ret = IsDfxAllocatorMem(p);
        free(p);
    }
    ASSERT_TRUE(ret == 0);
    GTEST_LOG_(INFO) << "DfxAllocatorTest001: end.";
}

/**
 * @tc.name: DfxAllocatorTest002
 * @tc.desc: test dfxAllocator malloc and free
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, DfxAllocatorTest002, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxAllocatorTest002: start.";
    void* p = nullptr;
    void* parr[MALLOC_TEST_TIMES] = {nullptr};
    uint32_t size = MALLOC_TEST_SMALL_SIZE;
    int res = 0;
    DfxAllocator* allocator = GetDfxAllocator();
    RegisterAllocator();
    for (int i = 0; i < DFX_MEMPOOLS_NUM; i++) {
        // malloc and free 1000 times
        for (int time = 0; time < MALLOC_TEST_TIMES; time++) {
            p = malloc(size);
            if (p == nullptr || !IsDfxAllocatorMem(p)) {
                res = 1;
            }
            free(p);
            if (allocator->dfxMempoolBuf[i].pageList != nullptr) {
                res = 1;
            }
        }
        // malloc 1000 times and free 1000 times
        for (int time = 0; time < MALLOC_TEST_TIMES; time++) {
            parr[time] = malloc(size);
            if (parr[time] == nullptr || !IsDfxAllocatorMem(parr[time])) {
                res = 1;
            }
        }
        for (int time = 0; time < MALLOC_TEST_TIMES; time++) {
            free(parr[time]);
        }
        if (allocator->dfxMempoolBuf[i].pageList != nullptr) {
            res = 1;
        }
        size = size << 1;
    }
    UnregisterAllocator();
    ASSERT_TRUE(res == 0);
    GTEST_LOG_(INFO) << "DfxAllocatorTest002: end.";
}

/**
 * @tc.name: DfxAllocatorTest003
 * @tc.desc: test dfxAllocator mmap
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, DfxAllocatorTest003, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxAllocatorTest003: start.";
    void* p = nullptr;
    void* parr[MALLOC_TEST_TIMES] = {nullptr};
    uint32_t size = MALLOC_TEST_BIG_SIZE;
    int res = 0;
    DfxAllocator* allocator = GetDfxAllocator();
    RegisterAllocator();
    // malloc and free 1000 times
    for (int time = 0; time < MALLOC_TEST_TIMES; time++) {
        p = malloc(size);
        if (p == nullptr || !IsDfxAllocatorMem(p)) {
            res = 1;
        }
        free(p);
        if (allocator->pageList != nullptr) {
            res = 1;
        }
    }
    // malloc 1000 times and free 1000 times
    for (int time = 0; time < MALLOC_TEST_TIMES; time++) {
        parr[time] = malloc(size);
        if (parr[time] == nullptr || !IsDfxAllocatorMem(parr[time])) {
            res = 1;
        }
    }
    for (int time = 0; time < MALLOC_TEST_TIMES; time++) {
        free(parr[time]);
    }
    if (allocator->pageList != nullptr) {
        res = 1;
    }
    UnregisterAllocator();
    ASSERT_TRUE(res == 0);
    GTEST_LOG_(INFO) << "DfxAllocatorTest003: end.";
}

/**
 * @tc.name: DfxAllocatorTest004
 * @tc.desc: test dfxAllocator realloc
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, DfxAllocatorTest004, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxAllocatorTest004: start.";
    void* p = nullptr;
    uint32_t size = MALLOC_TEST_SMALL_SIZE;
    int res = 0;
    DfxAllocator* allocator = GetDfxAllocator();
    RegisterAllocator();
    p = malloc(size);
    (void)memset_s(p, size, 0, size);
    size += MALLOC_TEST_SMALL_SIZE;
    p = realloc(p, size);
    if (p == nullptr || !IsDfxAllocatorMem(p)) {
        res = 1;
    }
    if (allocator->dfxMempoolBuf[1].pageList == nullptr) {
        res = 1;
    }
    free(p);
    UnregisterAllocator();
    ASSERT_TRUE(res == 0);
    GTEST_LOG_(INFO) << "DfxAllocatorTest004: end.";
}

/**
 * @tc.name: DfxAllocatorTest005
 * @tc.desc: test dfxAllocator localhandler crash log
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, DfxAllocatorTest005, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxAllocatorTest005: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        DFX_InstallLocalSignalHandler();
        sleep(1);
        // alloc a buffer
        int32_t initAllocSz = 10;
        int32_t reallocSz = 20;
        int8_t* addr = reinterpret_cast<int8_t*>(malloc(initAllocSz));
        // overwrite the control block
        int8_t* newAddr = addr - initAllocSz;
        (void)memset_s(newAddr, initAllocSz, 0, initAllocSz);
        addr = reinterpret_cast<int8_t*>(realloc(reinterpret_cast<void*>(addr), reallocSz));
        free(addr);
        // force crash if not crash in realloc
        abort();
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to wait process(" << pid << ")";
        sleep(2); // 2 : wait for cppcrash generating
#ifdef __aarch64__
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGABRT);
#else
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV);
#endif
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "DfxAllocatorTest005: end.";
}

/**
 * @tc.name: DfxAllocatorTest006
 * @tc.desc: test dfxAllocator localhandler abnormal scenario
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, DfxAllocatorTest006, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxAllocatorTest005: start.";
    int ret = IsDfxAllocatorMem(nullptr);
    ASSERT_EQ(ret, 0);
    GTEST_LOG_(INFO) << "DfxAllocatorTest005: end.";
}

/**
 * @tc.name: DfxAllocatorTest007
 * @tc.desc: test dfxAllocator localhandler crash log
 * @tc.type: FUNC
 */
HWTEST_F(LocalHandlerTest, DfxAllocatorTest007, TestSize.Level2)
{
    GTEST_LOG_(INFO) << "DfxAllocatorTest007: start.";
    pid_t pid = fork();
    if (pid < 0) {
        GTEST_LOG_(ERROR) << "Failed to fork new test process.";
    } else if (pid == 0) {
        sleep(1);
        // alloc a buffer
        int32_t initAllocSz = 10;
        int32_t reallocSz = 20;
        int8_t* addr = reinterpret_cast<int8_t*>(malloc(initAllocSz));
        // overwrite the control block
        int8_t* newAddr = addr - initAllocSz;
        (void)memset_s(newAddr, initAllocSz, 0, initAllocSz);
        addr = reinterpret_cast<int8_t*>(realloc(reinterpret_cast<void*>(addr), reallocSz));
        free(addr);
        // force crash if not crash in realloc
        abort();
    } else {
        usleep(10000); // 10000 : sleep 10ms
        GTEST_LOG_(INFO) << "process(" << getpid() << ") is ready to wait process(" << pid << ")";
        sleep(2); // 2 : wait for cppcrash generating
#ifdef __aarch64__
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGABRT);
#else
        bool ret = CheckLocalCrashKeyWords(GetCppCrashFileName(pid), pid, SIGSEGV);
#endif
        ASSERT_TRUE(ret);
    }
    GTEST_LOG_(INFO) << "DfxAllocatorTest007: end.";
}
} // namespace HiviewDFX
} // namepsace OHOS
