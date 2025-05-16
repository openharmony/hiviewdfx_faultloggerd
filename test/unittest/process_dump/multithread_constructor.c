/*
 * Copyright (c) 2022-2025 Huawei Device Co., Ltd.
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

#include "multithread_constructor.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "stdio.h"
#include "stdlib.h"

#define NOINLINE __attribute__((noinline))

const static unsigned int SLEEP_TIMEOUT = 360000;

static void CreateThread(int *argv)
{
    int threadID = *argv;
    printf("create MultiThread %d\n", threadID);
    sleep(3); // 3 : sleep 3 seconds // 3 : three seconds
    return;
}

static void CreateThreadForCrash(const int *argv)
{
    int threadID = *argv;
    printf("create ThreadForCrash %d\n", threadID);
    int ret = raise(SIGSEGV);
    if (ret != 0) {
        printf("remove failed!");
    }
    return;
}

pid_t CreateMultiThreadProcess(int threadNum)
{
    pid_t pid = fork();
    if (pid < 0) {
        printf("Failed to fork new test process.");
    } else if (pid == 0) {
        (void)MultiThreadConstructor(threadNum);
        sleep(3); // 3 : sleep 3 seconds
        _exit(0);
    }
    return pid;
}

pid_t CreateMultiThreadForThreadCrash(int threadNum)
{
    pid_t pid = fork();
    if (pid < 0) {
        printf("Failed to fork new test process.");
    } else if (pid == 0) {
        (void)MultiThreadConstructorForThreadCrash(threadNum);
    }
    return pid;
}

pid_t CreateMultiThreadForThreadCrashWithOpen(int threadNum, int openNum)
{
    pid_t pid = fork();
    if (pid < 0) {
        printf("Failed to fork new test process.");
    } else if (pid == 0) {
        for (int i = 0; i < openNum; ++i) {
            fopen("/dev/null", "r");
        }
        (void)MultiThreadConstructorForThreadCrash(threadNum);
    }
    return pid;
}

NOINLINE int MultiThreadConstructor(const int threadNum)
{
    pthread_t t[threadNum];
    int threadID[threadNum];

    for (int i = 0; i < threadNum; ++i) {
        threadID[i] = i;
        pthread_create(&t[i], NULL, (void *(*)(void *))CreateThread, &threadID[i]);
        pthread_detach(t[i]);
    }
    return 0;
}

NOINLINE int MultiThreadConstructorForThreadCrash(const int threadNum)
{
    pthread_t t[threadNum];
    int threadID[threadNum];
    pthread_t threadCrash;
    int threadIDCrash = threadNum;

    for (int i = 0; i < threadNum; ++i) {
        threadID[i] = i;
        pthread_create(&t[i], NULL, (void *(*)(void *))CreateThread, &threadID[i]);
        pthread_detach(t[i]);
    }
    pthread_create(&threadCrash, NULL, (void *(*)(void *))CreateThreadForCrash, &threadIDCrash);
    pthread_detach(threadCrash);

    sleep(5); // 5 : wait 5s, then exit process

    return 0;
}

NOINLINE int TestFunc70(void)
{
    sleep(SLEEP_TIMEOUT);
    return 0;
}

#ifndef UNITTEST
int main(int argc, char* argv[])
{
    const int argumentLimit = 2;
    if (argc != argumentLimit) {
        printf("invalid input argument.\n");
        return 0;
    }
    MultiThreadConstructor(atoi(argv[1]));
    return 0;
}
#endif
