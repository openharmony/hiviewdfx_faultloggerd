/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

/* This files contains faultlog cpp crasher modules. */

#include "dfx_crasher.h"

#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <hilog/log.h>
#include <iostream>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "securec.h"

#ifdef HAS_HITRACE
#include <hitrace/hitracechain.h>
using namespace OHOS::HiviewDFX;
#endif

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Unwind"
#endif

static const int ARG1024 = 1024;
static const int ARG128 = 128;

static const int NUMBER_TWO = 2;
static const int NUMBER_ONE = 1;

static const int SIGDUMP = 35;

DfxCrasher::DfxCrasher() {}
DfxCrasher::~DfxCrasher() {}

DfxCrasher &DfxCrasher::GetInstance()
{
    static DfxCrasher instance;
    return instance;
}

NOINLINE int DfxCrasher::TriggerTrapException() const
{
#ifndef __x86_64__
    __asm__ volatile(".inst 0xde01");
#endif
    return 0;
}

NOINLINE int DfxCrasher::TriggerSegmentFaultException() const
{
    std::cout << "test TriggerSegmentFaultException" << std::endl;
    // for crash test force cast the type
    int *a = (int *)(&TestFunc70);
    *a = SIGSEGV;
    return 0;
}

NOINLINE int DfxCrasher::RaiseAbort() const
{
    HILOG_FATAL(LOG_CORE, "Test Trigger ABORT!");
    raise(SIGABRT);
    return 0;
}

NOINLINE int DfxCrasher::Abort(void) const
{
    HILOG_FATAL(LOG_CORE, "Test Trigger ABORT!");
    abort();
    return 0;
}

NOINLINE int DfxCrasher::RaiseBusError() const
{
    raise(SIGBUS);
    return 0;
}

NOINLINE int DfxCrasher::DumpStackTrace() const
{
    if (raise(SIGDUMP) != 0) {
        std::cout << "raise error" << std::endl;
    }
    return 0;
}

NOINLINE int DfxCrasher::RaiseFloatingPointException() const
{
    raise(SIGFPE);
    return 0;
}

NOINLINE int DfxCrasher::RaiseIllegalInstructionException() const
{
    raise(SIGILL);
    return 0;
}

NOINLINE int DfxCrasher::IllegalInstructionException(void) const
{
    char mes[] = "ABCDEFGHIJ";
    char* ptr = mes;
    ptr = nullptr;
    *ptr = 0;

    return 0;
}

NOINLINE int DfxCrasher::SegmentFaultException(void) const
{
    volatile char *ptr = nullptr;
    *ptr;

    return 0;
}

NOINLINE int DfxCrasher::RaiseSegmentFaultException() const
{
    std::cout << "call RaiseSegmentFaultException" << std::endl;
    raise(SIGSEGV);
    return 0;
}

NOINLINE int DfxCrasher::RaiseTrapException() const
{
    raise(SIGTRAP);
    return 0;
}

NOINLINE int DfxCrasher::MaxStackDepth() const
{
    return TestFunc1();
}

NOINLINE int DfxCrasher::MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC() const
{
    std::cout << "call MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC" << std::endl;
    raise(SIGSEGV);
    return 0;
}

NOINLINE int DfxCrasher::StackOverflow() const
{
    std::cout << "call StackOverflow" << std::endl;
    // for stack overflow test
    char a[1024][1024][1024] = { { {'1'} } };
    char b[1024][1024][1024] = { { {'1'} } };
    char c[1024][1024][1024] = { { {'1'} } };
    char d[1024][1024][1024] = { { {'1'} } };
	
    std::cout << a[0][0] << std::endl;
    std::cout << b[0][0] << std::endl;
    std::cout << c[0][0] << std::endl;
    std::cout << d[0][0] << std::endl;

    return 0;
}

NOINLINE int DfxCrasher::Oom() const
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
        char* buf = static_cast<char*>(malloc(ARG1024 * ARG1024));
        if (!buf) {
            std::cout << "malloc return null" << std::endl;
            if (setrlimit(RLIMIT_AS, &oldRlimit) != 0) {
                std::cout << "restore rlimit failed" << std::endl;
            }
            std::cout << "restore rlimit ok" << std::endl;
            abort();
        }

        if (memset_s(buf, ARG1024 * ARG1024, 0xff, ARG1024 * ARG1024)) {
            std::cout << "oom memset_s failed." << std::endl;
        }
        vec.push_back(buf);
    }
    return 0;
}

NOINLINE int DfxCrasher::ProgramCounterZero() const
{
    std::cout << "test PCZero" << std::endl;
#if defined(__arm__)
    __asm__ volatile (
        "mov r0, #0x00\n mov lr, pc\n bx r0\n"
    );
#elif defined(__aarch64__)
    __asm__ volatile (
        "movz x0, #0x0\n"
        "adr x30, .\n"
        "br x0\n"
    );
#endif
    return 0;
}

NOINLINE int DfxCrasher::MultiThreadCrash() const
{
    std::cout << "test MultiThreadCrash" << std::endl;

    std::thread (SleepThread, NUMBER_ONE).detach();
    std::thread (SleepThread, NUMBER_TWO).detach();
    sleep(1);

    raise(SIGSEGV);

    return 0;
}

NOINLINE int DfxCrasher::StackOver64() const
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

NOINLINE int DfxCrasher::StackTop() const
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
    fout.open("sp");
    fout << std::hex << stackTop << std::endl;
    fout.close();

    // trigger an error to crash
    int a = 1;
    int *b = &a;
    b = nullptr;
    *b = 1;

    return 0;
}

void DfxCrasher::PrintUsage() const
{
    std::cout << "  usage: crasher CMD" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "  where CMD support:" << std::endl;
    std::cout << "  SIGFPE                raise a SIGFPE" << std::endl;
    std::cout << "  SIGILL                raise a SIGILL" << std::endl;
    std::cout << "  SIGSEGV               raise a SIGSEGV" << std::endl;
    std::cout << "  SIGTRAP               raise a SIGTRAP" << std::endl;
    std::cout << "  SIGABRT               raise a SIGABRT" << std::endl;
    std::cout << "  SIGBUS                raise a SIGBUS" << std::endl;
    std::cout << "  STACKTRACE            raise a SIGDUMP" << std::endl;

    std::cout << "  triSIGILL             trigger a SIGILL\n" << std::endl;
    std::cout << "  triSIGSEGV            trigger a SIGSEGV\n" << std::endl;
    std::cout << "  triSIGTRAP            trigger a SIGTRAP\n" << std::endl;
    std::cout << "  triSIGABRT            trigger a SIGABRT\n" << std::endl;

    std::cout << "  Loop                  trigger a ForeverLoop" << std::endl;
    std::cout << "  MaxStack              trigger SIGSEGV after 64 function call" << std::endl;
    std::cout << "  MaxMethod             trigger SIGSEGV after call a function with longer name" << std::endl;
    std::cout << "  STACKOF               trigger a stack overflow" << std::endl;
    std::cout << "  OOM                   trigger out of memory" << std::endl;
    std::cout << "  PCZero                trigger pc = 0" << std::endl;
    std::cout << "  MTCrash               trigger crash with multi-thread" << std::endl;
    std::cout << "  StackOver64           trigger SIGSEGV after 70 function call" << std::endl;
    std::cout << "  StackTop              trigger SIGSEGV to make sure stack top" << std::endl;
    std::cout << "  if you want the command execute in a sub thread" << std::endl;
    std::cout << "  add thread Prefix, e.g crasher thread-SIGFPE" << std::endl;
    std::cout << std::endl;
}

NOINLINE static uint64_t CrashInLambda()
{
    std::function<void()> lambda = TestFunc50;
    lambda();
    return 0;
}

NOINLINE static uint64_t DoDumpCrash()
{
    std::thread t(TestFunc1);
    raise(SIGDUMP);
    t.join();
    return 0;
}

NOINLINE static uint64_t TestExitHook()
{
    exit(1);
    return 0;
}

void SigHookHandler(int signo)
{
    printf("SigHookHandler:%d\n", signo);
}

NOINLINE static uint64_t TestSigHook()
{
    signal(SIGSEGV, SigHookHandler);
    return 0;
}

void* DfxCrasher::DoCrashInThread(void * inputArg)
{
    prctl(PR_SET_NAME, "SubTestThread");
    const char* arg = (const char *)(inputArg);
    return (void*)(DfxCrasher::GetInstance().ParseAndDoCrash(arg));
}

uint64_t DfxCrasher::DoActionOnSubThread(const char *arg) const
{
    pthread_t t;
    pthread_create(&t, nullptr, DfxCrasher::DoCrashInThread, const_cast<char*>(arg));
    void *result = nullptr;
    pthread_join(t, &result);
    return (uint64_t)(result);
}

uint64_t DfxCrasher::ParseAndDoCrash(const char *arg)
{
    // Prefix
    if (!strncmp(arg, "thread-", strlen("thread-"))) {
        return DoActionOnSubThread(arg + strlen("thread-"));
    }
#ifdef HAS_HITRACE
    auto beginId = HiTraceChain::Begin("test", HITRACE_FLAG_NO_BE_INFO);
#endif
    // Action
    if (!strcasecmp(arg, "SIGFPE")) {
        return RaiseFloatingPointException();
    }

    if (!strcasecmp(arg, "SIGILL")) {
        return RaiseIllegalInstructionException();
    }

    if (!strcasecmp(arg, "SIGSEGV")) {
        return RaiseSegmentFaultException();
    }

    if (!strcasecmp(arg, "SIGTRAP")) {
        return RaiseTrapException();
    }

    if (!strcasecmp(arg, "triSIGILL")) {
        return IllegalInstructionException();
    }

    if (!strcasecmp(arg, "SIGABRT")) {
        return RaiseAbort();
    }

    if (!strcasecmp(arg, "triSIGABRT")) {
        return Abort();
    }

    if (!strcasecmp(arg, "SIGBUS")) {
        return RaiseBusError();
    }

    if (!strcasecmp(arg, "STACKTRACE")) {
        return DumpStackTrace();
    }

    if (!strcasecmp(arg, "triSIGTRAP")) {
        return TriggerTrapException();
    }

    if (!strcasecmp(arg, "Loop")) {
        int i = 0;
        while (1) {
            usleep(10000); // 10000:sleep 0.01 second
            i++;
        }
    }

    if (!strcasecmp(arg, "triSIGSEGV")) {
        return SegmentFaultException();
    }

    if (!strcasecmp(arg, "MaxStack")) {
        return MaxStackDepth();
    }

    if (!strcasecmp(arg, "MaxMethod")) {
        return MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC();
    }

    if (!strcasecmp(arg, "STACKOF")) {
        return StackOverflow();
    }

    if (!strcasecmp(arg, "OOM")) {
        return Oom();
    }

    if (!strcasecmp(arg, "PCZero")) {
        return ProgramCounterZero();
    }

    if (!strcasecmp(arg, "MTCrash")) {
        return MultiThreadCrash();
    }

    if (!strcasecmp(arg, "StackOver64")) {
        return StackOver64();
    }

    if (!strcasecmp(arg, "StackTop")) {
        return StackTop();
    }

    if (!strcasecmp(arg, "DumpCrash")) {
        return DoDumpCrash();
    }

    if (!strcasecmp(arg, "CrashInLambda")) {
        return CrashInLambda();
    }

    if (!strcasecmp(arg, "ExitHook")) {
        return TestExitHook();
    }

    if (!strcasecmp(arg, "SigHook")) {
        return TestSigHook();
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

// auto gen function
GEN_TEST_FUNCTION(0, 1)
GEN_TEST_FUNCTION(1, 2)
GEN_TEST_FUNCTION(2, 3)
GEN_TEST_FUNCTION(3, 4)
GEN_TEST_FUNCTION(4, 5)
GEN_TEST_FUNCTION(5, 6)
GEN_TEST_FUNCTION(6, 7)
GEN_TEST_FUNCTION(7, 8)
GEN_TEST_FUNCTION(8, 9)
GEN_TEST_FUNCTION(9, 10)

GEN_TEST_FUNCTION(10, 11)
GEN_TEST_FUNCTION(11, 12)
GEN_TEST_FUNCTION(12, 13)
GEN_TEST_FUNCTION(13, 14)
GEN_TEST_FUNCTION(14, 15)
GEN_TEST_FUNCTION(15, 16)
GEN_TEST_FUNCTION(16, 17)
GEN_TEST_FUNCTION(17, 18)
GEN_TEST_FUNCTION(18, 19)
GEN_TEST_FUNCTION(19, 20)

GEN_TEST_FUNCTION(20, 21)
GEN_TEST_FUNCTION(21, 22)
GEN_TEST_FUNCTION(22, 23)
GEN_TEST_FUNCTION(23, 24)
GEN_TEST_FUNCTION(24, 25)
GEN_TEST_FUNCTION(25, 26)
GEN_TEST_FUNCTION(26, 27)
GEN_TEST_FUNCTION(27, 28)
GEN_TEST_FUNCTION(28, 29)
GEN_TEST_FUNCTION(29, 30)

GEN_TEST_FUNCTION(30, 31)
GEN_TEST_FUNCTION(31, 32)
GEN_TEST_FUNCTION(32, 33)
GEN_TEST_FUNCTION(33, 34)
GEN_TEST_FUNCTION(34, 35)
GEN_TEST_FUNCTION(35, 36)
GEN_TEST_FUNCTION(36, 37)
GEN_TEST_FUNCTION(37, 38)
GEN_TEST_FUNCTION(38, 39)
GEN_TEST_FUNCTION(39, 40)

GEN_TEST_FUNCTION(40, 41)
GEN_TEST_FUNCTION(41, 42)
GEN_TEST_FUNCTION(42, 43)
GEN_TEST_FUNCTION(43, 44)
GEN_TEST_FUNCTION(44, 45)
GEN_TEST_FUNCTION(45, 46)
GEN_TEST_FUNCTION(46, 47)
GEN_TEST_FUNCTION(47, 48)
GEN_TEST_FUNCTION(48, 49)
GEN_TEST_FUNCTION(49, 50)

GEN_TEST_FUNCTION(50, 51)
GEN_TEST_FUNCTION(51, 52)
GEN_TEST_FUNCTION(52, 53)
GEN_TEST_FUNCTION(53, 54)
GEN_TEST_FUNCTION(54, 55)
GEN_TEST_FUNCTION(55, 56)
GEN_TEST_FUNCTION(56, 57)
GEN_TEST_FUNCTION(57, 58)
GEN_TEST_FUNCTION(58, 59)
GEN_TEST_FUNCTION(59, 60)

GEN_TEST_FUNCTION(60, 61)
GEN_TEST_FUNCTION(61, 62)
GEN_TEST_FUNCTION(62, 63)
GEN_TEST_FUNCTION(63, 64)
GEN_TEST_FUNCTION(64, 65)
GEN_TEST_FUNCTION(65, 66)
GEN_TEST_FUNCTION(66, 67)
GEN_TEST_FUNCTION(67, 68)
GEN_TEST_FUNCTION(68, 69)
GEN_TEST_FUNCTION(69, 70)
