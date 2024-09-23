/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "pid_utils.h"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dfx_log.h"

namespace OHOS {
namespace HiviewDFX {
namespace {
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xD002D11
#define LOG_TAG "DfxPidUtils"

static constexpr time_t MAX_WAIT_TIME_SECONDS = 30;
static constexpr time_t USLEEP_TIME = 5000;
}

static bool Exited(pid_t pid)
{
    int status;
    pid_t waitPid = waitpid(pid, &status, WNOHANG);
    if (waitPid != pid) {
        return false;
    }

    if (WIFEXITED(status)) {
        LOGERROR("%{public}d died: Process exited with code %{public}d", pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        LOGERROR("%{public}d died: Process exited due to signal %{public}d", pid, WTERMSIG(status));
    } else {
        LOGERROR("%{public}d died: Process finished for unknown reason", pid);
    }
    return true;
}

bool PidUtils::Quiesce(pid_t pid)
{
    siginfo_t si;
    // Wait for up to 10 seconds.
    for (time_t startTime = time(nullptr); time(nullptr) - startTime < 10;) {
        if (ptrace(PTRACE_GETSIGINFO, pid, 0, &si) == 0) {
            return true;
        }
        if (errno != ESRCH) {
            if (errno != EINVAL) {
                LOGERROR("ptrace getsiginfo failed");
                return false;
            }
            // The process is in group-stop state, so try and kick the process out of that state.
            if (ptrace(PTRACE_LISTEN, pid, 0, 0) == -1) {
                // Cannot recover from this, so just pretend it worked and see if we can unwind.
                return true;
            }
        }
        usleep(USLEEP_TIME);
    }
    LOGERROR("%{public}d: Did not quiesce in 10 seconds", pid);
    return false;
}

bool PidUtils::Attach(pid_t pid)
{
    // Wait up to 45 seconds to attach.
    for (time_t startTime = time(nullptr); time(nullptr) - startTime < 45;) {
        if (ptrace(PTRACE_ATTACH, pid, 0, 0) == 0) {
            break;
        }
        if (errno != ESRCH) {
            LOGERROR("Failed to attach");
            return false;
        }
        usleep(USLEEP_TIME);
    }

    if (Quiesce(pid)) {
        return true;
    }

    if (ptrace(PTRACE_DETACH, pid, 0, 0) == -1) {
        LOGERROR("Failed to detach");
    }
    return false;
}

bool PidUtils::Detach(pid_t pid)
{
    if (ptrace(PTRACE_DETACH, pid, 0, 0) == -1) {
        LOGERROR("ptrace detach failed");
        return false;
    }
    return true;
}

bool PidUtils::WaitForPidState(pid_t pid, const std::function<PidRunEnum()>& stateCheckFunc)
{
    PidRunEnum status = PID_RUN_KEEP_GOING;
    for (time_t startTime = time(nullptr);
        time(nullptr) - startTime < MAX_WAIT_TIME_SECONDS && status == PID_RUN_KEEP_GOING;) {
        if (Attach(pid)) {
            status = stateCheckFunc();
            if (status == PID_RUN_PASS) {
                return true;
            }

            if (!Detach(pid)) {
                return false;
            }
        } else if (Exited(pid)) {
            return false;
        }
        usleep(USLEEP_TIME);
    }
    if (status == PID_RUN_KEEP_GOING) {
        LOGERROR("Timed out waiting for pid %{public}d to be ready", pid);
    }
    return status == PID_RUN_PASS;
}

bool PidUtils::WaitForPidStateAfterAttach(pid_t pid, const std::function<PidRunEnum()>& stateCheckFunc)
{
    PidRunEnum status;
    time_t startTime = time(nullptr);
    do {
        status = stateCheckFunc();
        if (status == PID_RUN_PASS) {
            return true;
        }
        if (!Detach(pid)) {
            return false;
        }
        usleep(USLEEP_TIME);
    } while (time(nullptr) - startTime < MAX_WAIT_TIME_SECONDS && status == PID_RUN_KEEP_GOING && Attach(pid));
    if (status == PID_RUN_KEEP_GOING) {
        LOGERROR("Timed out waiting for pid %{public}d to be ready", pid);
    }
    return status == PID_RUN_PASS;
}

}
}