/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "dfx_exit_hook.h"

#include <dlfcn.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

#include "dfx_log.h"
#include "dfx_hook_utils.h"

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0xD002D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxExitHook"
#endif

typedef int (*KillFunc)(pid_t pid, int sig);
typedef __attribute__((noreturn)) void (*ExitFunc)(int code);
static KillFunc hookedKill = NULL;
static ExitFunc hookedExit = NULL;
static ExitFunc hookedExitEx = NULL;
static bool abortWhenExit = false;

void __attribute__((constructor)) InitExitHook(void)
{
    char* str = getenv("ABORT_EXIT");
    if (str != NULL && strlen(str) > 0) {
        printf("AbortExit:%s\n", str);
        abortWhenExit = true;
    }

    StartHookExitFunc();
}

int kill(pid_t pid, int sig)
{
    LOGF("%d send signal(%d) to %d", getpid(), sig, pid);
    if ((sig == SIGKILL) && (pid == getpid())) {
        abort();
    } else if (sig == SIGKILL) {
        LogBacktrace();
    }

    if (hookedKill == NULL) {
        LOGE("hooked kill is NULL?\n");
        return syscall(SYS_kill, pid, sig);
    }
    return hookedKill(pid, sig);
}

void exit(int code)
{
    LOGF("%d call exit with code %d", getpid(), code);
    if (!abortWhenExit) {
        LogBacktrace();
    }

    if ((!abortWhenExit) && (hookedExit != NULL)) {
        hookedExit(code);
    } else if (abortWhenExit) {
        abort();
    }

    quick_exit(code);
}

void _exit(int code)
{
    LOGF("%d call exit with code %d", getpid(), code);
    if (!abortWhenExit) {
        LogBacktrace();
    }

    if ((!abortWhenExit) && (hookedExitEx != NULL)) {
        hookedExitEx(code);
    } else if (abortWhenExit) {
        abort();
    }

    quick_exit(code);
}

GenHookFunc(StartHookKillFunction, KillFunc, "kill", hookedKill)
GenHookFunc(StartHookExitFunction, ExitFunc, "exit", hookedExit)
GenHookFunc(StartHookExitExFunction, ExitFunc, "_exit", hookedExitEx)

void StartHookExitFunc(void)
{
    StartHookKillFunction();
    StartHookExitFunction();
    StartHookExitExFunction();
}
