/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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

#include <hilog/log.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>

#include "dfx_signal_handler.h"
#include "securec.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Unwind"
#endif

static const int ARG1024 = 1024;
static const int ARG128 = 128;

NOINLINE int TriggerTrapException(void)
{
    __asm__ volatile(".inst 0xde01");

    return 0;
}

NOINLINE int RaiseAbort(void)
{
    HILOG_FATAL(LOG_CORE, "Test Trigger ABORT!");
    int ret = raise(SIGABRT);
    if (ret != 0) {
        printf("raise failed!");
    }
    return 0;
}
NOINLINE int Abort(void)
{
    HILOG_FATAL(LOG_CORE, "Test Trigger ABORT!");
    abort();
    return 0;
}

NOINLINE int RaiseBusError(void)
{
    int ret = raise(SIGBUS);
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

NOINLINE int DumpStackTrace(void)
{
    int ret = raise(35); // 35:SIGDUMP
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

NOINLINE int RaiseFloatingPointException(void)
{
    int ret = raise(SIGFPE);
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

NOINLINE int RaiseIllegalInstructionException(void)
{
    int ret = raise(SIGILL);
    if (ret !=0) {
        printf("ramove failed!");
    }
    return 0;
}
NOINLINE int IllegalInstructionException(void)
{
    char mes[] = "ABCDEFGHIJ";
    char* ptr = mes;
    ptr = NULL;
    *ptr = 0;

    return 0;
}

NOINLINE int RaiseSegmentFaultException(void)
{
    printf("call RaiseSegmentFaultException \n");
    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

NOINLINE int SegmentFaultException(void)
{
    volatile char *ptr = NULL;
    *ptr;

    return 0;
}

NOINLINE int RaiseTrapException(void)
{
    int ret = raise(SIGTRAP);
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

NOINLINE int TrapException(void)
{
    return 0;
}

NOINLINE int MaxStackDepth(void)
{
    return TestFunc1();
}

NOINLINE int MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC(void)
{
    printf("call MaxMethodNameTest12345678901234567890123456789012345678901234567890ABC \n");
    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

NOINLINE int StackOverflow(void)
{
    printf("call StackOverflow\n");
    
    // for stack overflow test
    char a[1024][1024][1024] = { { {'1'} } };
    char b[1024][1024][1024] = { { {'1'} } };
    char c[1024][1024][1024] = { { {'1'} } };
    char d[1024][1024][1024] = { { {'1'} } };
    printf("a[0][0] is %s\n", a[0][0]);
    printf("b[0][0] is %s\n", b[0][0]);
    printf("c[0][0] is %s\n", c[0][0]);
    printf("d[0][0] is %s\n", d[0][0]);

    return 0;
}

NOINLINE int Oom(void)
{
    struct rlimit oldRlimit;
    if (getrlimit(RLIMIT_AS, &oldRlimit) != 0) {
        printf("getrlimit failed\n");
        raise(SIGINT);
    }
    printf("old rlimit, cur:0x%016" PRIx64 " max:0x%016" PRIx64 "\n",
        (uint64_t)oldRlimit.rlim_cur, (uint64_t)oldRlimit.rlim_max);

    struct rlimit rlim = {
        .rlim_cur = (ARG128 - 1) * ARG1024 * ARG1024,
        .rlim_max = (ARG128 - 1) * ARG1024 * ARG1024,
    };

    if (setrlimit(RLIMIT_AS, &rlim) != 0) {
        printf("setrlimit failed\n");
        raise(SIGINT);
    }
    char* bufferArray[ARG128];
    for (int i = 0; i < ARG128; i++) {
        char* buf = (char*)malloc(ARG1024 * ARG1024);
        if (!buf) {
            printf("malloc return null\n");
            if (setrlimit(RLIMIT_AS, &oldRlimit) != 0) {
                printf("restore rlimit failed\n");
            }
            printf("restore rlimit ok\n");
            abort();
        }
        bufferArray[i] = buf;
    }
    for (int i = 0; i < ARG128; i++) {
        printf("0x%x", *(bufferArray[i] + 1));
    }
    
    return 0;
}

NOINLINE int ProgramCounterZero(void)
{
    printf("test PCZero");
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

NOINLINE int MultiThreadCrash(void)
{
    printf("test MultiThreadCrash");

    pthread_t t[2];
    int threadID[2] = {1, 2};
    pthread_create(&t[0], NULL, SleepThread, &threadID[0]);
    pthread_create(&t[1], NULL, SleepThread, &threadID[1]);
    pthread_detach(t[0]);
    pthread_detach(t[1]);
    sleep(1);

    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("remove failed!");
    }

    return 0;
}

NOINLINE int StackOver64(void)
{
    printf("test StackOver64");

    return TestFunc1();
}

void *SleepThread(void *argv)
{
    int threadID = *(int*)argv;
    printf("create MultiThread %d", threadID);

    int sleepTime = 10;
    sleep(sleepTime);

    return 0;
}

NOINLINE int StackTop(void)
{
    printf("test StackTop");

#if defined(__arm__)
    int stackTop;
    __asm__ volatile ("mov %0, sp":"=r"(stackTop)::);
    printf("crasher_c: stack top is = %08x", stackTop);
#elif defined(__aarch64__)
    uint64_t stackTop;
    __asm__ volatile ("mov %0, sp":"=r"(stackTop)::);
    printf("crasher_c: stack top is = %16llx", (unsigned long long)stackTop);
#else
    return 0;
#endif

    FILE *fp = NULL;
    fp = fopen("sp", "w");
    if (fp == NULL) {
        printf("open file error!");
        return 0;
    }

#if defined(__arm__)
    int ret = fprintf(fp, "%08x", stackTop);
#elif defined(__aarch64__)
    int ret = fprintf(fp, "%16llx", (unsigned long long)stackTop);
#endif
    if (ret == EOF) {
        printf("error!");
    }
    ret = fclose(fp);
    if (ret == EOF) {
        printf("close error!");
    }
    // trigger an error to crash
    int a = 1;
    int *b = &a;
    b = NULL;
    *b = 1;

    return 0;
}

void PrintUsage(void)
{
    printf("  usage: crasher CMD\n");
    printf("\n");
    printf("  where CMD support:\n");
    printf("  SIGFPE                raise a SIGFPE\n");
    printf("  SIGILL                raise a SIGILL\n");
    printf("  SIGSEGV               raise a SIGSEGV\n");
    printf("  SIGTRAP               raise a SIGTRAP\n");
    printf("  SIGABRT               raise a SIGABRT\n");
    printf("  SIGBUS                raise a SIGBUS\n");
    printf("  STACKTRACE            raise a SIGDUMP\n");

    printf("  triSIGILL             trigger a SIGILL\n");
    printf("  triSIGSEGV            trigger a SIGSEGV\n");
    printf("  triSIGTRAP            trigger a SIGTRAP\n");
    printf("  triSIGABRT            trigger a SIGABRT\n");

    printf("  Loop                  trigger a ForeverLoop\n");
    printf("  MaxStack              trigger SIGSEGV after 64 function call\n");
    printf("  MaxMethod             trigger SIGSEGV after call a function with longer name\n");
    printf("  OOM                   trigger out of memory\n");
    printf("  STACKOF               trigger a stack overflow\n");
    printf("  PCZero                trigger pc = 0\n");
    printf("  MTCrash               trigger crash with multi-thread\n");
    printf("  StackOver64           trigger SIGSEGV after 70 function call\n");
    printf("  StackTop              trigger SIGSEGV to make sure stack top\n");
    printf("  if you want the command execute in a sub thread\n");
    printf("  add thread Prefix, e.g crasher thread-SIGFPE\n");
    printf("\n");
}

void *DoCrashInThread(void *inputArg)
{
    prctl(PR_SET_NAME, "SubTestThread");
    const char *arg = (const char *)(inputArg);
    return (void *)((uint64_t)(ParseAndDoCrash(arg)));
}

uint64_t DoActionOnSubThread(const char *arg)
{
    pthread_t t;
    pthread_create(&t, NULL, DoCrashInThread, (char *)(arg));
    void *result = NULL;
    pthread_join(t, &result);
    return (uint64_t)(result);
}

uint64_t ParseAndDoCrash(const char *arg)
{
    // Prefix
    if (!strncmp(arg, "thread-", strlen("thread-"))) {
        return DoActionOnSubThread(arg + strlen("thread-"));
    }

    // Action
    if (!strcasecmp(arg, "SIGFPE")) {
        return RaiseFloatingPointException();
    }

    if (!strcasecmp(arg, "SIGILL")) {
        return RaiseIllegalInstructionException();
    }

    if (!strcasecmp(arg, "triSIGILL")) {
        return IllegalInstructionException();
    }

    if (!strcasecmp(arg, "SIGSEGV")) {
        return RaiseSegmentFaultException();
    }
    
    if (!strcasecmp(arg, "SIGTRAP")) {
        return RaiseTrapException();
    }
    
    if (!strcasecmp(arg, "SIGABRT")) {
        return RaiseAbort();
    }

    if (!strcasecmp(arg, "triSIGABRT")) {
        return Abort();
    }

    if (!strcasecmp(arg, "triSIGSEGV")) {
        return SegmentFaultException();
    }

    if (!strcasecmp(arg, "SIGBUS")) {
        return RaiseBusError();
    }

    if (!strcasecmp(arg, "triSIGTRAP")) {
        return TriggerTrapException();
    }

    if (!strcasecmp(arg, "STACKTRACE")) {
        return DumpStackTrace();
    }

    if (!strcasecmp(arg, "Loop")) {
        int i = 0;
        while (1) {
            usleep(10000); // 10000:sleep 0.01 second
            i++;
        }
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

    if (!strcasecmp(arg, "CrashTest")) {
        return CrashTest();
    }

    return 0;
}

NOINLINE int TestFunc70(void)
{
    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

NOINLINE int CrashTest(void)
{
    int sleepTime = 3;
    sleep(sleepTime);
    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("remove failed!");
    }
    return 0;
}

int main(int argc, char *argv[])
{
    DFX_InstallSignalHandler();
    PrintUsage();
    if (argc <= 1) {
        printf("wrong usage!");
        PrintUsage();
        return 0;
    }

    printf("ParseAndDoCrash done: %" PRIu64 "!", ParseAndDoCrash(argv[1]));
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
