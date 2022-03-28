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

#include "dfx_signal_handler.h"

#include <dlfcn.h>
#include <unistd.h>

#include <hilog_base/log_base.h>

#ifdef LOG_DOMAIN
#undef LOG_DOMAIN
#define LOG_DOMAIN 0x2D11
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "DfxFuncHook"
#endif

typedef int (*KillFunc)(pid_t pid, int sig);
typedef int (*SigactionFunc)(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact);
static KillFunc hookedKill = NULL;
static SigactionFunc hookedSigaction = NULL;
static uintptr_t signalHandler = 0;
int kill(pid_t pid, int sig)
{
    HILOG_BASE_WARN(LOG_CORE, "%{public}d send signal(%{public}d) to %{public}d", getpid(), sig, pid);
    if (hookedKill == NULL) {
        HILOG_BASE_ERROR(LOG_CORE, "hooked kill is NULL?");
        return -1;
    }
    return hookedKill(pid, sig);
}

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oact)
{
    HILOG_BASE_WARN(LOG_CORE, "%{public}d call sigaction and signo is %{public}d", getpid(), sig);
    if (hookedSigaction == NULL) {
        HILOG_BASE_ERROR(LOG_CORE, "hooked sigaction is NULL?");
        return -1;
    }
    HILOG_BASE_WARN(LOG_CORE, "current signalhandler addr : 0x%{public}x, original signalhandler addr : 0x%{public}x",
        (uintptr_t)act->sa_sigaction, signalHandler);
    return hookedSigaction(sig, act, oact);
}

static void StartHookKillFunction(void)
{
    hookedKill = (KillFunc)dlsym(RTLD_NEXT, "kill");
    if (hookedKill != NULL) {
        return;
    }
    HILOG_BASE_ERROR(LOG_CORE, "Failed to find hooked kill use RTLD_NEXT");

    hookedKill = (KillFunc)dlsym(RTLD_DEFAULT, "kill");
    if (hookedKill != NULL) {
        return;
    }
    HILOG_BASE_ERROR(LOG_CORE, "Failed to find hooked kill use RTLD_DEFAULT");
}

static void StartHookSigactionFunction(void)
{
    hookedSigaction = (SigactionFunc)dlsym(RTLD_NEXT, "sigaction");
    if (hookedSigaction != NULL) {
        return;
    }
    HILOG_BASE_ERROR(LOG_CORE, "Failed to find hooked sigaction use RTLD_NEXT");

    hookedSigaction = (SigactionFunc)dlsym(RTLD_DEFAULT, "sigaction");
    if (hookedSigaction != NULL) {
        return;
    }
    HILOG_BASE_ERROR(LOG_CORE, "Failed to find hooked sigaction use RTLD_DEFAULT");
}

void StartHookFunc(uintptr_t sighdlr)
{
    signalHandler = sighdlr;
    StartHookKillFunction();
    StartHookSigactionFunction();
}
