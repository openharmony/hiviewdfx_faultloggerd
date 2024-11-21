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

#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include "errno.h"
#include "hilog/log.h"
#include "inttypes.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "strings.h"
#include "dfx_define.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Unwind"
#endif

NOINLINE int TriggerTrapException(void)
{
#ifndef __x86_64__
    __asm__ volatile(".inst 0xde01");
#endif
    return 0;
}

NOINLINE int RaiseAbort(void)
{
    HILOG_FATAL(LOG_CORE, "Test Trigger ABORT!");
    int ret = raise(SIGABRT);
    if (ret != 0) {
        printf("raise SIGABRT failed!");
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
        printf("raise SIGBUS failed!");
    }
    return 0;
}

NOINLINE int RaiseFloatingPointException(void)
{
    int ret = raise(SIGFPE);
    if (ret != 0) {
        printf("raise SIGFPE failed!");
    }
    return 0;
}

NOINLINE int RaiseIllegalInstructionException(void)
{
    int ret = raise(SIGILL);
    if (ret != 0) {
        printf("raise SIGILL failed!");
    }
    return 0;
}

NOINLINE int IllegalInstructionException(void)
{
    IllegalVolatile();
    return 0;
}

NOINLINE int RaiseSegmentFaultException(void)
{
    printf("call RaiseSegmentFaultException \n");
    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("raise SIGSEGV failed!");
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
        printf("raise SIGTRAP failed!");
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
        printf("raise SIGSEGV failed!");
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
        char* buf = (char*)mmap(NULL, (ARG1024 * ARG1024), PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (buf == (char*)MAP_FAILED) {
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

static NOINLINE int ProgramCounterZero(void)
{
    printf("test PCZero");
    ProgramVolatile();
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
        printf("raise SIGSEGV failed!");
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
    printf("test StackTop\n");
    register void* stackTop;
#if defined(__arm__)
    __asm__ volatile ("mov %0, sp":"=r"(stackTop)::);
    printf("crasher_c: stack top is = %08x\n", (unsigned int)stackTop);
#elif defined(__aarch64__)
    __asm__ volatile ("mov %0, sp":"=r"(stackTop)::);
    printf("crasher_c: stack top is = %16llx\n", (unsigned long long)stackTop);
#else
    return 0;
#endif

    FILE *fp = NULL;
    fp = fopen("/data/sp", "w");
    if (fp == NULL) {
        printf("Open /data/sp failed, errno(%d)\n", errno);
        return 0;
    }

    int ret = 0; // for fixing compile error on x64
#if defined(__arm__)
    ret = fprintf(fp, "%08x", (unsigned int)stackTop);
#elif defined(__aarch64__)
    ret = fprintf(fp, "%16llx", (unsigned long long)stackTop);
#endif
    if (ret == EOF) {
        printf("error!");
    }
    ret = fclose(fp);
    if (ret == EOF) {
        printf("close error!");
    }

#if defined(__arm__)
    __asm__ volatile ("mov r1, #0\nldr r2, [r1]\n");
#elif defined(__aarch64__)
    __asm__ volatile ("mov x1, #0\nldr x2, [x1]\n");
#endif
    return ret;
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
        printf("raise SIGSEGV failed!");
    }
    return 0;
}

NOINLINE int CrashTest(void)
{
    int sleepTime = 3;
    sleep(sleepTime);
    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("raise SIGSEGV failed!");
    }
    return 0;
}

int main(int argc, char *argv[])
{
    PrintUsage();
    if (argc <= 1) {
        printf("wrong usage!");
        PrintUsage();
        return 0;
    }

    printf("ParseAndDoCrash done: %" PRIu64 "!\n", ParseAndDoCrash(argv[1]));
    return 0;
}
