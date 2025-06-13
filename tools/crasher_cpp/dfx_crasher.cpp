/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "dfx_crasher.h"

#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <hilog/log.h>
#include <info/fatal_message.h>
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
#include <vector>

#ifndef is_ohos_lite
#include "async_stack.h"
#endif
#include "dfx_crash.h"
#include "dfx_define.h"
#ifndef is_ohos_lite
#include "ffrt_inner.h"
#include "uv.h"
#endif // !is_ohos_lite

#include "info/fatal_message.h"
#include "securec.h"

#ifdef HAS_HITRACE
#include <hitrace/hitracechain.h>
#endif

#ifdef HAS_CRASH_EXAMPLES
#include "faults/nullpointer_dereference.h"
#include "faults/multi_thread_container_access.h"
#include "faults/ipc_issues.h"
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Unwind"
#endif

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

static const int CMD_SZ = 32;
static const int CMD_DESC_SZ = 128;

static const int NUMBER_TWO = 2;
static const int NUMBER_ONE = 1;

using namespace OHOS::HiviewDFX;
using CommandFunc = int(*)();
using CommandFuncParam = int(*)(const std::string &);
struct CrasherCommandLine {
    char cmdline[CMD_SZ];
    char description[CMD_DESC_SZ];
    CommandFunc func;
};

struct CrasherCommandLineParam {
    char cmdline[CMD_SZ];
    char description[CMD_DESC_SZ];
    CommandFuncParam func;
};

constexpr static CrasherCommandLine CMDLINE_TABLE[] = {
    {"SIGFPE", "raise a SIGFPE", &DfxCrasher::RaiseFloatingPointException},
    {"SIGILL", "raise a SIGILL", &DfxCrasher::RaiseIllegalInstructionException},
    {"SIGSEGV", "raise a SIGSEGV", &DfxCrasher::RaiseSegmentFaultException},
    {"SIGTRAP", "raise a SIGTRAP", &DfxCrasher::RaiseTrapException},
    {"SIGABRT", "raise a SIGABRT", &DfxCrasher::RaiseAbort},
    {"SetLastFatalMessage", "raise a SIGABRT", &DfxCrasher::SetLastFatalMessage},
    {"SIGBUS", "raise a SIGBUS", &DfxCrasher::RaiseBusError},

    {"triSIGILL", "trigger a SIGILL", &DfxCrasher::IllegalInstructionException},
    {"triSIGSEGV", "trigger a SIGSEGV", &DfxCrasher::SegmentFaultException},
    {"triSIGTRAP", "trigger a SIGTRAP", &DfxCrasher::TriggerTrapException},
    {"triSIGABRT", "trigger a SIGABRT", &DfxCrasher::Abort},

    {"Loop", "trigger a ForeverLoop", &DfxCrasher::Loop},
    {"MaxStack", "trigger SIGSEGV after 64 function call", &DfxCrasher::MaxStackDepth},
    {"MaxMethod", "trigger SIGSEGV after call a function with longer name",
        &DfxCrasher::MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC},

    {"STACKOF", "trigger a stack overflow", &DfxCrasher::StackOverflow},
    {"OOM", "trigger out of memory", &DfxCrasher::Oom},
    {"PCZero", "trigger a crash with pc equal zero", &DfxCrasher::ProgramCounterZero},
    {"MTCrash", "trigger a multi-thread crash", &DfxCrasher::MultiThreadCrash},
    {"StackOver64", "trigger SIGSEGV after 70 function call", &DfxCrasher::StackOver64},
    {"StackTop", "trigger SIGSEGV to make sure stack top", &DfxCrasher::StackTop},
    {"DumpCrash", "trigger a SIGDUMP", &DfxCrasher::DoDumpCrash},
    {"CrashInLambda", "trigger a crash in lambda", &DfxCrasher::CrashInLambda},
    {"ExitHook", "trigger a process exit using exit(0)", &DfxCrasher::TestExitHook},
    {"SigHook", "register sigsegv signal handler", &DfxCrasher::TestSigHook},
    {"StackCorruption", "reset values stored on stack", &DfxCrasher::StackCorruption},
    {"StackCorruption2", "reset values stored in the middle of the stack", &DfxCrasher::StackCorruption2},

#ifdef HAS_CRASH_EXAMPLES
    {"NullPointerDeref0", "nullpointer fault testcase 0", &TestNullPointerDereferenceCrash0},
    {"NullPointerDeref1", "nullpointer fault testcase 1", &TestNullPointerDereferenceCrash1},
    {"NullPointerDeref2", "nullpointer fault testcase 2", &TestNullPointerDereferenceCrash2},
    {"NullPointerDeref3", "nullpointer fault testcase 3", &TestNullPointerDereferenceCrash3},

    {"MultiThreadList", "manipulate list without lock in multithread case", &MultiThreadListAccess},
    {"MultiThreadVector", "manipulate vector without lock in multithread case", &MultiThreadVectorAccess},
    {"MultiThreadMap", "manipulate map without lock in multithread case", &MultiThreadMapAccess},

    {"SptrMismatch", "mix use sptr and raw pointer", &IPCIssues::SptrMismatch},
    {"SptrAndSharedPtrMixUsage", "miss match parcel marshalling and unmarshalling",
        &IPCIssues::SptrAndSharedPtrMixUsage},
    {"ParcelReadWriteMismatch", "miss match parcel marshalling and unmarshalling",
        &IPCIssues::ParcelReadWriteMismatch},
#endif
    {"FatalMessage", "PrintFatalMessageInLibc",
        &DfxCrasher::PrintFatalMessageInLibc},
    {"TestGetCrashObj", "Test get object when crash",
        &DfxCrasher::TestGetCrashObj},
    {"TestGetCrashObjMemory", "Test get memory info when crash",
        &DfxCrasher::TestGetCrashObjMemory},
#ifndef is_ohos_lite
    {"AsyncStack", "Test async stacktrace in nomal thread crash case",
        &DfxCrasher::AsyncStacktrace},
#endif
    {"Deadlock", "Test deadlock and parse lock owner",
        &DfxCrasher::TestDeadlock},
};

constexpr static CrasherCommandLineParam CMDLINE_TABLE_PARAM[] = {
#ifndef is_ohos_lite
    {"CrashInFFRT", "Test async-stacktrace api in ffrt crash case",
        &DfxCrasher::CrashInFFRT},
    {"CrashInLibuvWork", "Test async-stacktrace api in work callback crash case",
        &DfxCrasher::CrashInLibuvWork},
    {"CrashInLibuvTimer", "Test async-stacktrace api in timer callback crash case",
        &DfxCrasher::CrashInLibuvTimer},
    {"CrashInLibuvWorkDone", "Test async-stacktrace api in work callback done crash case",
        &DfxCrasher::CrashInLibuvWorkDone},
#endif
};

extern "C" uintptr_t DFX_SetCrashObj(uint8_t type, uintptr_t addr) __attribute__((weak));
extern "C" void DFX_ResetCrashObj(uintptr_t crashObj) __attribute__((weak));

DfxCrasher::DfxCrasher() {}
DfxCrasher::~DfxCrasher() {}

DfxCrasher &DfxCrasher::GetInstance()
{
    static DfxCrasher instance;
    return instance;
}

int DfxCrasher::TestDeadlock()
{
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_ERRORCHECK);

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);

    int lockOnwerIdx = 1;
    int lockOwnerMask = 0x3fffffff;
    auto mutexInt = reinterpret_cast<int*>(&mutex);
    printf("mutex address:%llx\n", reinterpret_cast<long long>(&mutex));
    printf("mutex owner before lock:%d\n", mutexInt[lockOnwerIdx] & lockOwnerMask);
    pthread_mutex_lock(&mutex);
    printf("mutex owner after lock:%d\n", mutexInt[lockOnwerIdx] & lockOwnerMask);
    std::thread t1([&mutex] {
        pthread_mutex_lock(&mutex);
    });
    t1.join();
    return 0;
}

static NOINLINE int RecursiveHelperFunction(int curLevel, int targetLevel, int midLevel)
{
    auto top = __builtin_frame_address(0);
    uintptr_t size = 256;
    if (curLevel == targetLevel) {
        if (midLevel != 0) {
            abort();
        }
        printf("RecursiveHelperFunction top:%p\n", top);
        // crash in return address
        (void)memset_s(top, size, 0, size);
        return 0;
    }

    if (midLevel != 0 && curLevel == midLevel) {
        // crash in return address
        (void)memset_s(top, size, 0, size);
    }

    int nextLevel = curLevel + 1;
    if (midLevel != 0 || targetLevel != 0) {
        RecursiveHelperFunction(nextLevel, targetLevel, midLevel);
    }

    printf("RecursiveHelperFunction curLevel:%d targetLevel:%d top:%p\n", curLevel, targetLevel, top);
    return nextLevel + 1;
}

NOINLINE int DfxCrasher::StackCorruption()
{
    constexpr int targetLevel = 64;
    constexpr int midLevel = 0;
    return RecursiveHelperFunction(1, targetLevel, midLevel);
}

NOINLINE int DfxCrasher::StackCorruption2()
{
    constexpr int targetLevel = 64;
    constexpr int midLevel = 32;
    return RecursiveHelperFunction(1, targetLevel, midLevel);
}

NOINLINE int DfxCrasher::RaiseFloatingPointException()
{
    raise(SIGFPE);
    return 0;
}

NOINLINE int DfxCrasher::RaiseIllegalInstructionException()
{
    raise(SIGILL);
    return 0;
}

NOINLINE int DfxCrasher::RaiseSegmentFaultException()
{
    std::cout << "call RaiseSegmentFaultException" << std::endl;
    raise(SIGSEGV);
    return 0;
}

NOINLINE int DfxCrasher::RaiseTrapException()
{
    raise(SIGTRAP);
    return 0;
}

NOINLINE int DfxCrasher::RaiseAbort()
{
    HILOG_FATAL(LOG_CORE, "Test Trigger ABORT!");
    raise(SIGABRT);
    return 0;
}

NOINLINE int DfxCrasher::SetLastFatalMessage()
{
    const char msg[] = "Test Trigger ABORT!";
    set_fatal_message(msg);
    raise(SIGABRT);
    return 0;
}

NOINLINE int DfxCrasher::RaiseBusError()
{
    raise(SIGBUS);
    return 0;
}

NOINLINE int DfxCrasher::IllegalInstructionException(void)
{
    IllegalVolatile();
    return 0;
}

NOINLINE int DfxCrasher::TriggerSegmentFaultException()
{
    std::cout << "test TriggerSegmentFaultException" << std::endl;
    // for crash test force cast the type
    int *a = reinterpret_cast<int *>(&TestFunc70);
    *a = SIGSEGV;
    return 0;
}

NOINLINE int DfxCrasher::TriggerTrapException()
{
#ifndef __x86_64__
    __asm__ volatile(".inst 0xde01");
#endif
    return 0;
}

NOINLINE int DfxCrasher::Abort(void)
{
    HILOG_FATAL(LOG_CORE, "Test Trigger ABORT!");
    abort();
    return 0;
}

NOINLINE int DfxCrasher::SegmentFaultException(void)
{
    volatile char *ptr = nullptr;
    *ptr;
    return 0;
}

NOINLINE int DfxCrasher::MaxStackDepth()
{
    return TestFunc1();
}

NOINLINE int DfxCrasher::MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC()
{
    std::cout << "call MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC" << std::endl;
    raise(SIGSEGV);
    return 0;
}

static void *DoStackOverflow(void * inputArg)
{
    int b[10] = {1};
    int *c = nullptr;
    (void)memcpy_s(c, sizeof(int), b, sizeof(int));
    if (b[0] == 0) {
        return static_cast<void*>(b + 9); // 9: last element of array
    }
    DoStackOverflow(inputArg);
    return static_cast<void*>(b + 9); // 9: last element of array
}

NOINLINE int DfxCrasher::StackOverflow()
{
    pthread_t tid;
    pthread_attr_t attr;
    int err = pthread_attr_init(&attr);
    if (err != 0) {
        return err;
    }

    constexpr int maxStackSize = 1024 * 10;
    if (pthread_attr_setstacksize(&attr, maxStackSize) == 0) {
        pthread_create(&tid, &attr, DoStackOverflow, nullptr);
        pthread_join(tid, nullptr);
    } else {
        std::cout << "failed" << std::endl;
    }
    return 0;
}

NOINLINE int DfxCrasher::Oom()
{
    std::cout << "test oom" << std::endl;
    struct rlimit oldRlimit;
    if (getrlimit(RLIMIT_AS, &oldRlimit) != 0) {
        std::cout << "getrlimit failed" << std::endl;
        raise(SIGINT);
    }
    std::cout << std::hex << "old rlimit, cur:0x" << oldRlimit.rlim_cur << std::endl;
    std::cout << std::hex << "old rlimit, max:0x" << oldRlimit.rlim_max << std::endl;

    struct rlimit rlim = {
        .rlim_cur = ARG128 * ARG1024 * ARG1024,
        .rlim_max = ARG128 * ARG1024 * ARG1024,
    };

    if (setrlimit(RLIMIT_AS, &rlim) != 0) {
        std::cout << "setrlimit failed" << std::endl;
        raise(SIGINT);
    }

    std::vector<void*> vec;
    for (int i = 0; i < ARG128; i++) {
        char* buf = static_cast<char*>(mmap(nullptr, (ARG1024 * ARG1024), PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (buf == (char*)MAP_FAILED) {
            std::cout << "malloc return null" << std::endl;
            if (setrlimit(RLIMIT_AS, &oldRlimit) != 0) {
                std::cout << "restore rlimit failed" << std::endl;
            }
            std::cout << "restore rlimit ok" << std::endl;
            abort();
        }

        (void)memset_s(buf, ARG1024 * ARG1024, 0xff, ARG1024 * ARG1024);
        vec.push_back(buf);
    }
    return 0;
}

NOINLINE int DfxCrasher::ProgramCounterZero()
{
    std::cout << "test PCZero" << std::endl;
    ProgramVolatile();
    return 0;
}

NOINLINE int DfxCrasher::MultiThreadCrash()
{
    std::cout << "test MultiThreadCrash" << std::endl;

    std::thread ([] {
        SleepThread(NUMBER_ONE);
    }).detach();
    std::thread ([] {
        SleepThread(NUMBER_TWO);
    }).detach();
    sleep(1);

    raise(SIGSEGV);

    return 0;
}

NOINLINE int DfxCrasher::StackOver64()
{
    std::cout << "test StackOver64" << std::endl;

    return TestFunc1();
}

int SleepThread(int threadID)
{
    std::cout << "create MultiThread " <<  threadID << std::endl;

    int sleepTime = 10;
    sleep(sleepTime);

    return 0;
}

NOINLINE int DfxCrasher::StackTop()
{
    std::cout << "test StackTop" << std::endl;
#if defined(__arm__)
    unsigned int stackTop;
    __asm__ volatile ("mov %0, sp":"=r"(stackTop)::);
#elif defined(__aarch64__)
    uint64_t stackTop;
    __asm__ volatile ("mov %0, sp":"=r"(stackTop)::);
#else
    uint64_t stackTop = 0; // for fixing compile error on x64
#endif
    std::cout << "crasher_c: stack top is = " << std::hex << stackTop << std::endl;

    std::ofstream fout;
    fout.open("/data/sp");
    fout << std::hex << stackTop << std::endl;
    fout.close();

#if defined(__arm__)
    __asm__ volatile ("mov r1, #0\nldr r2, [r1]\n");
#elif defined(__aarch64__)
    __asm__ volatile ("mov x1, #0\nldr x2, [x1]\n");
#endif

    return 0;
}

void DfxCrasher::PrintUsage() const
{
    std::cout << "  usage: crasher CMD" << std::endl;
    std::cout << "\n";
    std::cout << "  where CMD support:" << std::endl;
    for (auto& item : CMDLINE_TABLE) {
        std::cout << "  " << item.cmdline << " : " << item.description << std::endl;
    }
    std::cout << "  if you want the command execute in a sub thread" << std::endl;
    std::cout << "  add thread Prefix, e.g crasher thread-SIGFPE" << std::endl;
    std::cout << "\n";
}

NOINLINE int DfxCrasher::CrashInLambda()
{
    std::function<void()> lambda = TestFunc50;
    lambda();
    return 0;
}

NOINLINE int DfxCrasher::DoDumpCrash()
{
    std::thread t([] {
        TestFunc1();
    });
    raise(SIGDUMP);
    t.join();
    return 0;
}

NOINLINE int DfxCrasher::TestExitHook()
{
    exit(1);
    return 0;
}

static void SigHookHandler(int signo)
{
    printf("SigHookHandler:%d\n", signo);
}

NOINLINE int DfxCrasher::TestSigHook()
{
    signal(SIGSEGV, SigHookHandler);
    return 0;
}

NOINLINE int DfxCrasher::PrintFatalMessageInLibc()
{
    set_fatal_message("TestPrintFatalMessageInLibc");
    RaiseAbort();
    return 0;
}

NOINLINE static void TestGetCrashObjInner()
{
    uintptr_t val = 0;
    if (DFX_SetCrashObj != nullptr) {
        uint8_t type = 0;
        std::string msg = "test get crashObjectInner.";
        val = DFX_SetCrashObj(type, reinterpret_cast<uintptr_t>(msg.c_str()));
    }
    if (DFX_ResetCrashObj != nullptr) {
        DFX_ResetCrashObj(val);
    }
}

NOINLINE int DfxCrasher::TestGetCrashObj()
{
    uintptr_t crashObj = 0;
    if (DFX_SetCrashObj != nullptr) {
        uint8_t type = 0;
        std::string msg = "test get crashObject.";
        crashObj = DFX_SetCrashObj(type, reinterpret_cast<uintptr_t>(msg.c_str()));
    }
    TestGetCrashObjInner();
    raise(SIGSEGV);
    if (DFX_ResetCrashObj != nullptr) {
        DFX_ResetCrashObj(crashObj);
    }
    return 0;
}

NOINLINE static void TestGetCrashObjMemoryInner()
{
    uint8_t type = 1;
    uintptr_t val = 0;
    constexpr size_t bufSize = 4096;
    uintptr_t memory[bufSize] = {2};
    if (DFX_SetCrashObj != nullptr) {
        val = DFX_SetCrashObj(type, reinterpret_cast<uintptr_t>(memory));
    }
    if (DFX_ResetCrashObj != nullptr) {
        DFX_ResetCrashObj(val);
    }
}

NOINLINE int DfxCrasher::TestGetCrashObjMemory()
{
    uint8_t type = 5;
    uintptr_t crashObj = 0;
    constexpr size_t bufSize = 4096;
    uintptr_t memory[bufSize];
    for (size_t i = 0; i < bufSize; i++) {
        memory[i] = i;
    }
    if (DFX_SetCrashObj != nullptr) {
        crashObj = DFX_SetCrashObj(type, reinterpret_cast<uintptr_t>(memory));
    }
    TestGetCrashObjMemoryInner();
    raise(SIGSEGV);
    if (DFX_ResetCrashObj != nullptr) {
        DFX_ResetCrashObj(crashObj);
    }
    return 0;
}

#ifndef is_ohos_lite
static void* CrashInSubThread(void* stackIdPtr)
{
    uint64_t value = *reinterpret_cast<uint64_t *>(stackIdPtr);
    SetStackId(value);
    printf("CrashInSubThread stackId:%p value:%p.\n", stackIdPtr, reinterpret_cast<void*>(value));
    raise(SIGSEGV);
    return nullptr;
}

NOINLINE int DfxCrasher::AsyncStacktrace()
{
#ifdef __aarch64__
    uint64_t stackId = CollectAsyncStack();
    printf("Current stackId:%p.\n", (void*)stackId);
    pthread_t thread;
    pthread_create(&thread, NULL, CrashInSubThread, (void*)&stackId);
    void *result = nullptr;
    pthread_join(thread, &result);
    return reinterpret_cast<uint64_t>(result);
#else
    printf("Unsupported arch.\n");
    return 0;
#endif
}

NOINLINE static int FFRTTaskSubmit1(int i)
{
    int inner = i + 1;
    printf("FFRTTaskSubmit1:current %d\n", inner);
    ffrt::submit(
        [&]() {
            inner = 2; // 2 : inner count
            raise(SIGSEGV);
        },
        {},
        {&inner});
    return inner;
}

NOINLINE static int FFRTTaskSubmit0(int i)
{
    int inner = i + 1;
    printf("FFRTTaskSubmit0:current %d\n", inner);
    return FFRTTaskSubmit1(i);
}

NOINLINE int DfxCrasher::CrashInFFRT(const std::string &debug)
{
    if (debug == "true") {
        setenv("HAP_DEBUGGABLE", "true", 1);
    }
    int i = FFRTTaskSubmit0(10);
    ffrt::wait();
    return i;
}

static bool g_done = 0;
static unsigned g_events = 0;
static unsigned g_result;

NOINLINE static void WorkCallback(uv_work_t* req)
{
    req->data = &g_result;
    raise(SIGSEGV);
}

NOINLINE static void AfterWorkCallback(uv_work_t* req, int status)
{
    g_events++;
    if (!g_done) {
        uv_queue_work(req->loop, req, WorkCallback, AfterWorkCallback);
    }
}

static void TimerCallback(uv_timer_t* handle)
{
    g_done = true;
}

NOINLINE int DfxCrasher::CrashInLibuvWork(const std::string &debug)
{
    if (debug == "true") {
        setenv("HAP_DEBUGGABLE", "true", 1);
    }
    uv_timer_t timerHandle;
    uv_work_t work;
    uv_loop_t* loop = uv_default_loop();
    int timeout = 5000;
    uv_timer_init(loop, &timerHandle);
    uv_timer_start(&timerHandle, TimerCallback, timeout, 0);
    uv_queue_work(loop, &work, WorkCallback, AfterWorkCallback);
    uv_run(loop, UV_RUN_DEFAULT);
    printf("END in CrashInLibuvWork\n");
    return 0;
}

static void TimerCallback2(uv_timer_t* handle)
{
    raise(SIGSEGV);
}

NOINLINE int DfxCrasher::CrashInLibuvTimer(const std::string &debug)
{
    if (debug == "true") {
        setenv("HAP_DEBUGGABLE", "true", 1);
    }
    uv_timer_t timerHandle;
    uv_work_t work;
    uv_loop_t* loop = uv_default_loop();
    int timeout = 5000;
    uv_timer_init(loop, &timerHandle);
    uv_timer_start(&timerHandle, TimerCallback2, timeout, 0);
    uv_queue_work(loop, &work, WorkCallback, AfterWorkCallback);
    uv_run(loop, UV_RUN_DEFAULT);
    printf("END in CrashInLibuvTimer\n");
    return 0;
}

NOINLINE static void WorkCallback2(uv_work_t* req)
{
    req->data = &g_result;
}

NOINLINE static void CrashAfterWorkCallback(uv_work_t* req, int status)
{
    raise(SIGSEGV);
}

NOINLINE int DfxCrasher::CrashInLibuvWorkDone(const std::string &debug)
{
    if (debug == "true") {
        setenv("HAP_DEBUGGABLE", "true", 1);
    }
    uv_work_t work;
    uv_loop_t* loop = uv_default_loop();
    uv_queue_work(loop, &work, WorkCallback2, CrashAfterWorkCallback);
    uv_run(loop, UV_RUN_DEFAULT);
    printf("END in CrashInLibuvWorkDone\n");
    return 0;
}
#endif

void* DfxCrasher::DoCrashInThread(void * inputArg)
{
    prctl(PR_SET_NAME, "SubTestThread");
    const char* arg = (const char *)(inputArg);
    return reinterpret_cast<void*>(DfxCrasher::GetInstance().ParseAndDoCrash(arg));
}

uint64_t DfxCrasher::DoActionOnSubThread(const char *arg) const
{
    pthread_t t;
    pthread_create(&t, nullptr, DfxCrasher::DoCrashInThread, const_cast<char*>(arg));
    void *result = nullptr;
    pthread_join(t, &result);
    return reinterpret_cast<uint64_t>(result);
}

int DfxCrasher::Loop()
{
    int i = 0;
    while (1) {
        usleep(10000); // 10000:sleep 0.01 second
        i++;
    }
    return 0;
}

uint64_t DfxCrasher::ParseAndDoCrash(const char *arg) const
{
    // Prefix
    if (!strncmp(arg, "thread-", strlen("thread-"))) {
        return DoActionOnSubThread(arg + strlen("thread-"));
    }
#ifdef HAS_HITRACE
    auto beginId = HiTraceChain::Begin("test", HITRACE_FLAG_NO_BE_INFO);
#endif
    std::istringstream str(arg);
    std::string out;
    str >> out;
    // Actions
    for (auto& item : CMDLINE_TABLE) {
        if (!strcasecmp(out.c_str(), item.cmdline)) {
            return item.func();
        }
    }
    for (auto& item : CMDLINE_TABLE_PARAM) {
        if (!strcasecmp(out.c_str(), item.cmdline)) {
            if (str >> out) {
                return item.func(out);
            }
        }
    }
#ifdef HAS_HITRACE
    HiTraceChain::End(beginId);
#endif
    return 0;
}

NOINLINE int TestFunc70()
{
    raise(SIGSEGV);
    return 0;
}

int main(int argc, char *argv[])
{
    DfxCrasher::GetInstance().PrintUsage();
    if (argc <= 1) {
        std::cout << "wrong usage!";
        DfxCrasher::GetInstance().PrintUsage();
        return 0;
    }

    std::cout << "ParseAndDoCrash done:" << DfxCrasher::GetInstance().ParseAndDoCrash(argv[1]) << "!\n";
    return 0;
}
