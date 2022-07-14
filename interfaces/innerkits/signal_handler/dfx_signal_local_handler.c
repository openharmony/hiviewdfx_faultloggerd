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

#include "dfx_signal_local_handler.h"
#include "dfx_crash_local_handler.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <securec.h>

#include "dfx_log.h"
#include "dfx_cutil.h"

static SignalHandlerFunc signalHandler = NULL;

static int g_platformSignals[] = {
    SIGABRT,
    SIGBUS,
    SIGILL,
    SIGSEGV,
};

void DFX_InitDumpRequest(struct ProcessDumpRequest* const request, const int sig)
{
    request->type = sig;
    request->tid = gettid();
    request->pid = getpid();
    request->uid = getuid();
    request->reserved = 0;
    request->timeStamp = GetTimeMilliseconds();
    GetThreadName(request->threadName, sizeof(request->threadName));
    GetProcessName(request->processName, sizeof(request->processName));
}

static void DFX_SignalHandler(int sig, siginfo_t * const si, void * const context)
{
    struct ProcessDumpRequest request;
    (void)memset_s(&request, sizeof(request), 0, sizeof(request));
    DFX_InitDumpRequest(&request, sig);

    CrashLocalHandler(&request, si, context);

    _exit(0);
}

void DFX_SetSignalHandlerFunc(SignalHandlerFunc func)
{
    signalHandler = func;
}

void DFX_InstallLocalSignalHandler(void)
{
    sigset_t set;
    sigemptyset (&set);
    struct sigaction action;
    memset_s(&action, sizeof(action), 0, sizeof(action));
    sigfillset(&action.sa_mask);
    if (signalHandler != NULL) {
        action.sa_sigaction = signalHandler;
    } else {
        action.sa_sigaction = DFX_SignalHandler;
    }
    action.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
    
    for (size_t i = 0; i < sizeof(g_platformSignals) / sizeof(g_platformSignals[0]); i++) {
        int32_t sig = g_platformSignals[i];
        sigaddset(&set, sig);
        if (sigaction(sig, &action, NULL) != 0) {
            DfxLogError("Failed to register signal. %{public}d", sig);
        }
    }
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}
